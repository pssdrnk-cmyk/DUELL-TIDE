#!/usr/bin/env python3
"""Protect cold-code selection when an ISR dispatches through global tables."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('size_fns', Path(__file__).resolve().parents[1] / 'tools' / 'size_fns.py')
size_fns = importlib.util.module_from_spec(spec)
spec.loader.exec_module(size_fns)

IR = '''@engines = internal constant [1 x ptr] [ptr @engine]
@engine = internal constant { ptr, ptr } {
  ptr @render_alias, ptr @engine_cycle
}
@engine_cycle = internal constant ptr @engine
@render_alias = internal alias void (), ptr @render
@ui_table = internal constant ptr @cold_ui

define void @fm1_alnk0_irq() #0 {
  %hook = load ptr, ptr @engines
  call void %hook()
  ret void
}
define void @fm1_timer5_irq() #0 {
  call void @scan()
  ret void
}
define void @render() #0 {
  call void @sample()
  ret void
}
define void @sample() #0 {
  ret void
}
define void @scan() #0 {
  ret void
}
define void @cold_ui() #0 {
  ret void
}
'''

class SizeSelectionTest(unittest.TestCase):
    def test_indirect_render_and_helpers_remain_fast(self):
        self.assertEqual(size_fns.isr_reach(IR), {'fm1_alnk0_irq', 'fm1_timer5_irq', 'render', 'sample', 'scan'})

    def test_unreachable_tables_do_not_make_ui_hot(self):
        self.assertNotIn('cold_ui', size_fns.isr_reach(IR))
        self.assertEqual(size_fns.isr_reach(''), set())

    def test_selection_excludes_indirect_targets(self):
        with tempfile.TemporaryDirectory() as temp:
            src, dst = Path(temp) / 'in.ll', Path(temp) / 'out.ll'
            src.write_text(IR)
            with patch.object(size_fns, 'size_names', return_value={'render', 'sample', 'scan', 'cold_ui'}):
                size_fns.main(src, dst)
            result = dst.read_text()
        self.assertIn('define void @cold_ui() minsize #0 {', result)
        for name in ('render', 'sample', 'scan'):
            self.assertIn('define void @' + name + '() #0 {', result)

if __name__ == '__main__':
    unittest.main()
