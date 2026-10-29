import os
import subprocess

try:
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], stderr=subprocess.DEVNULL
    ).strip().decode("utf-8")[-6:]
except (FileNotFoundError, subprocess.CalledProcessError):
    revision = os.environ.get("GRAVITYMON_BUILD_REV", "p002")[-6:]
print("-D CFG_GITREV='\"..%s\"'" % revision)
