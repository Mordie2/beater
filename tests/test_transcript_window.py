"""Windows integration smoke test; pass package directory and an existing vocal WAV.

Use a vocal with a prepared transcript cache. Leaves screenshots in build-windows.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

def main():
    package, vocal = (Path(p).resolve() for p in sys.argv[1:])
    output = package.parent
    with tempfile.TemporaryDirectory() as temp:
        session = Path(temp) / "session.json"

        def write(generation, title, stage, audio="", show_revision=1):
            pending = session.with_suffix(".tmp")
            pending.write_text(json.dumps(dict(generation=generation, title=title, stage=stage,
                                              status="Waiting for vocal extraction", vocals=audio, deck="A",
                                              showRevision=show_revision)), encoding="utf-8")
            os.replace(pending, session)

        for name in ("ready", "switch", "minimized"):
            prefix = output / ("transcript-test-" + name)
            screenshot = Path(str(prefix) + "-A.png")
            state = Path(str(prefix) + "-A.json")
            screenshot.unlink(missing_ok=True)
            state.unlink(missing_ok=True)
            write(1, "Vocal transcription test", "ready", str(vocal))
            env = dict(os.environ, BEATER_TRANSCRIPT_SNAPSHOT=str(prefix),
                       BEATER_TRANSCRIPT_SNAPSHOT_MS="4000", QT_FORCE_STDERR_LOGGING="1")
            if name == "minimized":
                env["BEATER_TRANSCRIPT_TEST_MINIMIZE_MS"] = "500"
            with (output / ("transcript-test-" + name + ".log")).open("w") as log:
                process = subprocess.Popen([str(package / "beater-transcript.exe"), "--track-session", str(session)],
                                           env=env, stdout=log, stderr=log)
                try:
                    if name == "switch":
                        time.sleep(1.5)
                        write(2, "New track waiting for vocals", "waiting", show_revision=2)
                    elif name == "minimized":
                        time.sleep(1.5)
                        write(1, "Vocal transcription test", "ready", str(vocal), show_revision=2)
                    deadline = time.monotonic() + 10
                    while not state.exists() and time.monotonic() < deadline:
                        assert process.poll() is None, "Test window exited unexpectedly"
                        time.sleep(0.1)
                    assert screenshot.exists(), "Test window did not render"
                    info = json.loads(state.read_text())
                    assert info["visible"] and not info["minimized"], "Deck A window was not restored"
                    if name == "switch":
                        assert info["wordCount"] == 0 and info["title"] == "New track waiting for vocals"
                    else:
                        assert info["wordCount"] > 0, "Prepared transcript did not appear"
                    write(3, "Closing", "closed")
                    assert process.wait(timeout=5) == 0, "Test window failed to close"
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
            print(name + ": rendered and closed cleanly")


if __name__ == "__main__":
    main()
