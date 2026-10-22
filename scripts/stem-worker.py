"""Local, resumable vocal extraction. One JSON event per stdout line."""
import argparse
import filecmp
import hashlib
import json
import os
from pathlib import Path
import sys
import uuid

VERSION = "htdemucs-4.0.1-v1"


def event(state, **fields):
    print(json.dumps(dict(state=state, **fields)), flush=True)


def atomic_json(path, value):
    temp = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        temp.write_text(json.dumps(value, indent=2), encoding="utf-8")
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def read_manifest(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def valid_vocals(path):
    import soundfile as sf
    try:
        info = sf.info(str(path))
        return info.frames > 0 and info.channels == 2 and info.samplerate == 44100
    except (OSError, RuntimeError):
        return False


def probe_cache(source, folder):
    """Validate a completed cache without copying audio or queuing inference."""
    previous = read_manifest(Path(folder) / "manifest.json")
    if previous.get("version") != VERSION or not previous.get("vocals"):
        return None
    source = Path(source)
    if not source.is_file():
        source = Path(previous.get("original", ""))
    try:
        before = source.stat()
        with source.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        after = source.stat()
        if ((before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns)
                or digest != previous.get("sha256") or not valid_vocals(previous["vocals"])):
            return None
        return previous
    except OSError:
        return None


def separate_audio(source, destination):
    import numpy as np
    import soundfile as sf
    import torch
    from demucs.apply import apply_model
    from demucs.audio import convert_audio
    from demucs.pretrained import get_model

    # Reserve CPU capacity for playback. CPU mode works without GPU drivers.
    torch.set_num_threads(max(1, min(4, (os.cpu_count() or 2) // 2)))
    torch.manual_seed(0)
    model = get_model("htdemucs").cpu().eval()
    audio, rate = sf.read(str(source), dtype="float32", always_2d=True)
    if not len(audio) or not np.isfinite(audio).all():
        raise ValueError("The audio file is empty or contains invalid samples")
    wav = convert_audio(torch.from_numpy(audio.T.copy()), rate, 44100, 2)
    reference = wav.mean(0)
    mean, std = reference.mean(), reference.std()
    if std < 1e-8:
        vocals = torch.zeros_like(wav)
    else:
        with torch.inference_mode():
            result = apply_model(model, ((wav - mean) / std)[None], device="cpu",
                                 shifts=0, split=True, overlap=0.25, progress=True, num_workers=0)
        vocals = result[0, model.sources.index("vocals")] * std + mean
    # Float WAV preserves dynamics and original timing for downstream transcription.
    sf.write(str(destination), vocals.T.numpy(), 44100, subtype="FLOAT", format="WAV")


def prepare(source, folder):
    source, folder = Path(source), Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    manifest_path = folder / "manifest.json"
    previous = read_manifest(manifest_path)
    if not source.is_file():
        source = Path(previous.get("original", ""))
        if not source.is_file():
            raise FileNotFoundError("Original track and cached copy are unavailable")
    event("caching")
    before = source.stat()
    temporary = folder / ("copy-" + uuid.uuid4().hex + ".tmp")
    try:
        digest = hashlib.sha256()
        with source.open("rb") as inp, temporary.open("wb") as out:
            while chunk := inp.read(1024 * 1024):
                digest.update(chunk)
                out.write(chunk)
        after = source.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise RuntimeError("Track changed while being cached; retry once the file is saved")
        track_dir = folder / digest.hexdigest()
        track_dir.mkdir(exist_ok=True)
        original = track_dir / ("original" + source.suffix.lower())
        # Leave an intact cached file in place, including while another deck
        # is reading it. Repair a damaged cache from the freshly hashed copy.
        if not original.is_file() or not filecmp.cmp(temporary, original, shallow=False):
            os.replace(temporary, original)
        vocal = track_dir / (VERSION + "-vocals.wav")
        data = dict(version=VERSION, sha256=digest.hexdigest(), original=str(original.resolve()),
                    source=str(source.resolve()), size=before.st_size,
                    modified_ms=before.st_mtime_ns // 1000000, vocals="")
        atomic_json(manifest_path, data)
        event("cached", original=data["original"])
        hit = valid_vocals(vocal)
        if not hit:
            event("separating")
            pending = track_dir / ("vocals-" + uuid.uuid4().hex + ".tmp.wav")
            try:
                separate_audio(original, pending)
                if not valid_vocals(pending):
                    raise RuntimeError("Separation did not produce a valid vocal WAV")
                os.replace(pending, vocal)
            finally:
                pending.unlink(missing_ok=True)
        data["vocals"] = str(vocal.resolve())
        atomic_json(manifest_path, data)
        event("ready", vocals=data["vocals"], cached=hit)
        return data
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--cache", required=True)
    parser.add_argument("--probe", action="store_true")
    args = parser.parse_args()
    try:
        if args.probe:
            cached = probe_cache(args.input, args.cache)
            if cached:
                event("ready", vocals=cached["vocals"], cached=True)
            else:
                event("miss")
        else:
            prepare(args.input, args.cache)
    except Exception as exc:
        event("error", message=str(exc))
        sys.exit(1)
