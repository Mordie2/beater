"""One-time model downloads. Runtime recognition itself never downloads models."""
import hashlib
import json
from pathlib import Path
import sys
from urllib.request import urlretrieve

from huggingface_hub import HfApi, snapshot_download
import torch
import torchaudio

root = Path(sys.argv[1]).resolve()
folder = root / 'whisper-large-v3'
revision = HfApi().model_info('Systran/faster-whisper-large-v3').sha
snapshot_download('Systran/faster-whisper-large-v3', revision=revision, local_dir=folder,
                  allow_patterns=['config.json', 'model.bin', 'tokenizer.json',
                                  'vocabulary.json', 'preprocessor_config.json', 'README.md'])
alignment = root / 'alignment-en' / 'wav2vec2-base-960h-alignment.pt'
alignment.parent.mkdir(parents=True, exist_ok=True)
if not alignment.exists():
    model = torchaudio.pipelines.WAV2VEC2_ASR_BASE_960H.get_model(
        dl_kwargs={'model_dir': 'build-tools/alignment-download', 'progress': True})
    pending = alignment.with_suffix('.partial')
    torch.save(model.state_dict(), pending)
    pending.replace(alignment)
    del model
urlretrieve('https://raw.githubusercontent.com/openai/whisper/main/LICENSE', folder / 'LICENSE.txt')
urlretrieve('https://raw.githubusercontent.com/pytorch/fairseq/ce6c9eeae163ac04b79539c78e74f292f29eaa18/LICENSE',
            alignment.parent / 'LICENSE.txt')
with (folder / 'model.bin').open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
(folder / 'beater-model.json').write_text(json.dumps({
    'repository': 'Systran/faster-whisper-large-v3', 'revision': revision, 'sha256': digest,
    'alignment': 'torchaudio WAV2VEC2_ASR_BASE_960H',
}, indent=2), encoding='utf-8')
print('Full large-v3 and English alignment models prepared.')
