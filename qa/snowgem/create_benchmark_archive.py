import binascii
import calendar
import json
import plyvel
import progressbar
import os
import stat
import struct
import subprocess
import sys
import tarfile
import time

VIDULUM_CLI = './src/vidulum-cli'
USAGE = """
Requirements:
- find
- xz
- %s (edit VIDULUM_CLI in this script to alter the path)
- A running mainnet vidulumd using the default datadir with -txindex=1

Example usage:

make -C src/leveldb/
virtualenv venv
. venv/bin/activate
pip install --global-option=build_ext --global-option="-L$(pwd)/src/leveldb/" --global-option="-I$(pwd)/src/leveldb/include/" plyvel
pip install progressbar2
LD_LIBRARY_PATH=src/leveldb python qa/vidulum/create_benchmark_archive.py
""" % VIDULUM_CLI

def check_deps():
    if subprocess.call(['which', 'find', 'xz', VIDULUM_CLI], stdout=subprocess.PIPE):
        print USAGE
        sys.exit()

def encode_varint(n):
    v = bytearray()
    l = 0
    while True:
        v.append((n & 0x7F) | (0x80 if l else 0x00))
        if (n <= 0x7F):
            break
        n = (n >> 7) - 1
        l += 1
    return bytes(v)[::-1]

def decode_varint(v):
    n = 0
    for ch in range(len(v)):
        n = (n << 7) | (ord(v[ch]) & 0x7F)
        if (ord(v[ch]) & 0x80):
            n += 1
        else:
            return n

def compress_amount(n):
    if n == 0:
        return 0
    e = 0
    while (((n % 10) == 0) and e < 9):
        n /= 10
        e += 1
    if e < 9:
        d = (n % 10)
        assert(d >= 1 and d <= 9)
        n /= 10
        return 1 + (n*9 + d - 1)*10 + e
    else:
        return 1 + (n - 1)*10 + 9

OP_DUP = 0x76
OP_EQUAL = 0x87
OP_EQUALVERIFY = 0x88
OP_HASH160 = 0xa9
OP_CHECKSIG = 0xac
def to_key_id(script):
    if len(script) == 25 and \
            script[0] == OP_DUP and \
            script[1] == OP_HASH160 and \
            script[2] == 20 and \
            script[23] == OP_EQUALVERIFY and \
            script[24] == OP_CHECKSIG:
        return script[3:23]
    return bytes()

def to_script_id(script):
    if len(script) == 23 and \
            script[0] == OP_HASH160 and \
            script[1] == 20 and \
            script[22] == OP_EQUAL:
        return script[2:22]
    return bytes()

def to_pubkey(script):
    if len(script) == 35 and \
            script[0] == 33 and \
            script[34] == OP_CHECKSIG and \
            (script[1] == 0x02 or script[1] == 0x03):
        return script[1:34]
    if len(script) == 67 and \
            script[0] == 65 and \
            script[66] == OP_CHECKSIG and \
            script[1] == 0x04:
        return script[1:66] # assuming is fully valid
    return bytes()

def compress_script(script):
    result = bytearray()

    key_id = to_key_id(script)
    if key_id:
        result.append(0x00)
        result.extend(key_id)
        return bytes(result)

    script_id = to_script_id(script)
    if script_id:
        result.append(0x01)
        result.extend(script_id)
        return bytes(result)

    pubkey = to_pubkey(script)
    if pubkey:
        result.append(0x00)
        result.extend(pubkey[1:33])
        if pubkey[0] == 0x02 or pubkey[0] == 0x03:
            result[0] = pubkey[0]
            return bytes(result)
        elif pubkey[0] == 0x04:
            result[0] = 0x04 | (pubkey[64] & 0x01)
            return bytes(result)

    size = len(script) + 6
    result.append(encode_varint(size))
    result.extend(script)
    return bytes(result)

def deterministic_filter(tarinfo):
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = "root"
    tarinfo.mtime = calendar.timegm(time.strptime('2017-05-17', '%Y-%m-%d'))
    tarinfo.mode |= stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP
    tarinfo.mode &= ~stat.S_IWGRP
    if tarinfo.isdir():
        tarinfo.mode |= \
            stat.S_IXUSR | \
            stat.S_IXGRP | \
            stat.S_IXOTH
    else:
        tarinfo.mode &= \
            ~stat.S_IXUSR & \
            ~stat.S_IXGRP & \
            ~stat.S_IXOTH
    return tarinfo

def create_benchmark_archive(blk_hash):
    blk = json.loads(subprocess.check_output([VIDULUM_CLI, 'getblock', blk_hash]))
    print 'Height: %d' % blk['height']
    print 'Transactions: %d' % len(blk['tx'])

    os.mkdir('benchmark')
    with open('benchmark/block-%d.dat' % blk['height'], 'wb') as f:
        f.write(binascii.unhexlify(subprocess.check_output([VIDULUM_CLI, 'getblock', blk_hash, 'false']).strip(