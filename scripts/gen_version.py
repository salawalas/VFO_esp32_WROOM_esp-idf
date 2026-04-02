import os
import subprocess
import datetime

Import("env")

version = os.environ.get("VERSION", "2.1")

try:
    git_commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"]
    ).decode().strip()
except:
    git_commit = "nogit"

build_date = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")

content = f'''
#pragma once

#define FW_NAME    "ESP32 VFO"
#define FW_AUTHOR  "Marcin"
#define FW_VERSION "{version}"
#define FW_COMMIT  "{git_commit}"
#define BUILD_DATE "{build_date}"
'''

with open("include/version.h", "w") as f:
    f.write(content)