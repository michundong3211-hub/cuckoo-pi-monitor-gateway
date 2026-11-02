# Isolated Waveshare 2.8 diagnostic only. Fail closed for other LVGL revisions.
from pathlib import Path
import hashlib
Import("env")
source = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "lvgl/src/indev/lv_indev.c"
original_hash = "8049f0fc4a11243d0d9824630398a4303c41736052c0d2af8d16433792dc349f"
patched_hash = "02684cee7605e220aafa4c87479715a203c8dd4ee77ecf9edf8731a81444267c"
data = source.read_bytes()
hash_value = hashlib.sha256(data).hexdigest()
if hash_value == patched_hash:
    print("LVGL indev v3 instrumentation already present")
elif hash_value != original_hash:
    raise RuntimeError("Unexpected LVGL indev source hash; refusing diagnostic patch: " + hash_value)
else:
    text = data.decode("utf-8")
    def replace_exact(old, new, expected):
        global text
        count = text.count(old)
        if count != expected:
            raise RuntimeError("LVGL patch context count mismatch: " + repr(old) + " count=" + str(count))
        text = text.replace(old, new)
    replace_exact('    lv_indev_read(timer->user_data);', '    freeze_indev_enter(9);\n    lv_indev_read(timer->user_data);\n    freeze_indev_leave(9);', 1)
    replace_exact('        indev_read_core(indev, &data);', '        freeze_indev_enter(10);\n        indev_read_core(indev, &data);\n        freeze_indev_leave(10);', 1)
    replace_exact('        continue_reading = indev->mode != LV_INDEV_MODE_EVENT && data.continue_reading;', '        continue_reading = indev->mode != LV_INDEV_MODE_EVENT && data.continue_reading;\n        freeze_indev_input((unsigned)data.state, (unsigned)continue_reading);', 1)
    replace_exact('indev_proc_reset_query_handler(indev);', 'freeze_diag_reset(indev);', 3)
    replace_exact('            indev_pointer_proc(indev, &data);', '            freeze_indev_enter(12);\n            indev_pointer_proc(indev, &data);\n            freeze_indev_leave(12);', 1)
    replace_exact('indev_proc_release(i);', 'freeze_diag_release(i);', 3)
    replace_exact('indev_proc_short_click(i)', 'freeze_diag_short_click(i)', 3)
    replace_exact('indev_proc_short_click(indev)', 'freeze_diag_short_click(indev)', 1)
    count = text.count('send_event(LV_EVENT_')
    if count < 10: raise RuntimeError("Unexpected send_event sites")
    text = text.replace('send_event(LV_EVENT_', 'freeze_diag_send_event(LV_EVENT_')
    anchor='static lv_result_t send_event(lv_event_code_t code, void * param);'
    wrappers='''
extern void freeze_indev_enter(unsigned id);
extern void freeze_indev_leave(unsigned id);
extern void freeze_indev_input(unsigned state, unsigned more);
extern void freeze_indev_event(unsigned code);
static void freeze_diag_reset(lv_indev_t * indev) {
    freeze_indev_enter(11); indev_proc_reset_query_handler(indev); freeze_indev_leave(11);
}
static void freeze_diag_release(lv_indev_t * indev) {
    freeze_indev_enter(13); indev_proc_release(indev); freeze_indev_leave(13);
}
static lv_result_t freeze_diag_short_click(lv_indev_t * indev) {
    freeze_indev_enter(14); lv_result_t r = indev_proc_short_click(indev); freeze_indev_leave(14); return r;
}
static lv_result_t freeze_diag_send_event(lv_event_code_t code, void * param) {
    freeze_indev_event((unsigned)code); freeze_indev_enter(15);
    lv_result_t r = send_event(code, param); freeze_indev_leave(15); return r;
}
'''
    replace_exact(anchor,anchor+wrappers,1)
    result=text.encode("utf-8")
    actual=hashlib.sha256(result).hexdigest()
    if actual != patched_hash:
        raise RuntimeError("Patched LVGL source hash mismatch")
    source.write_bytes(result)
    print("LVGL indev v3 diagnostic installed; sha256="+actual)
