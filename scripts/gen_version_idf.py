import os
import subprocess
import datetime

def run(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode().strip()
    except:
        return None

# --- VERSION (git describe) ---
git_desc = run(["git", "describe", "--tags", "--dirty", "--always"])
if not git_desc:
    git_desc = "nogit"
elif git_desc.endswith("-dirty"):
    git_desc = git_desc[:-6]

# --- COMMIT (short hash) ---
git_commit = run(["git", "rev-parse", "--short", "HEAD"])
if not git_commit:
    git_commit = "nogit"

# --- BUILD NUMBER ---
build_file = ".build_number"

try:
    with open(build_file, "r") as f:
        build_num = int(f.read().strip())
except:
    build_num = 0

build_num += 1

with open(build_file, "w") as f:
    f.write(str(build_num))

# --- DATE ---
build_date = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")

# --- TYPE ---
build_type = os.environ.get("BUILD_TYPE", "Release")

# --- OUTPUT ---
content = f'''
#pragma once

#define FW_NAME      "ESP32 VFO"
#define FW_AUTHOR    "Marcin"
#define FW_VERSION   "{git_desc}"
#define FW_COMMIT    "{git_commit}"
#define FW_BUILD     {build_num}
#define BUILD_DATE   "{build_date}"
#define BUILD_TYPE   "{build_type}"
'''

os.makedirs("include", exist_ok=True)

with open("include/version.h", "w") as f:
    f.write(content)
