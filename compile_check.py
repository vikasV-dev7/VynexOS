import subprocess
import os

try:
    res = subprocess.run(['cl', '/c', 'src/apps/desktop_shell.cpp', '/Iinclude', '/I.', '/std:c++20'], capture_output=True, text=True)
    print("STDOUT:", res.stdout)
    print("STDERR:", res.stderr)
except Exception as e:
    print(e)
