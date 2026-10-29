#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "lvgl.h"
#include "src/core/lv_global.h"

extern "C" {
extern const lv_font_t lv_font_montserrat_regular_12;
extern const lv_font_t lv_font_montserrat_regular_14;
extern const lv_font_t lv_font_montserrat_regular_16;
extern const lv_font_t lv_font_montserrat_regular_20;
extern const lv_font_t lv_font_montserrat_54_digits;
extern const lv_font_t lv_font_montserrat_80_digits;

// Host adapter for the real LVGL decoder: cache flush, A8 stride, malloc/free,
// bitmap search, and LVGL global state. It does not model the ESP32 allocator,
// its 32 KB device heap, or device-specific cache behavior.
void lv_draw_buf_flush_cache(const lv_draw_buf_t *, const lv_area_t *) {}
constexpr uint32_t host_a8_stride(uint32_t width) { return width; }
static_assert(LV_DRAW_BUF_STRIDE_ALIGN == 1, "host A8 stride must be aligned to 1 byte");
uint32_t lv_draw_buf_width_to_stride(uint32_t width, lv_color_format_t color_format) {
    if(color_format != LV_COLOR_FORMAT_A8) std::abort();
    return host_a8_stride(width);
}
void * lv_malloc(size_t size) { return std::malloc(size); }
void * lv_malloc_zeroed(size_t size) { return std::calloc(1, size); }
void lv_free(void * data) { std::free(data); }
void * lv_memcpy(void * dst, const void * src, size_t len) { return std::memcpy(dst, src, len); }
lv_global_t lv_global = {};

// Host-only adapter for the one LVGL utility used by the real font decoder.
void * lv_utils_bsearch(const void * key, const void * base, size_t count, size_t size,
                        int (*compare)(const void *, const void *))
{
    const auto *bytes = static_cast<const uint8_t *>(base);
    size_t first = 0;
    size_t last = count;
    while(first < last) {
        const size_t middle = first + (last - first) / 2;
        const void *element = bytes + middle * size;
        const int result = compare(key, element);
        if(result == 0) return const_cast<void *>(element);
        if(result < 0) last = middle;
        else first = middle + 1;
    }
    return nullptr;
}
}

namespace {

struct FontCase {
    const char * name;
    const lv_font_t * font;
    const char * glyphs;
};

bool check_glyph(const FontCase & font_case, unsigned char letter)
{
    lv_font_glyph_dsc_t glyph = {};
    if(!font_case.font->get_glyph_dsc(font_case.font, &glyph, letter, 0)) {
        std::fprintf(stderr, "%s: missing glyph U+%04X\n", font_case.name, letter);
        return false;
    }

    glyph.resolved_font = font_case.font;
    if(glyph.box_w == 0 || glyph.box_h == 0) {
        std::fprintf(stderr, "%s: empty glyph box U+%04X\n", font_case.name, letter);
        return false;
    }

    const size_t bitmap_size = static_cast<size_t>(glyph.box_w) * glyph.box_h;
    uint8_t bitmap[8192] = {};
    if(bitmap_size > sizeof(bitmap)) {
        std::fprintf(stderr, "%s: test buffer too small for U+%04X\n", font_case.name, letter);
        return false;
    }

    lv_draw_buf_t draw_buf = {};
    draw_buf.data = bitmap;
    draw_buf.data_size = sizeof(bitmap);

    const void * first_result = font_case.font->get_glyph_bitmap(&glyph, &draw_buf);
    const bool first_nonzero = std::any_of(bitmap, bitmap + bitmap_size, [](uint8_t value) {
        return value != 0;
    });
    if(first_result == nullptr || !first_nonzero) {
        std::fprintf(stderr, "%s: first decode empty U+%04X (result=%p)\n",
                     font_case.name, letter, first_result);
        return false;
    }

    uint8_t expected[8192] = {};
    std::memcpy(expected, bitmap, bitmap_size);
    for(int iteration = 0; iteration < 100; ++iteration) {
        std::fill(bitmap, bitmap + bitmap_size, 0xA5);
        const void * repeated_result = font_case.font->get_glyph_bitmap(&glyph, &draw_buf);
        if(repeated_result == nullptr || std::memcmp(bitmap, expected, bitmap_size) != 0) {
            std::fprintf(stderr, "%s: repeated decode differs at iteration %d U+%04X\n",
                         font_case.name, iteration + 1, letter);
            return false;
        }
    }

    return true;
}

bool check_gateway_ip_text_width()
{
    constexpr const char * text = "IP 255.255.255.255";
    int32_t width = 0;
    for(std::size_t index = 0; text[index] != '\0'; ++index) {
        const uint32_t letter = static_cast<unsigned char>(text[index]);
        const uint32_t next = text[index + 1] == '\0'
                                  ? 0
                                  : static_cast<unsigned char>(text[index + 1]);
        lv_font_glyph_dsc_t glyph = {};
        if(!lv_font_montserrat_regular_16.get_glyph_dsc(
               &lv_font_montserrat_regular_16, &glyph, letter, next)) {
            std::fprintf(stderr, "montserrat-16: missing glyph U+%04X\n",
                         static_cast<unsigned>(letter));
            return false;
        }
        width += glyph.adv_w;
    }
    if(width > 208) {
        std::fprintf(stderr,
                     "montserrat-16: gateway IP text width %ld exceeds 208px\n",
                     static_cast<long>(width));
        return false;
    }
    std::printf("PASS LVGL text width %ldpx <= 208px for %s\n",
                static_cast<long>(width), text);
    return true;
}

}  // namespace

int main()
{
    const FontCase fonts[] = {
        {"montserrat-12", &lv_font_montserrat_regular_12, "A0.-"},
        {"montserrat-14", &lv_font_montserrat_regular_14, "A0.-"},
        {"montserrat-16", &lv_font_montserrat_regular_16, "A0.-"},
        {"montserrat-20", &lv_font_montserrat_regular_20, "A0.-"},
        {"montserrat-54-digits", &lv_font_montserrat_54_digits,
         ".-0123456789"},
        {"montserrat-80-digits", &lv_font_montserrat_80_digits, "0.-"},
    };

    for(const FontCase & font_case : fonts) {
        for(const char * glyph = font_case.glyphs; *glyph != '\0'; ++glyph) {
            if(!check_glyph(font_case, static_cast<unsigned char>(*glyph))) return 1;
        }
    }

    if(!check_gateway_ip_text_width()) return 1;

    std::puts("PASS real LVGL compressed-font glyph decode and repeated decode");
    return 0;
}
