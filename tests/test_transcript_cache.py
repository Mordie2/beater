import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("transcript", Path(__file__).parents[1] / "scripts/transcript-worker.py")
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)


class TranscriptTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / "vocal café.wav"
        self.source.write_bytes(b"audio one")
        self.result = dict(text="Hello world.", language="en", segments=[dict(start=0, end=1, text="Hello world.")])

    def test_reuse_then_changed_audio_invalidates(self):
        with patch.object(worker, "recognize", side_effect=lambda *_: dict(self.result)) as recognize:
            worker.transcribe(self.source, "unused", device="cpu")
            worker.transcribe(self.source, "unused", device="cpu")
            self.assertEqual(recognize.call_count, 1)
            self.source.write_bytes(b"audio two")
            worker.transcribe(self.source, "unused", device="cpu")
            self.assertEqual(recognize.call_count, 2)

    def test_interrupted_retry_preserves_complete_result(self):
        with patch.object(worker, "recognize", return_value=dict(self.result)):
            worker.transcribe(self.source, "unused", device="cpu")
        with patch.object(worker, "recognize", side_effect=RuntimeError("stopped")):
            with self.assertRaises(RuntimeError):
                worker.transcribe(self.source, "unused", force=True, device="cpu")
        cached = json.loads(self.source.with_suffix(".transcript.json").read_text())
        self.assertEqual(cached["text"], "Hello world.")
        self.assertFalse(list(self.source.parent.glob("*.tmp")))

    def test_empty_transcript_is_valid_and_reused(self):
        with patch.object(worker, "recognize", return_value=dict(text="", language="en", segments=[])) as recognize:
            worker.transcribe(self.source, "unused", device="cpu")
            worker.transcribe(self.source, "unused", device="cpu")
            self.assertEqual(recognize.call_count, 1)
        self.assertEqual(self.source.with_suffix(".transcript.txt").read_text(), "")

    def test_broken_cache_and_explicit_retry(self):
        self.source.with_suffix(".transcript.json").write_text('{"version":')
        with patch.object(worker, "recognize", side_effect=lambda *_: dict(self.result)) as recognize:
            worker.transcribe(self.source, "unused", device="cpu")
            worker.transcribe(self.source, "unused", force=True, device="cpu")
            self.assertEqual(recognize.call_count, 2)

    def test_old_speech_filter_cache_is_invalidated(self):
        with patch.object(worker, "recognize", side_effect=lambda *_: dict(self.result)) as recognize:
            worker.transcribe(self.source, "unused", device="cpu")
            path = self.source.with_suffix(".transcript.json")
            data = json.loads(path.read_text())
            data["version"] = "faster-whisper-base-1.2.1-v1"
            path.write_text(json.dumps(data))
            worker.transcribe(self.source, "unused", device="cpu")
            self.assertEqual(recognize.call_count, 2)

    def test_model_and_language_changes_invalidate_cache(self):
        with patch.object(worker, "recognize", side_effect=lambda *_: dict(self.result)) as recognize:
            worker.transcribe(self.source, "unused", device="cpu")
            worker.transcribe(self.source, "unused", language="en", device="cpu")
            worker.transcribe(self.source, "different-model", language="en", device="cpu")
            self.assertEqual(recognize.call_count, 3)

    def test_gpu_failure_retries_same_model_on_cpu_and_clears_partial_text(self):
        events = []
        with patch.object(worker, "configure_gpu", return_value=True), \
             patch.dict("sys.modules", {"ctranslate2": SimpleNamespace(get_cuda_device_count=lambda: 1)}), \
             patch.object(worker, "event", side_effect=lambda state, **kwargs: events.append(state)), \
             patch.object(worker, "recognize", side_effect=[RuntimeError("GPU out of memory"), dict(self.result)]) as recognize:
            worker.transcribe(self.source, "large-model", device="auto")
        self.assertEqual([call.args[1:3] for call in recognize.call_args_list],
                         [("large-model", "cuda"), ("large-model", "cpu")])
        self.assertIn('reset', events)

    def test_explicit_gpu_test_never_silently_passes_on_cpu(self):
        self.assertRaises(RuntimeError, worker.select_device, 'cuda', False, 1)
        self.assertEqual(worker.select_device('auto', False, 1), 'cpu')
        self.assertEqual(worker.select_device('auto', True, 1), 'cuda')

    def test_explicit_language_skips_guessing_and_gpu_uses_float16(self):
        from unittest.mock import MagicMock
        model = MagicMock()
        model.transcribe.return_value = ([], SimpleNamespace(language='en', duration=3))
        factory = MagicMock(return_value=model)
        with patch.dict('sys.modules', {
            'faster_whisper': SimpleNamespace(WhisperModel=factory),
            'faster_whisper.audio': SimpleNamespace(decode_audio=lambda *a, **k: object()),
        }):
            result = worker.recognize(self.source, 'unused', device='cuda', language='en', align=False)
        model.detect_language.assert_not_called()
        self.assertEqual(factory.call_args.kwargs['compute_type'], 'float16')
        self.assertEqual(result['device'], 'cuda')

    def test_singing_keeps_full_timeline_and_word_timestamps(self):
        from unittest.mock import MagicMock
        import numpy as np
        model = MagicMock()
        model.detect_language.return_value = ("en", .95, [])
        word = SimpleNamespace(start=12.2, end=12.8, word=" Hello", probability=.9)
        segment = SimpleNamespace(start=12.2, end=12.8, text=" Hello", words=[word])
        model.transcribe.return_value = ([segment], SimpleNamespace(language="en", duration=30))
        module = SimpleNamespace(WhisperModel=MagicMock(return_value=model))
        audio = np.zeros(30 * 16000, dtype=np.float32)
        audio[12 * 16000:13 * 16000] = .001
        audio_module = SimpleNamespace(decode_audio=MagicMock(return_value=audio))
        with patch.dict("sys.modules", {"faster_whisper": module, "faster_whisper.audio": audio_module}):
            result = worker.recognize(self.source, "unused", align=False)
        options = model.transcribe.call_args.kwargs
        self.assertFalse(options["vad_filter"])
        self.assertTrue(options["word_timestamps"])
        self.assertEqual(options["language"], "en")
        self.assertIs(model.transcribe.call_args.args[0], audio)
        self.assertTrue(model.detect_language.call_args.kwargs["vad_filter"])
        self.assertEqual(result["segments"][0]["words"][0]["start"], 12.2)

    def test_digital_silence_rejected_but_quiet_vocals_kept(self):
        import numpy as np
        audio = np.zeros(3 * 16000, dtype=np.float32)
        self.assertFalse(worker.has_audio_signal(audio, 0, 1))
        audio[2 * 16000] = .001
        self.assertTrue(worker.has_audio_signal(audio, 2, 2.5))
        self.assertFalse(worker.has_audio_signal(audio, 0, 1))

    def test_native_alignment_failure_keeps_completed_lyrics(self):
        from unittest.mock import MagicMock
        import numpy as np
        word = SimpleNamespace(start=1, end=2, word=' hurricane', probability=.95)
        segment = SimpleNamespace(start=1, end=2, text=' hurricane', words=[word])
        model = MagicMock()
        model.transcribe.return_value = ([segment], SimpleNamespace(language='en', duration=3))
        with patch.dict('sys.modules', {
            'faster_whisper': SimpleNamespace(WhisperModel=MagicMock(return_value=model)),
            'faster_whisper.audio': SimpleNamespace(decode_audio=lambda *a, **k: np.ones(48000)*.01),
        }), patch.object(worker, 'refine_timing', side_effect=RuntimeError('native alignment failure')):
            result = worker.recognize(self.source, 'unused', language='en')
        self.assertEqual(result['text'], 'hurricane')
        self.assertEqual(result['segments'][0]['words'][0]['start'], 1)
        self.assertEqual(result['alignment']['method'], 'whisper')

    def test_nonspeech_rejection_preserves_confident_words(self):
        from unittest.mock import MagicMock
        import numpy as np
        false = SimpleNamespace(start=1, end=2, text='False', words=[], no_speech_prob=.9, avg_logprob=-.6)
        true = SimpleNamespace(start=3, end=4, text='Singing', words=[], no_speech_prob=.9, avg_logprob=-.1)
        model = MagicMock()
        model.transcribe.return_value = ([false, true], SimpleNamespace(language='en', duration=5))
        with patch.dict('sys.modules', {
            'faster_whisper': SimpleNamespace(WhisperModel=MagicMock(return_value=model)),
            'faster_whisper.audio': SimpleNamespace(decode_audio=lambda *a, **k: np.ones(80000)*.01),
        }):
            result = worker.recognize(self.source, 'unused', language='en', align=False)
        self.assertEqual(result['text'], 'Singing')

    def test_unreliable_long_words_rejected_without_changing_decode_timeline(self):
        segment = SimpleNamespace(avg_logprob=-1.2, no_speech_prob=.3,
                                  words=[SimpleNamespace(start=190, end=205)])
        self.assertTrue(worker.unreliable_segment(segment))
        segment.avg_logprob = -.1
        self.assertFalse(worker.unreliable_segment(segment))

    def test_no_speech_language_detection_falls_back_without_trimming_audio(self):
        from unittest.mock import MagicMock
        model = MagicMock()
        model.detect_language.side_effect = [ValueError("need at least one array to concatenate"), ("en", .2, [])]
        model.transcribe.return_value = ([], SimpleNamespace(language="en", duration=5))
        audio = object()
        with patch.dict("sys.modules", {
            "faster_whisper": SimpleNamespace(WhisperModel=MagicMock(return_value=model)),
            "faster_whisper.audio": SimpleNamespace(decode_audio=MagicMock(return_value=audio)),
        }):
            self.assertEqual(worker.recognize(self.source, "unused", align=False)["text"], "")
        self.assertFalse(model.detect_language.call_args.kwargs["vad_filter"])
        self.assertIs(model.transcribe.call_args.args[0], audio)


if __name__ == "__main__":
    unittest.main()
