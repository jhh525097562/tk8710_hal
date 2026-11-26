"""Compatibility entry: unified tests cover both external and D8 polarities.
The old standalone GPIO fixture is archived with the legacy LED implementation.
"""
import pathlib
import runpy

if __name__ == '__main__':
    runpy.run_path(str(pathlib.Path(__file__).with_name('test_unified_led.py')), run_name='__main__')
