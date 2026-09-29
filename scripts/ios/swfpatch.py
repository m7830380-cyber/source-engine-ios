#!/usr/bin/env python3
"""Replace the body of one ActionScript 2 function in a CS:GO .swf, in place.

The new body is padded with a jump to the end of the old one, so the function
keeps its size and nothing else in the movie moves (no tag lengths or branch
offsets to fix). Strings the movie's constant pool already has are pushed by
index, which keeps the new code short.

  swfpatch.py in.swf out.swf FunctionName "Target.Path.Method('string arg')"

The call form: dotted path, first part a variable (e.g. _global), the rest
members, last one the method; one optional string argument.
"""
import re
import struct
import sys
import zlib


def read_swf(path):
    data = open(path, 'rb').read()
    sig = data[:3]
    if sig == b'CWS':
        body = zlib.decompress(data[8:])
    elif sig == b'FWS':
        body = data[8:]
    else:
        raise SystemExit('not an uncompressed/zlib SWF: %r' % sig)
    return sig, data[3], bytearray(data[:8] + body)


def write_swf(path, sig, version, swf):
    total = len(swf)
    header = bytearray(sig + bytes([version]) + struct.pack('<I', total))
    if sig == b'CWS':
        out = header + zlib.compress(bytes(swf[8:]), 9)
    else:
        out = header + swf[8:]
    open(path, 'wb').write(out)


def tags(swf, start, end):
    """(code, body_start, body_end) for each tag between start and end."""
    pos = start
    while pos < end:
        code_len = struct.unpack_from('<H', swf, pos)[0]
        code, length = code_len >> 6, code_len & 0x3F
        pos += 2
        if length == 0x3F:
            length = struct.unpack_from('<I', swf, pos)[0]
            pos += 4
        yield code, pos, pos + length
        pos += length
        if code == 0:
            break


def action_blocks(swf, start, end):
    """Byte ranges of every action list (DoAction, DoInitAction, in sprites too)."""
    for code, b, e in tags(swf, start, end):
        if code == 12:          # DoAction
            yield b, e
        elif code == 59:        # DoInitAction: sprite id first
            yield b + 2, e
        elif code == 39:        # DefineSprite: id, frame count, then tags
            yield from action_blocks(swf, b + 4, e)


def cstr(swf, pos):
    end = swf.index(0, pos)
    return swf[pos:end].decode('latin-1'), end + 1


def patch(swf, func_name, call):
    m = re.fullmatch(r"([\w.]+)\((?:'([^']*)')?\)", call)
    if not m:
        raise SystemExit('call must look like A.B.Method(\'arg\'): %s' % call)
    path, arg = m.group(1).split('.'), m.group(2)

    # the header: RECT (bit packed), frame rate, frame count
    nbits = swf[8] >> 3
    tags_start = 8 + (5 + nbits * 4 + 7) // 8 + 4

    for b, e in action_blocks(swf, tags_start, len(swf)):
        pool = []
        pos = b
        while pos < e:
            op = swf[pos]
            if op == 0:
                break
            length = struct.unpack_from('<H', swf, pos + 1)[0] if op >= 0x80 else 0
            rec = pos + 3 if op >= 0x80 else pos + 1
            if op == 0x88:      # ConstantPool
                count = struct.unpack_from('<H', swf, rec)[0]
                p = rec + 2
                pool = []
                for _ in range(count):
                    s, p = cstr(swf, p)
                    pool.append(s)
            if op == 0x8E:      # DefineFunction2
                name, p = cstr(swf, rec)
                if name == func_name:
                    code_size = struct.unpack_from('<H', swf, rec + length - 2)[0]
                    body = rec + length
                    new = build_body(path, arg, pool)
                    if len(new) + 3 > code_size and len(new) != code_size:
                        raise SystemExit('%s: new body %d bytes, room for %d' % (func_name, len(new), code_size))
                    rest = code_size - len(new)
                    if rest:
                        # ActionJump over the rest of the old body
                        new += bytes([0x99]) + struct.pack('<H', 2) + struct.pack('<h', rest - 5)
                        new += bytes(rest - 5)
                    swf[body:body + code_size] = new
                    print('%s: %d-byte body replaced (%d used)' % (func_name, code_size, code_size - rest + (5 if rest else 0)))
                    return
            pos = rec + length
    raise SystemExit('function %s not found' % func_name)


def build_body(path, arg, pool):
    def push(*items):
        data = b''
        for kind, value in items:
            if kind == 'str':
                if value in pool and pool.index(value) < 256:
                    data += bytes([8, pool.index(value)])
                elif value in pool:
                    data += bytes([9]) + struct.pack('<H', pool.index(value))
                else:
                    data += bytes([0]) + value.encode('latin-1') + b'\0'
            elif kind == 'int':
                data += bytes([7]) + struct.pack('<i', value)
        return bytes([0x96]) + struct.pack('<H', len(data)) + data

    code = b''
    args = [('str', arg)] if arg is not None else []
    code += push(*(args + [('int', len(args)), ('str', path[0])]))
    code += bytes([0x1C])                   # GetVariable
    for member in path[1:-1]:
        code += push(('str', member)) + bytes([0x4E])   # GetMember
    code += push(('str', path[-1])) + bytes([0x52])     # CallMethod
    code += bytes([0x17])                   # Pop
    return code


if __name__ == '__main__':
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    sig, version, swf = read_swf(sys.argv[1])
    patch(swf, sys.argv[3], sys.argv[4])
    write_swf(sys.argv[2], sig, version, swf)
