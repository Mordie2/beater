"""Two real worker processes must not reserve GPU memory concurrently."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = '''
import importlib.util, sys
spec=importlib.util.spec_from_file_location('worker',sys.argv[1])
w=importlib.util.module_from_spec(spec); spec.loader.exec_module(w)
with w.recognition_lock():
    print('acquired',flush=True)
    if sys.argv[2]=='hold': sys.stdin.read(1)
'''


class LockTests(unittest.TestCase):
    def test_second_deck_waits_until_first_releases(self):
        worker = str(Path(__file__).parents[1] / 'scripts/transcript-worker.py')
        with tempfile.TemporaryDirectory() as temp:
            env = dict(os.environ, BEATER_TRANSCRIPT_LOCK=str(Path(temp) / 'worker.lock'))
            first = subprocess.Popen([sys.executable, '-c', SCRIPT, worker, 'hold'],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
            second = None
            try:
                self.assertEqual(first.stdout.readline().strip(), 'acquired')
                second = subprocess.Popen([sys.executable, '-c', SCRIPT, worker, 'finish'],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
                self.assertIn('queued', second.stdout.readline())
                first.communicate('x', timeout=5)
                out, err = second.communicate(timeout=5)
                self.assertEqual(second.returncode, 0, err)
                self.assertIn('acquired', out)
            finally:
                for process in (first, second):
                    if process and process.poll() is None:
                        process.kill(); process.communicate()


if __name__ == '__main__':
    unittest.main()
