"""Apply C3-only TFT_eSPI compatibility and startup diagnostic patches."""

from __future__ import annotations

import os
import re
import tempfile
from pathlib import Path


TARGET_ENV = "gmt154-06-c3"
TARGET_ENVS = {
    "gmt154-06-c3-ble-profile-act3",
    "gmt154-06-c3-ble-profile-act1",
    TARGET_ENV,
    "gmt154-06-c3-mdns-off",
    "gmt154-06-c3-mdns-off-ble-init-off",
    "gmt154-06-c3-mdns-off-ble-stage-diag",
}
PROCESSORS_RELATIVE_PATH = Path("TFT_eSPI") / "Processors"
HEADER_NAME = "TFT_eSPI_ESP32_C3.h"
SOURCE_NAME = "TFT_eSPI_ESP32_C3.c"
LIBRARY_SOURCE_NAME = "TFT_eSPI.cpp"
SPI_PORT_DEFINE = re.compile(rb"(?m)^[ \t]*#[ \t]*define[ \t]+SPI_PORT\b[^\r\n]*\r?$")
SPI_PORT_LINE = re.compile(
    rb"(?m)^[ \t]*#[ \t]*define[ \t]+SPI_PORT[ \t]+SPI2_HOST[ \t]*\r?$")
SPI_REG_PORT_DEFINE = re.compile(
    rb"(?m)^[ \t]*#[ \t]*define[ \t]+SPI_REG_PORT\b[^\r\n]*\r?$")
SPI_REG_PORT_LINE = re.compile(
    rb"(?m)^[ \t]*#[ \t]*define[ \t]+SPI_REG_PORT[ \t]+2[ \t]*\r?$")
SPI_MOSI_DLEN_DEFINE = re.compile(
    rb"(?m)^[ \t]*#[ \t]*define[ \t]+SPI_MOSI_DLEN_REG\b[^\r\n]*\r?$")
SPI_MOSI_DLEN_ALIAS = re.compile(
    rb"(?m)^[ \t]*#[ \t]*define[ \t]+SPI_MOSI_DLEN_REG[ \t]*\([ \t]*x[ \t]*\)"
    rb"[ \t]+SPI_MS_DLEN_REG[ \t]*\([ \t]*x[ \t]*\)[ \t]*\r?$")
SPI_REG_CALL = re.compile(
    rb"\b(SPI_[A-Za-z0-9_]+_REG)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
SPI_PORT_ARGUMENT = re.compile(
    rb"\b(SPI_[A-Za-z0-9_]+_REG\s*\(\s*)SPI_PORT(\s*\))")
SPI_REG_ARGUMENT = re.compile(
    rb"\b(SPI_[A-Za-z0-9_]+_REG\s*\(\s*)SPI_REG_PORT(\s*\))")
MISO_ALIAS_BLOCK = re.compile(
    rb"(?m)^[ \t]*#if defined\(CONFIG_IDF_TARGET_ESP32C3\) \|\| "
    rb"defined\(CONFIG_IDF_TARGET_ESP32S2\)\r?\n"
    rb"[ \t]*#if \(TFT_MISO == -1\)\r?\n"
    rb"[ \t]*#undef TFT_MISO\r?\n"
    rb"[ \t]*#define TFT_MISO TFT_MOSI\r?\n"
    rb"[ \t]*#endif\r?\n"
    rb"[ \t]*#endif[ \t]*\r?$"
)
MISO_ALIAS_S2_BLOCK = re.compile(
    rb"(?m)^[ \t]*#if defined\(CONFIG_IDF_TARGET_ESP32S2\)\r?\n"
    rb"[ \t]*#if \(TFT_MISO == -1\)\r?\n"
    rb"[ \t]*#undef TFT_MISO\r?\n"
    rb"[ \t]*#define TFT_MISO TFT_MOSI\r?\n"
    rb"[ \t]*#endif\r?\n"
    rb"[ \t]*#endif[ \t]*\r?$"
)
MISO_ALIAS_DEFINE = b"#define TFT_MISO TFT_MOSI"
EXPECTED_HEADER_CALLS = 8
EXPECTED_SOURCE_CALLS = 114
INIT_TRACE_MARKERS = (
    b"GMT154_C3_TFT: initBus.begin",
    b"GMT154_C3_TFT: initBus.done",
    b"GMT154_C3_TFT: spi.begin.start",
    b"GMT154_C3_TFT: spi.begin.done",
    b"GMT154_C3_TFT: reset.begin",
    b"GMT154_C3_TFT: reset.done",
    b"GMT154_C3_TFT: st7789_2.begin",
    b"GMT154_C3_TFT: st7789_2.done",
)
RESET_TRACE_MARKERS = (
    b"GMT154_C3_TFT: reset.pinMode.begin",
    b"GMT154_C3_TFT: reset.pinMode.done",
    b"GMT154_C3_TFT: reset.nop.begin",
    b"GMT154_C3_TFT: reset.writecommand.enter",
    b"GMT154_C3_TFT: reset.begin_tft_write.done",
    b"GMT154_C3_TFT: reset.tft_Write_8.done",
    b"GMT154_C3_TFT: reset.end_tft_write.done",
    b"GMT154_C3_TFT: reset.nop.done",
    b"GMT154_C3_TFT: reset.rst.high1.done",
    b"GMT154_C3_TFT: reset.delay5.done",
    b"GMT154_C3_TFT: reset.rst.low.done",
    b"GMT154_C3_TFT: reset.delay20.done",
    b"GMT154_C3_TFT: reset.rst.high2.done",
    b"GMT154_C3_TFT: reset.delay150.done",
)


class PatchError(RuntimeError):
    """TFT_eSPI's C3 processor sources are not in a known patchable state."""


def patch_init_stage_trace(source: bytes) -> bytes:
    """Add target-only serial markers around known TFT initialization stages."""
    marker_counts = {marker: source.count(marker + b'"') for marker in INIT_TRACE_MARKERS}
    marker_count = sum(marker_counts.values())
    if all(count == 1 for count in marker_counts.values()):
        return source
    missing_spi_entry = b"GMT154_C3_TFT: spi.begin.start"
    if marker_counts[missing_spi_entry] == 0 and all(
        count == 1 for marker, count in marker_counts.items() if marker != missing_spi_entry
    ):
        if marker_count != len(INIT_TRACE_MARKERS) - 1:
            raise PatchError("partial or unknown C3 TFT initialization trace patch")
        pattern = re.compile(
            rb"(?m)^([ \t]*)(spi\.begin\(TFT_SCLK, TFT_MISO, TFT_MOSI, -1\);[^\r\n]*)(\r?\n)"
        )
        found = list(pattern.finditer(source))
        if len(found) != 1:
            raise PatchError("unknown or ambiguous C3 SPI begin source anchor")
        match = found[0]
        ending = match.group(3)
        statement = match.group(1) + match.group(2)
        replacement = (
            match.group(1)
            + b'Serial.println("GMT154_C3_TFT: spi.begin.start");'
            + ending
            + statement
            + ending
        )
        return source[:match.start()] + replacement + source[match.end():]
    if marker_count:
        raise PatchError("partial or unknown C3 TFT initialization trace patch")

    line_ending = _line_ending(source)
    anchors = (
        (re.compile(rb"(?m)^([ \t]*)initBus\(\);[ \t]*(\r?\n)"),
         b"initBus.begin", b"initBus.done"),
        (re.compile(rb"(?m)^([ \t]*)spi\.begin\(TFT_SCLK, TFT_MISO, TFT_MOSI, -1\);[^\r\n]*(\r?\n)"),
         b"spi.begin.start", b"spi.begin.done"),
        (re.compile(rb"(?m)^([ \t]*)// Toggle RST low to reset[ \t]*(\r?\n)"),
         b"reset.begin", None),
        (re.compile(rb"(?m)^([ \t]*)delay\(150\); // Wait for reset to complete[ \t]*(\r?\n)"),
         None, b"reset.done"),
        (re.compile(rb'(?m)^([ \t]*)#include "TFT_Drivers/ST7789_2_Init\.h"[ \t]*(\r?\n)'),
         b"st7789_2.begin", b"st7789_2.done"),
    )
    matches = []
    for pattern, before, after in anchors:
        found = list(pattern.finditer(source))
        if len(found) != 1:
            raise PatchError("unknown or ambiguous C3 TFT initialization source anchor")
        matches.append((found[0], before, after))

    updated = source
    for match, before, after in reversed(matches):
        indent = match.group(1)
        ending = match.group(2) if match.lastindex and match.lastindex >= 2 else _line_ending(source)
        rendered = []
        if before:
            rendered.append(indent + b'Serial.println("GMT154_C3_TFT: ' + before + b'");')
        rendered.append(match.group(0)[:-len(ending)])
        if after:
            rendered.append(indent + b'Serial.println("GMT154_C3_TFT: ' + after + b'");')
        replacement = ending.join(rendered) + ending
        updated = updated[:match.start()] + replacement + updated[match.end():]
    return updated


def _mark_lines(
    source: bytes,
    pattern: re.Pattern[bytes],
    *,
    before: tuple[bytes, ...] = (),
    after: tuple[bytes, ...] = (),
    conditional_c_zero: bool = False,
) -> bytes:
    matches = list(pattern.finditer(source))
    expected = max(len(before), len(after), 1)
    if len(matches) != expected:
        raise PatchError("unknown or ambiguous C3 TFT reset source anchor")
    updated = source
    for index in reversed(range(expected)):
        match = matches[index]
        indent, statement, ending = match.group(1), match.group(2), match.group(3)
        lines = []
        if index < len(before):
            lines.append(_trace_line(indent, before[index], conditional_c_zero))
        lines.append(indent + statement)
        if index < len(after):
            lines.append(_trace_line(indent, after[index], conditional_c_zero))
        replacement = ending.join(lines) + ending
        updated = updated[:match.start()] + replacement + updated[match.end():]
    return updated


def _trace_line(indent: bytes, marker: bytes, conditional_c_zero: bool = False) -> bytes:
    line = indent + b'Serial.println("GMT154_C3_TFT: ' + marker + b'");'
    if conditional_c_zero:
        return indent + b'if (c == 0x00) ' + line[len(indent):]
    return line


def patch_reset_trace(source: bytes) -> bytes:
    """Split the first reset command and GPIO sequence into diagnostic stages."""
    counts = {marker: source.count(marker + b'"') for marker in RESET_TRACE_MARKERS}
    if all(count == 1 for count in counts.values()):
        return source
    if any(count for count in counts.values()):
        raise PatchError("partial or unknown C3 TFT reset trace patch")
    if any(source.count(marker) != 1 for marker in INIT_TRACE_MARKERS):
        raise PatchError("C3 TFT stage trace must be installed before reset trace")

    block_pattern = re.compile(
        rb"(?ms)(^[ \t]*Serial\.println\(\"GMT154_C3_TFT: reset\.begin\"\);\r?\n)"
        rb"(.*?)"
        rb"(^[ \t]*Serial\.println\(\"GMT154_C3_TFT: reset\.done\"\);\r?\n)"
    )
    blocks = list(block_pattern.finditer(source))
    if len(blocks) != 1:
        raise PatchError("unknown or ambiguous C3 TFT reset block")
    block = blocks[0]
    reset = block.group(2)
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(pinMode\(TFT_RST, OUTPUT\);)(\r?\n)"),
        before=(b"reset.pinMode.begin",),
        after=(b"reset.pinMode.done",),
    )
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(writecommand\(0x00\);[^\r\n]*)(\r?\n)"),
        before=(b"reset.nop.begin",),
        after=(b"reset.nop.done",),
    )
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(digitalWrite\(TFT_RST, HIGH\);)(\r?\n)"),
        after=(b"reset.rst.high1.done", b"reset.rst.high2.done"),
    )
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(delay\(5\);)(\r?\n)"),
        after=(b"reset.delay5.done",),
    )
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(digitalWrite\(TFT_RST, LOW\);)(\r?\n)"),
        after=(b"reset.rst.low.done",),
    )
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(delay\(20\);)(\r?\n)"),
        after=(b"reset.delay20.done",),
    )
    reset = _mark_lines(
        reset,
        re.compile(rb"(?m)^([ \t]*)(delay\(150\); // Wait for reset to complete)(\r?\n)"),
        after=(b"reset.delay150.done",),
    )

    writecommand_pattern = re.compile(
        rb"(?ms)(^[ \t]*void TFT_eSPI::writecommand\(uint8_t c\)\r?\n\{\r?\n)"
        rb"(.*?)"
        rb"(^\}\r?\n)"
    )
    methods = list(writecommand_pattern.finditer(source))
    if len(methods) != 1:
        raise PatchError("unknown or ambiguous C3 writecommand(uint8_t) method")
    method = methods[0]
    body = method.group(2)
    body = _mark_lines(
        body,
        re.compile(rb"(?m)^([ \t]*)(begin_tft_write\(\);)(\r?\n)"),
        before=(b"reset.writecommand.enter",),
        after=(b"reset.begin_tft_write.done",),
        conditional_c_zero=True,
    )
    body = _mark_lines(
        body,
        re.compile(rb"(?m)^([ \t]*)(tft_Write_8\(c\);)(\r?\n)"),
        after=(b"reset.tft_Write_8.done",),
        conditional_c_zero=True,
    )
    body = _mark_lines(
        body,
        re.compile(rb"(?m)^([ \t]*)(end_tft_write\(\);)(\r?\n)"),
        after=(b"reset.end_tft_write.done",),
        conditional_c_zero=True,
    )
    updated_method = method.group(1) + body + method.group(3)
    updated_source = source[:method.start()] + updated_method + source[method.end():]
    start = block.start(2)
    end = block.end(2)
    reset_start = start
    reset_end = end
    return updated_source[:reset_start] + reset + updated_source[reset_end:]


def patch_init_trace(source: bytes) -> bytes:
    return patch_reset_trace(patch_init_stage_trace(source))


def patch_library_source(library_dir: Path) -> str:
    source_path = Path(library_dir) / LIBRARY_SOURCE_NAME
    source = _read(source_path)
    updated = patch_init_trace(source)
    if updated == source:
        return "already"
    staged = _atomic_replace(source_path, updated)
    try:
        os.replace(staged, source_path)
    finally:
        staged.unlink(missing_ok=True)
    return "changed"


def _read(path: Path) -> bytes:
    try:
        content = path.read_bytes()
    except OSError as exc:
        raise PatchError(f"required TFT_eSPI C3 source file is missing: {path}") from exc
    if b"\x00" in content:
        raise PatchError(f"unexpected binary data in TFT_eSPI processor file: {path}")
    return content


def _line_ending(content: bytes) -> bytes:
    return b"\r\n" if b"\r\n" in content else b"\n"


def _insert_register_port(content: bytes) -> tuple[bytes, int]:
    lines = content.splitlines(keepends=True)
    inserted = 0
    for index, line in enumerate(lines):
        body = line.rstrip(b"\r\n")
        if body.strip() != b"#define SPI_PORT SPI2_HOST":
            continue
        ending = line[len(body):] or _line_ending(content)
        indentation = body[: len(body) - len(body.lstrip())]
        lines[index] = line + indentation + b"#define SPI_REG_PORT 2" + ending
        inserted += 1
    return b"".join(lines), inserted


def _atomic_replace(path: Path, content: bytes) -> Path:
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        return temporary
    except Exception:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise


def _validate_layout(header: bytes, source: bytes, *, patched: bool) -> None:
    host_defs = SPI_PORT_DEFINE.findall(header)
    host_lines = SPI_PORT_LINE.findall(header)
    register_defs = SPI_REG_PORT_DEFINE.findall(header)
    register_lines = SPI_REG_PORT_LINE.findall(header)
    alias_defs = SPI_MOSI_DLEN_DEFINE.findall(header)
    aliases = list(SPI_MOSI_DLEN_ALIAS.finditer(header))
    if len(host_defs) != 1 or len(host_lines) != 1:
        raise PatchError("unknown or ambiguous C3 SPI_PORT definition")
    if len(alias_defs) != 1 or len(aliases) != 1:
        raise PatchError("unknown or ambiguous C3 SPI_MOSI_DLEN_REG alias")
    if len(register_defs) != int(patched) or len(register_lines) != int(patched):
        raise PatchError("unknown or ambiguous C3 SPI_REG_PORT definition")

    alias = aliases[0]
    header_without_alias = header[:alias.start()] + header[alias.end():]
    header_calls = SPI_REG_CALL.findall(header_without_alias)
    source_calls = SPI_REG_CALL.findall(source)
    if len(header_calls) != EXPECTED_HEADER_CALLS:
        raise PatchError(
            f"unexpected C3 header SPI register call count: {len(header_calls)}"
        )
    if len(source_calls) != EXPECTED_SOURCE_CALLS:
        raise PatchError(
            f"unexpected C3 source SPI register call count: {len(source_calls)}"
        )

    expected_argument = b"SPI_REG_PORT" if patched else b"SPI_PORT"
    if any(argument != expected_argument for _, argument in header_calls + source_calls):
        raise PatchError("unknown or partial C3 SPI register macro arguments")


def _patch_c3_no_miso_alias(header: bytes) -> tuple[bytes, bool]:
    """Keep the no-MISO fallback for S2, but leave C3 MISO disabled."""
    alias_definitions = header.count(MISO_ALIAS_DEFINE)
    legacy_blocks = list(MISO_ALIAS_BLOCK.finditer(header))
    s2_blocks = list(MISO_ALIAS_S2_BLOCK.finditer(header))

    if alias_definitions != 1:
        raise PatchError("unknown or ambiguous C3 TFT_MISO fallback")
    if len(legacy_blocks) == 1 and not s2_blocks:
        old = b"#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)"
        new = b"#if defined(CONFIG_IDF_TARGET_ESP32S2)"
        start, end = legacy_blocks[0].span()
        block = header[start:end]
        if block.count(old) != 1:
            raise PatchError("failed to remove C3 TFT_MISO fallback")
        patched_block = block.replace(old, new, 1)
        updated = header[:start] + patched_block + header[end:]
        if (
            len(list(MISO_ALIAS_BLOCK.finditer(updated))) != 0
            or len(list(MISO_ALIAS_S2_BLOCK.finditer(updated))) != 1
            or updated.count(MISO_ALIAS_DEFINE) != 1
        ):
            raise PatchError("C3 TFT_MISO fallback patch produced an unexpected layout")
        return updated, True
    if not legacy_blocks and len(s2_blocks) == 1:
        return header, False
    raise PatchError("partial, duplicate, or unknown C3 TFT_MISO fallback")


def patch_processors(processors_dir: Path) -> str:
    """Patch known C3 processor sources, leaving the SPI host enum untouched."""
    processors_dir = Path(processors_dir)
    header_path = processors_dir / HEADER_NAME
    source_path = processors_dir / SOURCE_NAME
    header = _read(header_path)
    source = _read(source_path)

    register_defs = SPI_REG_PORT_DEFINE.findall(header)
    if len(register_defs) > 1:
        raise PatchError("partial or unknown C3 SPI register-index patch; refusing to edit")
    if register_defs:
        _validate_layout(header, source, patched=True)
        updated_header, miso_changed = _patch_c3_no_miso_alias(header)
        if not miso_changed:
            return "already"
        updated_source = source
    else:
        _validate_layout(header, source, patched=False)
        updated_header, inserted = _insert_register_port(header)
        if inserted != 1:
            raise PatchError("failed to add C3 SPI register index next to SPI2_HOST")

        replace_argument = lambda match: match.group(1) + b"SPI_REG_PORT" + match.group(2)
        updated_header, header_replacements = SPI_PORT_ARGUMENT.subn(
            replace_argument, updated_header
        )
        updated_source, source_replacements = SPI_PORT_ARGUMENT.subn(
            replace_argument, source
        )
        if header_replacements == 0 or source_replacements == 0:
            raise PatchError("expected C3 SPI register arguments were not found in both files")
        _validate_layout(updated_header, updated_source, patched=True)
        updated_header, _ = _patch_c3_no_miso_alias(updated_header)

    staged: list[tuple[Path, Path, Path]] = []
    temporary_paths: list[Path] = []
    replaced: list[tuple[Path, Path]] = []
    preserved_backups: set[Path] = set()
    try:
        for target, updated, original in (
            (header_path, updated_header, header),
            (source_path, updated_source, source),
        ):
            updated_temp = _atomic_replace(target, updated)
            temporary_paths.append(updated_temp)
            backup_temp = _atomic_replace(target, original)
            temporary_paths.append(backup_temp)
            staged.append((target, updated_temp, backup_temp))
        for target, updated_temp, backup_temp in staged:
            os.replace(updated_temp, target)
            replaced.append((target, backup_temp))
    except Exception as exc:
        rollback_errors = []
        for target, backup_temp in reversed(replaced):
            try:
                os.replace(backup_temp, target)
            except OSError as rollback_error:
                preserved_backups.add(backup_temp)
                rollback_errors.append(rollback_error)
        if rollback_errors:
            paths = ", ".join(str(path) for path in sorted(preserved_backups))
            raise PatchError(
                "SPI patch failed and rollback could not restore all files; "
                f"recovery backup(s) preserved at: {paths}"
            ) from exc
        raise
    finally:
        for temporary in temporary_paths:
            if temporary not in preserved_backups:
                temporary.unlink(missing_ok=True)
    return "changed"


def apply_patch_for_env(pioenv: str, libdeps_root: Path) -> str:
    """Apply only to an explicitly supported C3 environment's libdeps checkout."""
    if pioenv not in TARGET_ENVS:
        return "skipped"
    environment_dir = Path(libdeps_root) / pioenv
    processor_result = patch_processors(environment_dir / PROCESSORS_RELATIVE_PATH)
    trace_result = patch_library_source(environment_dir / "TFT_eSPI")
    return "changed" if "changed" in (processor_result, trace_result) else "already"


def _register_platformio_hook() -> None:
    try:
        Import("env")  # type: ignore[name-defined]
    except NameError:
        return

    pioenv = env.get("PIOENV")  # type: ignore[name-defined]
    if pioenv not in TARGET_ENVS:
        return
    libdeps_root = Path(env.subst("$PROJECT_LIBDEPS_DIR"))  # type: ignore[name-defined]
    result = apply_patch_for_env(pioenv, libdeps_root)
    print(
        "GMT154_C3_TFT_ESPI_REGISTER_PATCH: "
        f"result={result} env={pioenv} path={libdeps_root / pioenv / PROCESSORS_RELATIVE_PATH}"
    )


_register_platformio_hook()
