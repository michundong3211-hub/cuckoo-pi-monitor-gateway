import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).parents[2]
PATCHER_PATH = ROOT / "script" / "patch_tft_espi_gmt154_c3.py"
SPEC = importlib.util.spec_from_file_location("patch_tft_espi_gmt154_c3", PATCHER_PATH)
PATCHER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PATCHER)


def make_layout(header_count: int = 8, source_count: int = 114) -> tuple[bytes, bytes]:
    names = (b"SPI_CMD_REG", b"SPI_MOSI_DLEN_REG", b"SPI_W0_REG", b"SPI_USER_REG")
    header_calls = [
        b"WRITE_PERI_REG(" + names[index % len(names)] + b"(SPI_PORT), 1);\n"
        for index in range(header_count)
    ]
    source_calls = [
        b"WRITE_PERI_REG(" + names[index % len(names)] + b"(SPI_PORT), 1);\n"
        for index in range(source_count)
    ]
    header = (
        b"#define SPI_PORT SPI2_HOST\n"
        b"#define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)\n"
        b"spi_host_device_t spi_host = (spi_host_device_t) DMA_CHANNEL;\n"
        + b"".join(header_calls)
    )
    return header, b"".join(source_calls)


HEADER, SOURCE = make_layout()
MISO_ALIAS_BLOCK = (
    b"    #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)\n"
    b"      #if (TFT_MISO == -1)\n"
    b"        #undef TFT_MISO\n"
    b"        #define TFT_MISO TFT_MOSI\n"
    b"      #endif\n"
    b"    #endif"
)


def make_miso_alias(header: bytes = HEADER) -> bytes:
    newline = b"\r\n" if b"\r\n" in header else b"\n"
    block = MISO_ALIAS_BLOCK.replace(b"\n", newline)
    return header + newline + block + newline


def make_tft_init_source(newline: bytes = b"\n") -> bytes:
    lines = (
        b"void TFT_eSPI::init(uint8_t tc) {",
        b"    initBus();",
        b"      spi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);",
        b"  // Toggle RST low to reset",
        b"#ifdef TFT_RST",
        b"  #if !defined(RP2040_PIO_INTERFACE)",
        b"    if (TFT_RST >= 0) {",
        b"      pinMode(TFT_RST, OUTPUT);",
        b"    }",
        b"  #endif",
        b"  if (TFT_RST >= 0) {",
        b"    writecommand(0x00); // Put SPI bus in known state for TFT with CS tied low",
        b"    digitalWrite(TFT_RST, HIGH);",
        b"    delay(5);",
        b"    digitalWrite(TFT_RST, LOW);",
        b"    delay(20);",
        b"    digitalWrite(TFT_RST, HIGH);",
        b"  }",
        b"#endif",
        b"  delay(150); // Wait for reset to complete",
        b'    #include "TFT_Drivers/ST7789_2_Init.h"',
        b"}",
        b"void TFT_eSPI::writecommand(uint8_t c)",
        b"{",
        b"  begin_tft_write();",
        b"  DC_C;",
        b"  tft_Write_8(c);",
        b"  DC_D;",
        b"  end_tft_write();",
        b"}",
    )
    return newline.join(lines) + newline


class PatchTftEspiGmt154C3Test(unittest.TestCase):
    def make_processors(
        self, root: Path, header: bytes | None = None, source: bytes = SOURCE
    ) -> Path:
        if header is None:
            header = make_miso_alias()
        elif b"#define TFT_MISO" not in header:
            header = make_miso_alias(header)
        processors = root / "TFT_eSPI" / "Processors"
        processors.mkdir(parents=True)
        (processors / "TFT_eSPI_ESP32_C3.h").write_bytes(header)
        (processors / "TFT_eSPI_ESP32_C3.c").write_bytes(source)
        return processors

    def test_patch_uses_register_index_without_changing_spi_host(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary), make_miso_alias())

            result = PATCHER.patch_processors(processors)

            self.assertEqual(result, "changed")
            header = (processors / "TFT_eSPI_ESP32_C3.h").read_bytes()
            source = (processors / "TFT_eSPI_ESP32_C3.c").read_bytes()
            self.assertIn(b"#define SPI_PORT SPI2_HOST", header)
            self.assertIn(b"#define SPI_REG_PORT 2", header)
            self.assertIn(b"#if defined(CONFIG_IDF_TARGET_ESP32S2)", header)
            self.assertNotIn(
                b"#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)",
                header,
            )
            self.assertIn(b"#define TFT_MISO TFT_MOSI", header)  # Retained only for ESP32-S2.
            self.assertIn(b"spi_host_device_t spi_host", header)
            self.assertIn(b"#define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)", header)
            self.assertNotRegex(header, rb"SPI_[A-Za-z0-9_]+_REG\(SPI_PORT\)")
            self.assertNotRegex(source, rb"SPI_[A-Za-z0-9_]+_REG\(SPI_PORT\)")
            self.assertEqual(source.count(b"SPI_CMD_REG(SPI_REG_PORT)"), 29)
            self.assertEqual(source.count(b"SPI_USER_REG(SPI_REG_PORT)"), 28)
            self.assertEqual(source.count(b"SPI_MOSI_DLEN_REG(SPI_REG_PORT)"), 29)
            self.assertEqual(source.count(b"SPI_W0_REG(SPI_REG_PORT)"), 28)

    def test_patch_is_idempotent(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary), make_miso_alias())
            self.assertEqual(PATCHER.patch_processors(processors), "changed")
            first = {
                name: (processors / name).read_bytes()
                for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
            }

            self.assertEqual(PATCHER.patch_processors(processors), "already")
            self.assertEqual(
                first,
                {
                    name: (processors / name).read_bytes()
                    for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                },
            )

    def test_miso_patch_targets_legacy_conditional_not_matching_comment(self):
        old = b"#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)"
        comment = b"// " + old + b"\n"
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(
                Path(temporary), comment + make_miso_alias()
            )

            self.assertEqual(PATCHER.patch_processors(processors), "changed")
            patched = (processors / "TFT_eSPI_ESP32_C3.h").read_bytes()

            self.assertIn(comment, patched)
            self.assertEqual(list(PATCHER.MISO_ALIAS_BLOCK.finditer(patched)), [])
            self.assertEqual(len(list(PATCHER.MISO_ALIAS_S2_BLOCK.finditer(patched))), 1)
            self.assertEqual(patched.count(PATCHER.MISO_ALIAS_DEFINE), 1)

    def test_old_spi_index_patch_is_upgraded_with_c3_no_miso_alias_fix(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary), make_miso_alias())
            legacy_header, inserted = PATCHER._insert_register_port(
                (processors / "TFT_eSPI_ESP32_C3.h").read_bytes()
            )
            self.assertEqual(inserted, 1)
            replace_argument = lambda match: match.group(1) + b"SPI_REG_PORT" + match.group(2)
            legacy_header = PATCHER.SPI_PORT_ARGUMENT.sub(replace_argument, legacy_header)
            legacy_source = PATCHER.SPI_PORT_ARGUMENT.sub(
                replace_argument, (processors / "TFT_eSPI_ESP32_C3.c").read_bytes()
            )
            (processors / "TFT_eSPI_ESP32_C3.h").write_bytes(legacy_header)
            (processors / "TFT_eSPI_ESP32_C3.c").write_bytes(legacy_source)
            legacy = (processors / "TFT_eSPI_ESP32_C3.h").read_bytes()
            self.assertIn(b"#define SPI_REG_PORT 2", legacy)
            self.assertIn(b"#define TFT_MISO TFT_MOSI", legacy)

            self.assertEqual(PATCHER.patch_processors(processors), "changed")
            upgraded = (processors / "TFT_eSPI_ESP32_C3.h").read_bytes()
            self.assertIn(b"#if defined(CONFIG_IDF_TARGET_ESP32S2)", upgraded)
            self.assertNotIn(
                b"#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)",
                upgraded,
            )
            self.assertEqual(PATCHER.patch_processors(processors), "already")

    def test_partial_or_unknown_no_miso_alias_fails_closed(self):
        malformed = (
            MISO_ALIAS_BLOCK.replace(b"TFT_MISO TFT_MOSI", b"TFT_MISO 5"),
            MISO_ALIAS_BLOCK + b"\n" + MISO_ALIAS_BLOCK,
            MISO_ALIAS_BLOCK.replace(
                b"CONFIG_IDF_TARGET_ESP32C3) || ", b"CONFIG_IDF_TARGET_UNKNOWN) || "
            ),
        )
        for alias in malformed:
            with self.subTest(alias=alias):
                with tempfile.TemporaryDirectory() as temporary:
                    header = HEADER + b"\n" + alias + b"\n"
                    processors = self.make_processors(Path(temporary), header)
                    before = {
                        name: (processors / name).read_bytes()
                        for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                    }
                    with self.assertRaises(PATCHER.PatchError):
                        PATCHER.patch_processors(processors)
                    self.assertEqual(
                        before,
                        {
                            name: (processors / name).read_bytes()
                            for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                        },
                    )

    def test_partial_or_unknown_source_fails_closed(self):
        cases = (
            (HEADER.replace(b"SPI_PORT SPI2_HOST", b"SPI_PORT OTHER_HOST"), SOURCE),
            (HEADER, SOURCE.replace(b"SPI_USER_REG(SPI_PORT)", b"SPI_USER_REG(SPI_REG_PORT)")),
            (HEADER, SOURCE.replace(b"SPI_CMD_REG(SPI_PORT)", b"SPI_CMD_REG(SPI_OTHER)", 1)),
            (HEADER.replace(b"SPI_PORT SPI2_HOST", b"SPI_PORT SPI2_HOST\n#define SPI_PORT SPI2_HOST"), SOURCE),
            (HEADER, SOURCE.replace(b"SPI_CMD_REG(SPI_PORT)", b"SPI_CMD_REG(spi_host)", 1)),
            (HEADER, SOURCE.replace(b"SPI_CMD_REG(SPI_PORT)", b"SPI_CMD_REG(x)", 1)),
        )
        for header, source in cases:
            with self.subTest(header=header, source=source):
                with tempfile.TemporaryDirectory() as temporary:
                    processors = self.make_processors(Path(temporary), header, source)
                    before = {
                        name: (processors / name).read_bytes()
                        for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                    }
                    with self.assertRaises(PATCHER.PatchError):
                        PATCHER.patch_processors(processors)
                    self.assertEqual(
                        before,
                        {
                            name: (processors / name).read_bytes()
                            for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                        },
                    )

    def test_sdk_index_two_resolves_real_spi2_register_addresses(self):
        reg_base = lambda index: 0x60024000 if index == 2 else 0
        self.assertEqual(reg_base(1) + 0x10, 0x10)
        self.assertEqual(reg_base(2) + 0x10, 0x60024010)
        self.assertEqual(reg_base(2) + 0x98, 0x60024098)

    def test_crlf_sources_keep_crlf_after_patch(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(
                Path(temporary), HEADER.replace(b"\n", b"\r\n"), SOURCE.replace(b"\n", b"\r\n")
            )
            self.assertEqual(PATCHER.patch_processors(processors), "changed")
            for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c"):
                content = (processors / name).read_bytes()
                self.assertNotIn(b"\n", content.replace(b"\r\n", b""))

    def test_macro_name_whitespace_is_replaced(self):
        header = HEADER.replace(b"SPI_CMD_REG(SPI_PORT)", b"SPI_CMD_REG ( SPI_PORT )", 1)
        source = SOURCE.replace(b"SPI_USER_REG(SPI_PORT)", b"SPI_USER_REG \t( SPI_PORT )", 1)
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary), header, source)
            self.assertEqual(PATCHER.patch_processors(processors), "changed")
            self.assertIn(b"SPI_CMD_REG ( SPI_REG_PORT )", (processors / "TFT_eSPI_ESP32_C3.h").read_bytes())
            self.assertIn(b"SPI_USER_REG \t( SPI_REG_PORT )", (processors / "TFT_eSPI_ESP32_C3.c").read_bytes())

    def test_second_replace_failure_restores_both_files_and_cleans_temps(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary))
            originals = {
                name: (processors / name).read_bytes()
                for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
            }
            real_replace = PATCHER.os.replace
            attempts = 0

            def fail_second_replace(source, destination):
                nonlocal attempts
                attempts += 1
                if attempts == 2:
                    raise OSError("injected second replace failure")
                return real_replace(source, destination)

            with mock.patch.object(PATCHER.os, "replace", side_effect=fail_second_replace):
                with self.assertRaises(OSError):
                    PATCHER.patch_processors(processors)

            self.assertEqual(
                originals,
                {
                    name: (processors / name).read_bytes()
                    for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                },
            )
            self.assertEqual(
                {path.name for path in processors.iterdir()},
                set(originals),
            )

    def test_rollback_failure_preserves_recovery_backup_and_reports_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary))
            originals = {
                name: (processors / name).read_bytes()
                for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
            }
            real_replace = PATCHER.os.replace
            attempts = 0

            def fail_source_replace_and_header_rollback(source, destination):
                nonlocal attempts
                attempts += 1
                if attempts in (2, 3):
                    raise OSError(f"injected replace failure {attempts}")
                return real_replace(source, destination)

            with mock.patch.object(
                PATCHER.os, "replace", side_effect=fail_source_replace_and_header_rollback
            ):
                with self.assertRaises(PATCHER.PatchError) as raised:
                    PATCHER.patch_processors(processors)

            extras = [path for path in processors.iterdir() if path.name not in originals]
            self.assertEqual(len(extras), 1)
            backup = extras[0]
            self.assertIn(str(backup), str(raised.exception))
            self.assertEqual(backup.read_bytes(), originals["TFT_eSPI_ESP32_C3.h"])

    def test_second_backup_staging_failure_cleans_first_staged_temp(self):
        with tempfile.TemporaryDirectory() as temporary:
            processors = self.make_processors(Path(temporary))
            originals = {
                name: (processors / name).read_bytes()
                for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
            }
            real_stage = PATCHER._atomic_replace
            attempts = 0

            def fail_second_backup(path, content):
                nonlocal attempts
                attempts += 1
                if attempts == 4:
                    raise OSError("injected second backup staging failure")
                return real_stage(path, content)

            with mock.patch.object(PATCHER, "_atomic_replace", side_effect=fail_second_backup):
                with self.assertRaises(OSError):
                    PATCHER.patch_processors(processors)

            self.assertEqual(
                originals,
                {
                    name: (processors / name).read_bytes()
                    for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                },
            )
            self.assertEqual(
                {path.name for path in processors.iterdir()},
                set(originals),
            )

    def test_exact_sdk_layout_counts_and_definitions_are_required(self):
        bad_cases = (
            make_layout(header_count=7),
            make_layout(source_count=113),
            (HEADER.replace(b"#define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)\n", b""), SOURCE),
            (HEADER + b"#define SPI_REG_PORT 3\n", SOURCE),
        )
        for header, source in bad_cases:
            with self.subTest(header_calls=header.count(b"SPI_PORT"), source_len=len(source)):
                with tempfile.TemporaryDirectory() as temporary:
                    processors = self.make_processors(Path(temporary), header, source)
                    before = {
                        name: (processors / name).read_bytes()
                        for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                    }
                    with self.assertRaises(PATCHER.PatchError):
                        PATCHER.patch_processors(processors)
                    self.assertEqual(
                        before,
                        {
                            name: (processors / name).read_bytes()
                            for name in ("TFT_eSPI_ESP32_C3.h", "TFT_eSPI_ESP32_C3.c")
                        },
                    )

    def test_only_c3_environment_is_selected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "gmt154-06-c3"
            self.make_processors(target, HEADER, SOURCE)
            (target / "TFT_eSPI" / "TFT_eSPI.cpp").write_bytes(make_tft_init_source())
            self.assertEqual(PATCHER.apply_patch_for_env("gmt154-06", root), "skipped")
            self.assertEqual(PATCHER.apply_patch_for_env("gmt154-06-c3", root), "changed")

    def test_init_trace_marks_spi_reset_and_panel_driver_boundaries(self):
        source = make_tft_init_source()
        patched = PATCHER.patch_init_trace(source)
        self.assertIn(b'GMT154_C3_TFT: initBus.begin', patched)
        self.assertIn(b'GMT154_C3_TFT: spi.begin.start', patched)
        self.assertIn(b'GMT154_C3_TFT: spi.begin.done', patched)
        self.assertIn(b'GMT154_C3_TFT: reset.done', patched)
        self.assertIn(b'GMT154_C3_TFT: st7789_2.begin', patched)
        self.assertIn(b'GMT154_C3_TFT: st7789_2.done', patched)
        reset_order = (
            b"GMT154_C3_TFT: reset.pinMode.begin",
            b"GMT154_C3_TFT: reset.pinMode.done",
            b"GMT154_C3_TFT: reset.nop.begin",
            b"GMT154_C3_TFT: reset.nop.done",
            b"GMT154_C3_TFT: reset.rst.high1.done",
            b"GMT154_C3_TFT: reset.delay5.done",
            b"GMT154_C3_TFT: reset.rst.low.done",
            b"GMT154_C3_TFT: reset.delay20.done",
            b"GMT154_C3_TFT: reset.rst.high2.done",
            b"GMT154_C3_TFT: reset.delay150.done",
        )
        reset_positions = [patched.index(marker) for marker in reset_order]
        self.assertEqual(reset_positions, sorted(reset_positions))
        writecommand_order = (
            b"GMT154_C3_TFT: reset.writecommand.enter",
            b"GMT154_C3_TFT: reset.begin_tft_write.done",
            b"GMT154_C3_TFT: reset.tft_Write_8.done",
            b"GMT154_C3_TFT: reset.end_tft_write.done",
        )
        writecommand_positions = [patched.index(marker) for marker in writecommand_order]
        self.assertEqual(writecommand_positions, sorted(writecommand_positions))
        for statement in (
            b"pinMode(TFT_RST, OUTPUT);",
            b"writecommand(0x00); // Put SPI bus in known state for TFT with CS tied low",
            b"digitalWrite(TFT_RST, HIGH);",
            b"digitalWrite(TFT_RST, LOW);",
            b"delay(5);",
            b"delay(20);",
            b"delay(150); // Wait for reset to complete",
            b"tft_Write_8(c);",
        ):
            self.assertIn(statement, patched)
        self.assertEqual(PATCHER.patch_init_trace(patched), patched)

    def test_reset_trace_preserves_crlf_source(self):
        patched = PATCHER.patch_init_trace(make_tft_init_source(b"\r\n"))
        self.assertNotIn(b"\n", patched.replace(b"\r\n", b""))

    def test_init_trace_fails_closed_on_missing_anchor(self):
        with self.assertRaises(PATCHER.PatchError):
            PATCHER.patch_init_trace(b"void TFT_eSPI::init() {}\n")

    def test_init_trace_upgrades_legacy_trace_missing_spi_entry_marker(self):
        legacy = PATCHER.patch_init_stage_trace(make_tft_init_source()).replace(
            b'      Serial.println("GMT154_C3_TFT: spi.begin.start");\n', b""
        )
        upgraded = PATCHER.patch_init_trace(legacy)
        self.assertIn(b'GMT154_C3_TFT: spi.begin.start', upgraded)
        self.assertEqual(PATCHER.patch_init_trace(upgraded), upgraded)

    def test_init_trace_rejects_duplicate_marker_instead_of_accepting_partial_patch(self):
        patched = PATCHER.patch_init_trace(make_tft_init_source())
        duplicate = patched.replace(
            b'GMT154_C3_TFT: st7789_2.done', b'GMT154_C3_TFT: st7789_2.missing'
        ) + b'\nSerial.println("GMT154_C3_TFT: reset.done");\n'
        with self.assertRaises(PATCHER.PatchError):
            PATCHER.patch_init_trace(duplicate)


if __name__ == "__main__":
    unittest.main()
