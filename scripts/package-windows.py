"""Stream the portable package to ZIP without buffering the ML runtime in RAM."""
from pathlib import Path
import os
import sys
import zipfile

folder, target = (Path(p).resolve() for p in sys.argv[1:])
pending = target.with_suffix(".partial.zip")
with zipfile.ZipFile(pending, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=3) as archive:
    for path in sorted(folder.rglob("*")):
        if path.is_file() and "__pycache__" not in path.parts:
            archive.write(path, path.relative_to(folder.parent).as_posix())
os.replace(pending, target)
print(f"Ready: {target} ({target.stat().st_size / 1024**2:.1f} MiB)")
