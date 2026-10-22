import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest

spec = importlib.util.spec_from_file_location('alignment', Path(__file__).parents[1] / 'scripts/transcript-alignment.py')
alignment = importlib.util.module_from_spec(spec); spec.loader.exec_module(alignment)


class AlignmentTests(unittest.TestCase):
    def test_confident_timing_preserves_hurricane_text(self):
        words = [dict(word='hurricane', start=1, end=2)]
        spans = [SimpleNamespace(start=i, end=i+1, score=.9) for i in range(9)]
        self.assertEqual(alignment.apply_spans(words, ['HURRICANE'], spans, 1.2, .1), 1)
        self.assertEqual(words[0]['word'], 'hurricane')
        self.assertEqual((words[0]['start'], words[0]['end']), (1.2, 2.1))

    def test_low_confidence_keeps_whisper_timing(self):
        words = [dict(word='held', start=1, end=2)]
        spans = [SimpleNamespace(start=i, end=i+1, score=.1) for i in range(4)]
        self.assertEqual(alignment.apply_spans(words, ['HELD'], spans, 1.2, .1), 0)
        self.assertEqual((words[0]['start'], words[0]['end']), (1, 2))

    def test_partial_alignment_cannot_shift_other_words(self):
        words = [dict(word='hello', start=1, end=2)]
        self.assertEqual(alignment.apply_spans(words, ['HELLO'], [], 1, .1), 0)
        self.assertEqual(words[0]['start'], 1)

    def test_unsupported_numbers_skip_alignment(self):
        self.assertEqual(alignment.normalized_words([dict(word='2026')]), [])

    def test_conflicting_word_does_not_discard_independent_refinement(self):
        words = [dict(word='A', start=0, end=1), dict(word='B', start=1, end=2),
                 dict(word='C', start=3, end=4)]
        spans = [SimpleNamespace(start=0, end=15, score=.9),
                 SimpleNamespace(start=15, end=20, score=.1),
                 SimpleNamespace(start=32, end=38, score=.9)]
        self.assertEqual(alignment.apply_spans(words, ['A', 'B', 'C'], spans, 0, .1), 1)
        self.assertEqual(words[0]['end'], 1)
        self.assertEqual(words[2]['start'], 3.2)


if __name__ == '__main__':
    unittest.main()
