"""Patch TFT_eSPI's ESP32-S3 no-MISO fallback for GMT154-06 only."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Any


TARGET_ENVS = frozenset({
    "gmt154-06",
    "gmt154-06-c2b-diag",
    "gmt154-06-c2c-diag",
    "gmt154-06-sync-wifi-scan-diag",
    "gmt154-06-sync-wifi-scan-diag-ble-init-off",
})
HEADER_RELATIVE_PATH = Path("TFT_eSPI") / "Processors" / "TFT_eSPI_ESP32_S3.h"
OLD_LINE = b"#if (TFT_MISO == -1)"
NEW_LINE = b"#if (TFT_MISO == -1) && !defined(GMT154_06)"


class PatchError(RuntimeError):
    """Raised when the dependency header is not the expected known version."""


def _line_body_and_ending(line: bytes) -> tuple[bytes, bytes]:
    if line.endswith(b"\r\n"):
        return line[:-2], b"\r\n"
    if line.endswith(b"\n"):
        return line[:-1], b"\n"
    return line, b""


def _matching_line_indices(lines: list[bytes], expected: bytes) -> list[int]:
    return [
        index
        for index, line in enumerate(lines)
        if _line_body_and_ending(line)[0].strip() == expected
    ]


def _atomic_write(path: Path, content: bytes) -> None:
    temporary_path: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary_path = temporary.name
            temporary.write(content)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            try:
                os.unlink(temporary_path)
            except FileNotFoundError:
                pass


def patch_header(path: Path) -> str:
    """Convert the one known header anchor, or return ``already``."""

    try:
        original = path.read_bytes()
    except OSError as exc:
        raise PatchError(f"cannot read TFT_eSPI header: {path}") from exc

    lines = original.splitlines(keepends=True)
    old_indices = _matching_line_indices(lines, OLD_LINE)
    new_indices = _matching_line_indices(lines, NEW_LINE)

    if len(new_indices) == 1 and not old_indices:
        return "already"
    if len(old_indices) != 1 or new_indices:
        raise PatchError(
            "unexpected TFT_eSPI header version or anchor count "
            f"(old={len(old_indices)}, new={len(new_indices)}): {path}"
        )

    index = old_indices[0]
    line_body, line_ending = _line_body_and_ending(lines[index])
    indentation = line_body[: len(line_body) - len(line_body.lstrip())]
    lines[index] = indentation + NEW_LINE + line_ending
    _atomic_write(path, b"".join(lines))
    return "changed"


def apply_patch_if_gmt154(pioenv: str, path: Path) -> str:
    """Patch only the GMT154 production and diagnostic environments."""

    if pioenv not in TARGET_ENVS:
        return "skipped"
    return patch_header(Path(path))


def _is_tft_espi_source(node: Any) -> bool:
    source_path = Path(node.srcnode().get_path())
    return any(part.casefold() == "tft_espi" for part in source_path.parts)


def _register_platformio_hook() -> None:
    try:
        Import("env")  # type: ignore[name-defined]
    except NameError:
        return

    pioenv = env.get("PIOENV")  # type: ignore[name-defined]
    if pioenv not in TARGET_ENVS:
        return

    header_path = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))  # type: ignore[name-defined]
        / pioenv
        / HEADER_RELATIVE_PATH
    )
    state = {"ran": False}

    def patch_before_tft_espi_compile(pio_env: Any, node: Any) -> Any:
        if not state["ran"] and _is_tft_espi_source(node):
            result = apply_patch_if_gmt154(pioenv, header_path)
            print(
                "GMT154_TFT_ESPI_PATCH: "
                f"result={result} env={pioenv} path={header_path}"
            )
            state["ran"] = True
        return node

    env.AddBuildMiddleware(patch_before_tft_espi_compile)  # type: ignore[name-defined]


_register_platformio_hook()
