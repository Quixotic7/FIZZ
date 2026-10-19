# SPDX-License-Identifier: GPL-3.0-only
# P1.3j: builds tools/test_remote.cpp with FM1VST's own compile flags and libraries (those of its remote_test target in
# an FM1VST build tree: FM1VST's shared code, fm1engine, remotefm1). Nothing in FM1VST is built or changed.
#     python3 build_remote.py FM1VST_BUILD_DIR SOURCE OUTPUT
import shlex, subprocess, sys, os
B = sys.argv[1]
SRC = sys.argv[2]
OUT = sys.argv[3]
flags = {}
for line in open(B + '/CMakeFiles/remote_test.dir/flags.make'):
    if ' = ' in line and not line.startswith('#'):
        k, v = line.rstrip('\n').split(' = ', 1)
        flags[k] = v
sdk = '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX27.0.sdk'
cc = ['c++', '-O1', '-std=gnu++17', '-arch', 'arm64', '-isysroot', sdk] + shlex.split(flags['CXX_DEFINES']) + \
     shlex.split(flags['CXX_INCLUDES']) + ['-c', SRC, '-o', OUT + '.o']
r = subprocess.run(cc, cwd=B, capture_output=True, text=True)
if r.returncode:
    print(r.stderr[-4000:])
    sys.exit(1)
link = shlex.split(open(B + '/CMakeFiles/remote_test.dir/link.txt').read())
link = [OUT + '.o' if a == 'CMakeFiles/remote_test.dir/tests/remote_test.cpp.o' else a for a in link]
i = link.index('-o')
link[i + 1] = OUT
r = subprocess.run(link, cwd=B, capture_output=True, text=True)
if r.returncode:
    print(r.stderr[-4000:])
    sys.exit(1)
print('built', OUT)
