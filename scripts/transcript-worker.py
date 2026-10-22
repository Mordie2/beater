"""Local original-language transcription with streaming JSON events and disk cache."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import uuid
import gc
import subprocess
import tempfile
import time
from contextlib import contextmanager

VERSION = "large-v3-fp16-ctc-en-v2"
DLL_HANDLES = []


def configure_gpu(runtime):
    """Register app-local DLLs without changing the machine's CUDA installation."""
    gpu = Path(runtime) / 'gpu'
    if sys.platform != 'win32':
        return True
    if not (gpu / 'cublas64_12.dll').exists() or not (gpu / 'cudnn64_9.dll').exists():
        return False
    os.environ['PATH'] = str(gpu) + os.pathsep + os.environ.get('PATH', '')
    DLL_HANDLES.append(os.add_dll_directory(str(gpu.resolve())))
    import ctypes
    for name in ('cublasLt64_12.dll', 'cublas64_12.dll', 'cudnn64_9.dll'):
        DLL_HANDLES.append(ctypes.CDLL(str((gpu / name).resolve())))
    return True


def select_device(requested, gpu_libraries, cuda_count):
    if requested == 'cpu':
        return 'cpu'
    if gpu_libraries and cuda_count > 0:
        return 'cuda'
    if requested == 'cuda':
        raise RuntimeError('NVIDIA GPU or bundled CUDA libraries are unavailable')
    return 'cpu'


@contextmanager
def recognition_lock():
    # OS releases this lock when a worker is killed or crashes. Both decks share it.
    path = Path(os.environ.get('BEATER_TRANSCRIPT_LOCK', str(Path(tempfile.gettempdir()) / 'beater-recognition.lock')))
    with path.open('a+b') as lock:
        if lock.tell() == 0:
            lock.write(b'0'); lock.flush()
        announced = False
        while True:
            try:
                lock.seek(0)
                if sys.platform == 'win32':
                    import msvcrt
                    msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except OSError:
                if not announced:
                    event('queued'); announced = True
                time.sleep(.25)
        try:
            yield
        finally:
            lock.seek(0)
            if sys.platform == 'win32':
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock, fcntl.LOCK_UN)


def model_identity(model_dir):
    path = Path(model_dir) / 'model.bin'
    if not path.exists():
        return str(Path(model_dir).name)
    info = path.stat()
    return f'{Path(model_dir).name}:{info.st_size}:{int(info.st_mtime)}'


def alignment_identity(model_dir):
    path = Path(model_dir).parent / 'alignment-en' / 'wav2vec2-base-960h-alignment.pt'
    if not path.exists():
        return 'missing'
    info = path.stat()
    return f'{info.st_size}:{int(info.st_mtime)}'


def event(state, **values):
    print(json.dumps(dict(state=state, **values)), flush=True)


def atomic_write(path, text):
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        temporary.write_text(text, encoding="utf-8")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def has_audio_signal(audio, start, end):
    # Reject words invented over silence/separation residue, relative to this
    # recording's level. No speech detection or time removal is involved.
    chunk = audio[max(0, int((start - .1) * 16000)):max(0, int((end + .1) * 16000))]
    floor = max(1e-7, float((audio ** 2).mean()) ** .5 * .01)
    return bool(chunk.size and float((chunk ** 2).mean()) ** .5 > floor)


def unreliable_segment(segment):
    # Filter after decoding so a suspect instrumental passage cannot change
    # Whisper's seek position and skip real lyrics in the next phrase.
    confidence = getattr(segment, 'avg_logprob', 0)
    if getattr(segment, 'no_speech_prob', 0) > .85 and confidence < -.5:
        return True
    durations = [w.end - w.start for w in (segment.words or [])]
    return confidence < -1.0 and (sum(max(0, d - 2) for d in durations) >= 3
                                  or sum(d <= .02 for d in durations) >= 2)


def refine_timing(audio, segments, language, models_dir):
    # Keep TorchAudio and CTranslate2's native libraries in separate processes.
    # A native alignment failure must not discard a completed transcription.
    import numpy as np
    with tempfile.TemporaryDirectory(prefix='beater-alignment-') as directory:
        folder = Path(directory)
        np.save(folder / 'audio.npy', audio)
        (folder / 'input.json').write_text(json.dumps(dict(segments=segments, language=language,
            models=str(Path(models_dir).resolve()))), encoding='utf-8')
        with (folder / 'error.log').open('w', encoding='utf-8') as errors:
            process = subprocess.Popen([sys.executable, str(Path(__file__).with_name('transcript-alignment.py')),
                '--job', str(folder), '--parent-pid', str(os.getpid())],
                stdout=subprocess.PIPE, stderr=errors, text=True, encoding='utf-8')
            try:
                for line in process.stdout:
                    update = json.loads(line)
                    if update.get('state') == 'aligning':
                        event(**update)
                code = process.wait()
                if code:
                    raise RuntimeError(f'Word timing process exited ({code}); keeping Whisper timing')
                data = json.loads((folder / 'output.json').read_text(encoding='utf-8'))
                segments[:] = data['segments']
                return data['alignment']
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                process.stdout.close()


def recognize(source, model_dir, device='cpu', language='auto', align=True):
    from faster_whisper import WhisperModel
    from faster_whisper.audio import decode_audio
    compute_type = 'float16' if device == 'cuda' else 'int8'
    event("loading", device=device, model='Whisper large-v3', compute_type=compute_type)
    model = WhisperModel(str(model_dir), device=device, compute_type=compute_type,
                         cpu_threads=2, num_workers=1, local_files_only=True)
    audio = decode_audio(str(source), sampling_rate=16000)
    # An instrumental intro is a poor language sample. VAD is used only for
    # language detection; the unfiltered audio below retains every sung passage.
    probability = 1.0
    if language == 'auto':
        try:
            language, probability, _ = model.detect_language(
                audio=audio, vad_filter=True, language_detection_segments=3,
                language_detection_threshold=0.8)
        except ValueError:
            language, probability, _ = model.detect_language(
                audio=audio, vad_filter=False, language_detection_segments=3,
                language_detection_threshold=0.8)
    segments, info = model.transcribe(audio, task="transcribe", language=language, beam_size=5,
                                      # Speech VAD can reject sustained/quiet singing.
                                      # Keep the complete source timeline, including intros.
                                      vad_filter=False, condition_on_previous_text=False,
                                      max_initial_timestamp=30.0, word_timestamps=True)
    event("transcribing", language=info.language, duration=info.duration)
    result = []
    for segment in segments:
        text = segment.text.strip()
        if not text or not has_audio_signal(audio, segment.start, segment.end):
            continue
        if unreliable_segment(segment):
            continue
        item = dict(start=segment.start, end=segment.end, text=text,
                    words=[dict(start=w.start, end=w.end, word=w.word, probability=w.probability)
                           for w in (segment.words or [])])
        if hasattr(segment, 'no_speech_prob'):
            item['no_speech_probability'] = segment.no_speech_prob
            item['average_log_probability'] = segment.avg_logprob
        result.append(item)
        event("segment", **item)
    detected_language = info.language
    del segments, model
    gc.collect()
    alignment = dict(method='whisper', refined_words=0)
    if align and result:
        try:
            alignment = refine_timing(audio, result, detected_language, Path(model_dir).parent)
        except Exception as exc:
            alignment['reason'] = str(exc)[:240]
            event('notice', message='Word refinement unavailable; using Whisper timestamps')
    return dict(language=detected_language, language_probability=float(probability), segments=result,
                device=device, compute_type=compute_type, model='Whisper large-v3', alignment=alignment,
                text="\n\n".join(s["text"] for s in result))


def transcribe(source, model_dir, force=False, language='auto', device='auto', align=True):
    source = Path(source)
    with source.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    cache = source.with_suffix(".transcript.json")
    text_file = source.with_suffix(".transcript.txt")
    settings = dict(model=model_identity(model_dir), language=language, align=align,
                    alignment_model=alignment_identity(model_dir) if align else 'disabled')
    def cached_result():
        if force:
            return None
        try:
            data = json.loads(cache.read_text(encoding="utf-8"))
            if (data.get("version") == VERSION and data.get("settings") == settings and data.get("sha256") == digest
                    and isinstance(data.get("text"), str) and isinstance(data.get("segments"), list)
                    and isinstance(data.get("language"), str)):
                atomic_write(text_file, data["text"])
                event("ready", cached=True, **data)
                return data
        except (ValueError, OSError):
            pass
        return None
    data = cached_result()
    if data is not None:
        return data
    with recognition_lock():
        data = cached_result()
        if data is not None:
            return data
        gpu_libraries = False
        if device != 'cpu':
            try:
                gpu_libraries = configure_gpu(Path(model_dir).parent.parent)
            except OSError:
                pass
        cuda_count = 0
        if device != 'cpu':
            try:
                import ctranslate2
                cuda_count = ctranslate2.get_cuda_device_count()
            except (RuntimeError, OSError):
                pass
        selected = select_device(device, gpu_libraries, cuda_count)
        retry_cpu = False
        try:
            data = recognize(source, model_dir, selected, language, align)
        except Exception:
            if selected != 'cuda' or device == 'cuda':
                raise
            retry_cpu = True
        if retry_cpu:
            # Leave the exception handler first so its traceback releases the GPU model.
            gc.collect()
            event('reset', message='GPU recognition unavailable; retrying the same model on CPU')
            data = recognize(source, model_dir, 'cpu', language, align)
        data.update(version=VERSION, sha256=digest, settings=settings)
        # Publish while still holding the lock so a second deck can reuse the result.
        atomic_write(text_file, data["text"])
        atomic_write(cache, json.dumps(data, ensure_ascii=False, indent=2))
    # Publish only complete transcripts. A failed retry keeps the previous one.
    event("ready", cached=False, **data)
    return data


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--force", action="store_true")
    parser.add_argument('--device', choices=['auto', 'cuda', 'cpu'], default='auto')
    parser.add_argument('--language', default='auto')
    parser.add_argument('--no-align', action='store_true')
    args = parser.parse_args()
    try:
        transcribe(args.input, args.model, args.force, args.language, args.device, not args.no_align)
    except Exception as exc:
        event("error", message=str(exc))
        sys.exit(1)
