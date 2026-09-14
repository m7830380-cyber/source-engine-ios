#!/usr/bin/env python
# Generate TF/GC protobuf C++ sources with protobuf 2.6.1.
from __future__ import print_function
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'game', 'shared', 'generated_proto')
PROTOC = os.environ.get('PROTOC') or os.path.join(os.environ.get('TEMP', '/tmp'), 'protoc-261', 'protoc.exe')

def gen(input_dir, proto_name):
    src = os.path.join(input_dir, proto_name)
    cmd = [
        PROTOC,
        '--proto_path=' + input_dir,
        '--proto_path=' + os.path.join(ROOT, 'thirdparty', 'protobuf-2.6.1', 'src'),
        '--proto_path=' + os.path.join(ROOT, 'gcsdk'),
        '--proto_path=' + os.path.join(ROOT, 'game', 'shared'),
        '--proto_path=' + os.path.join(ROOT, 'game', 'shared', 'econ'),
        '--proto_path=' + os.path.join(ROOT, 'game', 'shared', 'tf'),
        '--cpp_out=' + OUT,
        src,
    ]
    print('GEN', src)
    subprocess.check_call(cmd)

def main():
    if not os.path.isfile(PROTOC):
        sys.exit('protoc not found: %s' % PROTOC)
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    gen(os.path.join(ROOT, 'gcsdk'), 'steammessages.proto')
    gen(os.path.join(ROOT, 'gcsdk'), 'gcsystemmsgs.proto')
    gen(os.path.join(ROOT, 'gcsdk'), 'gcsdk_gcmessages.proto')
    gen(os.path.join(ROOT, 'game', 'shared'), 'base_gcmessages.proto')
    gen(os.path.join(ROOT, 'game', 'shared', 'econ'), 'econ_gcmessages.proto')
    gen(os.path.join(ROOT, 'game', 'shared', 'tf'), 'tf_gcmessages.proto')
    for name in os.listdir(OUT):
        print('OUT', name)

if __name__ == '__main__':
    main()
