import subprocess
import unittest
from pathlib import Path
ROOT = Path(__file__).parents[2]
class Integration(unittest.TestCase):
    def test_task_lock_and_log_order(self):
        source = (ROOT/'src/display.cpp').read_text(encoding='utf-8')
        task = source.split('void lvgl_loop_handler(void *parameter) {',1)[1].split('#else  // ENABLE_TFT',1)[0]
        acquire = task.index('if (semaphore_acquired)')
        capture = task.index('gmt154::captureLvglBudget(')
        unlock = task.index('xSemaphoreGive(semaphore);',capture)
        log = task.index('gmt154LogLvglBudget(lvglBudgetSnapshot);',unlock)
        self.assertLess(acquire,capture)
        self.assertLess(capture,unlock)
        self.assertLess(unlock,log)
        self.assertEqual(task.count('captureLvglBudget('),1)
        self.assertIn('#if defined(GMT154_06_C3)\n      gmt154::captureLvglBudget',task)
        self.assertIn('#if defined(GMT154_06_C3)\n      gmt154LogLvglBudget',task)
        self.assertNotIn('lv_mem_monitor(', (ROOT/'src/main_gateway.cpp').read_text(encoding='utf-8'))
    def test_other_targets_do_not_include_lvgl_or_freertos(self):
        result = subprocess.run(['g++','-E','-P','-Isrc','-x','c++','-'],
            input='#include "gmt154_lvgl_budget_probe.hpp"\nint main() {}\n',
            cwd=ROOT,text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertNotIn('captureLvglBudget',result.stdout)
    def test_probe_has_no_logging_or_allocation(self):
        source=(ROOT/'src/gmt154_lvgl_budget_probe.hpp').read_text()
        for token in ['Log.', 'malloc(', 'new ', 'String(', 'sizeof(StackType_t)', 'xTaskCreate']:
            self.assertNotIn(token,source)
        self.assertIn('uxTaskGetStackHighWaterMark(nullptr)',source)
if __name__ == '__main__': unittest.main()
