#!/usr/bin/env python3
"""Compile and run the real LVGL compressed-font decode smoke test."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


def section_text(ini_text: str, section: str) -> str:
    match = re.search(
        rf"(?ms)^\[{re.escape(section)}\]\s*\n(.*?)(?=^\[|\Z)", ini_text
    )
    if not match:
        raise RuntimeError(f"missing platformio.ini section [{section}]")
    return match.group(1)


def key_block(text: str, key: str) -> str:
    lines = text.splitlines(keepends=True)
    key_line = re.compile(rf"^{re.escape(key)}[ \t]*=[ \t]*(.*)$")
    for index, line in enumerate(lines):
        match = key_line.fullmatch(line.rstrip("\r\n"))
        if not match:
            continue
        values = [match.group(1) + ("\n" if match.group(1) else "")]
        for continuation in lines[index + 1:]:
            if not continuation.startswith((" ", "\t")):
                break
            values.append(continuation)
        return "".join(values)
    raise RuntimeError(f"missing common_env_data key {key}")


def uncommented_lines(text: str) -> list[str]:
    return [line.split(";", 1)[0].strip() for line in text.splitlines()]


def flag_directives(text: str, name: str) -> list[tuple[str, int | None]]:
    directives = []
    escaped = re.escape(name)
    for line in uncommented_lines(text):
        if not line:
            continue
        define = re.search(rf"(?:^|\s)-D\s*{escaped}(?:(?:\s*=\s*|\s+)([^\s]+))?(?=$|\s)", line)
        if define:
            value = define.group(1)
            directives.append(("define", int(value) if value in {"0", "1"} else None))
        if re.search(rf"(?:^|\s)-U\s*{escaped}\b", line):
            directives.append(("undef", None))
    return directives


def flag_value(text: str, name: str) -> int:
    directives = flag_directives(text, name)
    if any(kind == "undef" for kind, _ in directives):
        raise RuntimeError(f"unsupported -U {name} in active flags")
    values = [value for kind, value in directives if kind == "define"]
    if any(value is None for value in values):
        raise RuntimeError(f"unsupported non-binary -D {name} in active flags")
    if len(set(values)) > 1:
        raise RuntimeError(f"conflicting -D {name} values in active flags")
    return values[0] if values else 0


def active_references(text: str) -> list[str]:
    references = []
    for line in uncommented_lines(text):
        references.extend(re.findall(r"\$\{([^}]+)\}", line))
    return references


def reject_env_overrides(env_text: str, name: str) -> None:
    build_flags = key_block(env_text, "build_flags")
    build_unflags = key_block(env_text, "build_unflags")
    if flag_directives(build_flags, name):
        raise RuntimeError(f"unsupported env override for {name}")
    if re.search(rf"\b{re.escape(name)}\b", "\n".join(uncommented_lines(build_unflags))):
        raise RuntimeError(f"unsupported env unflag for {name}")


def require_env_define(env_text: str, name: str, expected: int) -> None:
    directives = flag_directives(key_block(env_text, "build_flags"), name)
    if not directives or any(kind != "define" or value != expected
                             for kind, value in directives):
        raise RuntimeError(f"env must explicitly define {name}={expected}")
    if re.search(rf"\b-U\s*{re.escape(name)}\b",
                 "\n".join(uncommented_lines(key_block(env_text, "build_unflags")))):
        raise RuntimeError(f"unsupported env unflag for {name}")


def parser_self_tests() -> None:
    commented = "\t; -D LV_USE_FONT_COMPRESSED=1\n"
    assert flag_value(commented, "LV_USE_FONT_COMPRESSED") == 0
    assert active_references(
        "build_flags =\n\t; ${common_env_data.build_flags_lvgl_gateway35}\n"
        "\t${common_env_data.build_flags_lvgl_gateway35}\n"
    ) == ["common_env_data.build_flags_lvgl_gateway35"]
    active = "\t-D LV_USE_FONT_COMPRESSED=1 ; ignored -D LV_USE_FONT_COMPRESSED=0\n"
    assert flag_value(active, "LV_USE_FONT_COMPRESSED") == 1
    env_override = "build_flags =\n\t-D LV_USE_FONT_COMPRESSED=0\nbuild_unflags =\n"
    try:
        reject_env_overrides(env_override, "LV_USE_FONT_COMPRESSED")
    except RuntimeError as error:
        assert "override" in str(error)
    else:
        raise AssertionError("active env override=0 was not rejected")
    env_unflag = "build_flags =\n\nbuild_unflags =\n\t-U LV_USE_FONT_COMPRESSED\n"
    try:
        reject_env_overrides(env_unflag, "LV_USE_FONT_COMPRESSED")
    except RuntimeError as error:
        assert "unflag" in str(error)
    else:
        raise AssertionError("active env unflag was not rejected")


def run(command: list[str], cwd: Path) -> None:
    compact = []
    for argument in command:
        if len(argument) > 80 and ("\\" in argument or "/" in argument):
            compact.append(Path(argument).name)
        else:
            compact.append(argument)
    print("+", " ".join(compact))
    subprocess.run(command, cwd=cwd, check=True)


def compiler(name: str) -> str:
    configured = Path(r"C:\Program Files\mingw64\bin") / f"{name}.exe"
    if configured.is_file():
        return str(configured)
    found = shutil.which(name)
    if found:
        return found
    raise RuntimeError(f"{name} was not found; expected MinGW at C:\\Program Files\\mingw64\\bin")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--negative", action="store_true",
                        help="compile in an isolated directory with LV_USE_FONT_COMPRESSED=0 and require the known NULL failure")
    args = parser.parse_args()
    root = args.root.resolve()
    ini_text = (root / "platformio.ini").read_text(encoding="utf-8")
    parser_self_tests()

    common_env = section_text(ini_text, "common_env_data")
    common_lvgl_flags = key_block(common_env, "build_flags_lvgl")
    gmt154_env = section_text(ini_text, "env:gmt154-06")
    gmt154_build_flags = key_block(gmt154_env, "build_flags")
    compressed = flag_value(gmt154_build_flags, "LV_USE_FONT_COMPRESSED")
    if "LV_USE_FONT_COMPRESSED" in common_lvgl_flags:
        raise RuntimeError("common LVGL flags unexpectedly define LV_USE_FONT_COMPRESSED")
    require_env_define(gmt154_env, "LV_USE_FONT_COMPRESSED", 1)

    try:
        from platformio.project.config import ProjectConfig
    except ImportError as error:
        raise RuntimeError("PlatformIO ProjectConfig is required for expanded flag cross-check") from error
    project_config = ProjectConfig(str(root / "platformio.ini"))
    expanded_gmt154 = project_config.get("env:gmt154-06", "build_flags") or []
    unflags_gmt154 = project_config.get("env:gmt154-06", "build_unflags") or []
    if flag_value("\n".join(map(str, expanded_gmt154)), "LV_USE_FONT_COMPRESSED") != 1:
        raise RuntimeError("expanded GMT154 build_flags do not enable LV_USE_FONT_COMPRESSED=1")
    if flag_directives("\n".join(map(str, unflags_gmt154)), "LV_USE_FONT_COMPRESSED"):
        raise RuntimeError("expanded GMT154 build_unflags unexpectedly alter LV_USE_FONT_COMPRESSED")

    if args.negative:
        compressed = 0

    lvgl = root / ".pio" / "libdeps" / "gmt154-06" / "lvgl"
    if not (lvgl / "src" / "font" / "lv_font_fmt_txt.c").is_file():
        raise RuntimeError(f"LVGL 9.4 source not found: {lvgl}")

    output = args.output or (root / "artifacts" / "font-smoke" / ("negative" if args.negative else "green"))
    output.mkdir(parents=True, exist_ok=True)
    exe = output / "lvgl_font_decode_smoke.exe"
    c_object = output / "lv_font_fmt_txt.o"
    font_runtime_sources = []
    defines = [
        f"-DLV_USE_FONT_COMPRESSED={compressed}",
        "-DLV_CONF_SKIP=1",
        "-DGATEWAY_35=1",
        "-DGMT154_06=1",
    ]
    include_dirs = [f"-I{lvgl}", f"-I{lvgl / 'src'}"]

    gcc = compiler("gcc")
    gxx = compiler("g++")
    run(
        [gcc, "-std=c11", "-O2", "-ffunction-sections", *defines, *include_dirs,
         "-c", str(lvgl / "src" / "font" / "lv_font_fmt_txt.c"), "-o", str(c_object)],
        root,
    )

    runtime_objects = []
    for runtime_source in font_runtime_sources:
        runtime_object = output / (runtime_source.stem + ".o")
        run(
            [gcc, "-std=c11", "-O2", "-ffunction-sections", *defines, *include_dirs,
             "-c", str(runtime_source), "-o", str(runtime_object)],
            root,
        )
        runtime_objects.append(runtime_object)

    font_sources = [
        root / "src" / "lv_font_montserrat_regular_12.c",
        root / "src" / "lv_font_montserrat_regular_14.c",
        root / "src" / "lv_font_montserrat_regular_16.c",
        root / "src" / "lv_font_montserrat_regular_20.c",
        root / "src" / "lv_font_montserrat_54_digits.c",
        root / "src" / "lv_font_montserrat_80_digits.c",
    ]
    font_objects = []
    for font_source in font_sources:
        font_object = output / (font_source.stem + ".o")
        run(
            [gcc, "-std=c11", "-O2", "-ffunction-sections", *defines, *include_dirs,
             "-c", str(font_source), "-o", str(font_object)],
            root,
        )
        font_objects.append(font_object)

    run(
        [gxx, "-std=c++17", "-O2", "-ffunction-sections", *defines, *include_dirs,
         str(root / "test" / "font-smoke" / "lvgl_font_decode_smoke.cpp"),
         *map(str, font_objects), str(c_object), *map(str, runtime_objects),
         "-Wl,--gc-sections", "-o", str(exe)],
        root,
    )
    if args.negative:
        result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True)
        output_text = (result.stdout + result.stderr).strip()
        expected = re.compile(r"^montserrat-12: first decode empty U\+0041 \(result=0+\)$")
        if result.returncode != 1 or not any(expected.fullmatch(line) for line in output_text.splitlines()):
            print(output_text, file=sys.stderr)
            raise RuntimeError("disabled-font negative case did not produce the expected bitmap NULL failure")
        print("PASS expected disabled-font failure: bitmap NULL for U+0041, exit=1")
    else:
        run([str(exe)], root)
        print(f"LV_USE_FONT_COMPRESSED from [env:gmt154-06]: {compressed}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from error
    except RuntimeError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(2) from error
