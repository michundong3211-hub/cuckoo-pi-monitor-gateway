# Isolated Waveshare 2.8 diagnostic only. Each known LVGL file is pinned by SHA-256.
from pathlib import Path
import hashlib
Import("env")
root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "lvgl/src"

def exact(text, old, new, n):
    if text.count(old) != n:
        raise RuntimeError("LVGL v4 context mismatch " + repr(old) + ": " + str(text.count(old)))
    return text.replace(old, new)

def patch_file(path, before_hash, after_hash, transform):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest == after_hash:
        print("LVGL v4 already patched: " + str(path.name)); return
    if digest != before_hash:
        raise RuntimeError("LVGL v4 source mismatch: " + str(path) + " " + digest)
    output = transform(data.decode("utf-8")).encode("utf-8")
    actual = hashlib.sha256(output).hexdigest()
    if actual != after_hash:
        raise RuntimeError("LVGL v4 transformed hash mismatch: " + str(path))
    path.write_bytes(output)
    print("LVGL v4 patched " + str(path.name) + " sha256=" + actual)

def patch_mem(text):
    text = exact(text, '#include "lv_mem_private.h"', '#include "lv_mem_private.h"\nextern void freeze_alloc_failure(unsigned op, unsigned size);', 1)
    def section(text, start, end, needle, op, size):
        a=text.index(start); b=text.index(end,a); part=text[a:b]
        part=exact(part, needle, needle+'\n        freeze_alloc_failure('+str(op)+', (unsigned)'+size+');',1)
        return text[:a]+part+text[b:]
    text=section(text,'void * lv_malloc(size_t size)','void * lv_malloc_zeroed(size_t size)','if(alloc == NULL) {',1,'size')
    text=section(text,'void * lv_malloc_zeroed(size_t size)','void * lv_calloc(size_t num, size_t size)','if(alloc == NULL) {',2,'size')
    text=section(text,'void * lv_realloc(void * data_p, size_t new_size)','lv_result_t lv_mem_test(void)','if(new_p == NULL) {',3,'new_size')
    return text

def patch_refr(text):
    text=exact(text,'refr_area(&sub_area, y_off);','freeze_diag_refr_area(&sub_area, y_off);',2)
    text=exact(text,'refr_area(&disp_refr->inv_areas[i], 0);','freeze_diag_refr_area(&disp_refr->inv_areas[i], 0);',1)
    text=exact(text,'draw_buf_flush(disp_refr);','freeze_diag_draw_flush(disp_refr);',3)
    text=exact(text,'lv_draw_dispatch_wait_for_request();','freeze_diag_draw_wait();',2)
    text=exact(text,'lv_draw_dispatch();','freeze_diag_draw_dispatch();',2)
    anchor='static void draw_buf_flush(lv_display_t * disp);'
    wrappers='''
extern void freeze_indev_enter(unsigned id);
extern void freeze_indev_leave(unsigned id);
extern void freeze_render_progress(int phase,int maxrow,int w,int h,int x1,int y1,int x2,int y2);
static void freeze_diag_refr_area(const lv_area_t * area,int32_t y) { freeze_indev_enter(16); refr_area(area,y); freeze_indev_leave(16); }
static void freeze_diag_draw_flush(lv_display_t * disp) { freeze_indev_enter(17); draw_buf_flush(disp); freeze_indev_leave(17); }
static void freeze_diag_draw_wait(void) { freeze_indev_enter(18); lv_draw_dispatch_wait_for_request(); freeze_indev_leave(18); }
static void freeze_diag_draw_dispatch(void) { freeze_indev_enter(19); lv_draw_dispatch(); freeze_indev_leave(19); }
'''
    text=exact(text,anchor,anchor+'\n'+wrappers,1)
    x='lv_display_send_event(disp_refr, LV_EVENT_RENDER_START, NULL);'
    text=exact(text,x,'freeze_render_progress(0,0,0,0,0,0,0,0);\n    '+x+'\n    freeze_render_progress(1,0,0,0,0,0,0,0);',1)
    x='int32_t max_row = get_max_row(disp_refr, w, h);'
    text=exact(text,x,'freeze_render_progress(2,0,w,h,inv_a.x1,inv_a.y1,inv_a.x2,inv_a.y2);\n            '+x+'\n            freeze_render_progress(3,max_row,w,h,inv_a.x1,inv_a.y1,inv_a.x2,inv_a.y2);',1)
    return text

patch_file(root/'stdlib/lv_mem.c','8178b28d317d7ff3773d3f4314b04cf0e039f4c3cf2c9934517010180c317112','dcbd47b19a3ad9f8f2b1d53c4524bf65ec2c01e051f8a4b2a36a6134a10cb1d7',patch_mem)
patch_file(root/'core/lv_refr.c','46600ee6188e97b56cfe6d91b6fad9b7cac7eff32c64ed74d5242445b048f9db','46899ccc51a4a5cddd2a3ab08cb69ddd05098c51a2c065df5518d24485a05c0a',patch_refr)
