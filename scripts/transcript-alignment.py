"""Conservative, offline English CTC timing refinement using TorchAudio 2.5."""
import math
import unicodedata
from pathlib import Path

CHECKPOINT = "wav2vec2-base-960h-alignment.pt"


def normalized_words(words):
    result = []
    for word in words:
        text = unicodedata.normalize("NFKD", word["word"].replace("’", "'")).upper()
        # Numbers and non-English letters need language-specific normalization.
        if any(c.isdigit() or (c.isalpha() and not ('A' <= c <= 'Z')) for c in text):
            return []
        cleaned = ''.join(c for c in text if 'A' <= c <= 'Z' or c == "'")
        if not cleaned:
            return []
        result.append(cleaned)
    return result


def keep_word_order(replacements, originals):
    # Keep independent good refinements if a neighbour cannot be aligned.
    # Revert conflicting changes until the original word order is preserved.
    while True:
        reverted = False
        for i, (a, b) in enumerate(zip(replacements, replacements[1:])):
            if a['end'] <= b['start'] + .02:
                continue
            for j in (i, i + 1):
                if replacements[j] != originals[j]:
                    replacements[j] = dict(originals[j])
                    reverted = True
        if not reverted:
            break
    return replacements


def apply_spans(words, tokens, spans, start, frame_seconds):
    """Accept only complete, confident, ordered refinements; never change text."""
    if len(spans) != sum(map(len, tokens)):
        return 0
    replacements = []
    offset = 0
    for word, token in zip(words, tokens):
        chars = spans[offset:offset + len(token)]
        offset += len(token)
        score = sum(float(c.score) for c in chars) / len(chars)
        begin = start + chars[0].start * frame_seconds
        end = start + chars[-1].end * frame_seconds
        if not (math.isfinite(score) and math.isfinite(begin) and math.isfinite(end) and score >= .35 and end > begin):
            replacements.append(dict(word))
            continue
        replacements.append(dict(word, start=round(begin, 3), end=round(end, 3),
                                 timing="ctc", alignment_score=round(score, 3)))
    keep_word_order(replacements, words)
    words[:] = replacements
    return sum(w.get('timing') == 'ctc' for w in replacements)


def refine(audio, segments, language, models_dir, event):
    if language != 'en':
        return dict(method="whisper", refined_words=0, reason="Alignment model is English-only")
    checkpoint = Path(models_dir) / 'alignment-en' / CHECKPOINT
    if not checkpoint.is_file():
        return dict(method="whisper", refined_words=0, reason="Alignment model is not installed")
    import torch
    import torchaudio
    from torchaudio import functional as af
    torch.set_num_threads(2)
    bundle = torchaudio.pipelines.WAV2VEC2_ASR_BASE_960H
    # Construct the pinned 2.5 architecture locally; get_model() would download on a miss.
    model = torchaudio.models.wav2vec2_base(aux_num_out=len(bundle.get_labels()))
    model.load_state_dict(torch.load(checkpoint, map_location='cpu', weights_only=True))
    model.eval()
    labels = {c: i for i, c in enumerate(bundle.get_labels())}
    originals = [dict(word) for segment in segments for word in segment.get('words', [])]
    event('aligning', message='Refining English word timing')
    for index, segment in enumerate(segments):
        words = segment.get('words', [])
        tokens = normalized_words(words)
        start = max(0, segment['start'] - .3)
        end = min(len(audio) / 16000, segment['end'] + .3)
        if not tokens or not .1 < end - start <= 35:
            continue
        chunk = torch.from_numpy(audio[int(start * 16000):int(end * 16000)].copy())[None]
        target_ids = [labels[c] for c in '|'.join(tokens)]
        try:
            with torch.inference_mode():
                emission, _ = model(chunk)
                emission = emission.log_softmax(-1)
                targets = torch.tensor([target_ids], dtype=torch.int32)
                path, scores = af.forced_align(emission, targets, blank=0)
                spans = af.merge_tokens(path[0], scores[0].exp(), blank=0)
            if [s.token for s in spans] != target_ids:
                continue
            spans = [s for s in spans if s.token != labels['|']]
            apply_spans(words, tokens, spans, start, chunk.shape[1] / 16000 / emission.shape[1])
        except (RuntimeError, ValueError):
            # Unalignable singing keeps its original Whisper timestamps.
            continue
        event('aligning', completed=index + 1, total=len(segments))
    words = keep_word_order([word for segment in segments for word in segment.get('words', [])], originals)
    offset = 0
    for segment in segments:
        size = len(segment.get('words', []))
        segment['words'] = words[offset:offset + size]
        offset += size
    return dict(method="ctc-en-with-whisper-fallback", refined_words=sum(w.get('timing') == 'ctc' for w in words))


def watch_parent(pid):
    """Stop CPU work immediately if the app cancels/crashes the owning worker."""
    import os
    import sys
    import threading
    def wait():
        if sys.platform == 'win32':
            import ctypes
            kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
            kernel.OpenProcess.restype = ctypes.c_void_p
            kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
            handle = kernel.OpenProcess(0x00100000, False, pid)
            if handle:
                kernel.WaitForSingleObject(handle, 0xFFFFFFFF)
        else:
            import time
            while os.getppid() == pid:
                time.sleep(.25)
        os._exit(1)
    threading.Thread(target=wait, daemon=True).start()


if __name__ == '__main__':
    import argparse
    import json
    parser = argparse.ArgumentParser()
    parser.add_argument('--job', type=Path, required=True)
    parser.add_argument('--parent-pid', type=int, required=True)
    args = parser.parse_args()
    watch_parent(args.parent_pid)
    import numpy as np
    data = json.loads((args.job / 'input.json').read_text(encoding='utf-8'))
    audio = np.load(args.job / 'audio.npy', allow_pickle=False)
    alignment = refine(audio, data['segments'], data['language'], Path(data['models']),
                       lambda state, **values: print(json.dumps(dict(state=state, **values)), flush=True))
    (args.job / 'output.json').write_text(json.dumps(dict(segments=data['segments'], alignment=alignment)),
                                        encoding='utf-8')
