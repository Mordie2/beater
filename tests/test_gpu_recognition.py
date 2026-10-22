"""Real GPU/large-v3 smoke test. Fails instead of silently accepting CPU fallback."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import wave

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--runtime', type=Path, required=True)
    args = p.parse_args()
    runtime = args.runtime.resolve()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as temporary:
        source = Path(temporary) / 'silence.wav'
        with wave.open(str(source), 'wb') as w:
            w.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
            w.writeframes(b'\0\0' * 16000 * 3)
        result = subprocess.run([str(runtime / 'python.exe'), str(root / 'scripts/transcript-worker.py'),
            '--input', str(source), '--model', str(runtime / 'models/whisper-large-v3'),
            '--device', 'cuda', '--language', 'en', '--force'], capture_output=True, text=True, timeout=240)
        if result.returncode:
            raise RuntimeError(result.stderr[-2000:] + result.stdout[-2000:])
        data = json.loads(source.with_suffix('.transcript.json').read_text(encoding='utf-8'))
        assert data['device'] == 'cuda' and data['compute_type'] == 'float16'
        assert data['text'] == '', 'Silent audio produced hallucinated lyrics'
        check = subprocess.run([str(runtime / 'python.exe'), '-c',
            "import sys,torch,torchaudio; m=torchaudio.models.wav2vec2_base(aux_num_out=29); "
            "m.load_state_dict(torch.load(sys.argv[1],map_location='cpu',weights_only=True)); print('Alignment weights validated')",
            str(runtime / 'models/alignment-en/wav2vec2-base-960h-alignment.pt')], check=True)
    print('Full large-v3 GPU FP16 inference and offline alignment weights passed.')


if __name__ == "__main__":
    main()
