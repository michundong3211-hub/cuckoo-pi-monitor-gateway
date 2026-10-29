# LVGL font decode smoke

`script/run_font_smoke.py` reads the real 3.5 LVGL flags from
`platformio.ini`, compiles the actual LVGL 9.4 `lv_font_fmt_txt.c` and the
five project fonts with MinGW, then calls each font's real glyph callbacks.

Normal GREEN run:

```powershell
python script/run_font_smoke.py
```

Disabled-font negative run (isolated output directory):

```powershell
python script/run_font_smoke.py --negative
```

The negative mode requires every compile step to succeed, the known bitmap
`NULL` failure for `U+0041`, and process exit code `1`. It rejects other
failures. The smoke covers English/digits/`.`/`-` where supported, non-empty
decoded A8 output, and 100 repeated full-bitmap byte comparisons.

The C++ host adapter supplies only LVGL linkage needed by this smoke:
cache flush, one-byte-aligned A8 stride, malloc/free, bitmap search, and
global state. It does not validate the ESP32 allocator, its 32 KB device heap,
or device cache behavior.
