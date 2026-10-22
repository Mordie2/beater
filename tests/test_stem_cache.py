"""Cache lifecycle tests with a tiny stand-in separator; no model download needed."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import wave

spec = importlib.util.spec_from_file_location("worker", Path(__file__).parents[1] / "scripts/stem-worker.py")
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)


def wav(path, value=1):
    with wave.open(str(path), "wb") as out:
        out.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
        out.writeframes(bytes([value, 0, value, 0]) * 441)


class CacheTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "voice café test.wav"
        self.cache = self.root / "cache"
        wav(self.source)

    def test_reload_and_missing_original_reuse_vocals(self):
        with patch.object(worker, "separate_audio", side_effect=lambda src, dest: wav(dest)) as separator:
            first = worker.prepare(self.source, self.cache)
            second = worker.prepare(self.source, self.cache)
            self.source.unlink()
            third = worker.prepare(self.source, self.cache)
        self.assertEqual(separator.call_count, 1)
        self.assertEqual(first["vocals"], second["vocals"])
        self.assertEqual(first["vocals"], third["vocals"])
        self.assertTrue(Path(third["original"]).is_file())

    def test_changed_contents_and_corrupt_stem_are_reprocessed(self):
        with patch.object(worker, "separate_audio", side_effect=lambda src, dest: wav(dest)) as separator:
            first = worker.prepare(self.source, self.cache)
            wav(self.source, 2)
            second = worker.prepare(self.source, self.cache)
            self.assertNotEqual(first["sha256"], second["sha256"])
            Path(second["vocals"]).write_bytes(b"broken")
            worker.prepare(self.source, self.cache)
        self.assertEqual(separator.call_count, 3)

    def test_probe_reuses_without_copying_and_rejects_changed_or_broken_audio(self):
        with patch.object(worker, "separate_audio", side_effect=lambda src, dest: wav(dest)):
            ready = worker.prepare(self.source, self.cache)
        manifest_before = (self.cache / "manifest.json").read_bytes()
        self.assertEqual(worker.probe_cache(self.source, self.cache)["vocals"], ready["vocals"])
        self.assertEqual((self.cache / "manifest.json").read_bytes(), manifest_before)
        wav(self.source, 2)
        self.assertIsNone(worker.probe_cache(self.source, self.cache))
        self.source.unlink()
        self.assertIsNotNone(worker.probe_cache(self.source, self.cache))
        Path(ready["vocals"]).write_bytes(b"broken")
        self.assertIsNone(worker.probe_cache(self.source, self.cache))

    def test_failure_keeps_original_without_publishing_partial_stem(self):
        def fail(src, dest):
            dest.write_bytes(b"partial")
            raise RuntimeError("interrupted")
        with patch.object(worker, "separate_audio", side_effect=fail):
            with self.assertRaises(RuntimeError):
                worker.prepare(self.source, self.cache)
        manifest = worker.read_manifest(self.cache / "manifest.json")
        self.assertTrue(Path(manifest["original"]).is_file())
        self.assertEqual(manifest["vocals"], "")
        self.assertEqual(list(self.cache.rglob("*.tmp*")), [])


if __name__ == "__main__":
    unittest.main()
