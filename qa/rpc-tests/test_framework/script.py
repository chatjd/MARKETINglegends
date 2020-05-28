#
# script.py
#
# This file is modified from python-bitcoinlib.
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

"""Scripts

Functionality to build scripts, as well as SignatureHash().
"""

from __future__ import absolute_import, division, print_function, unicode_literals

from test_framework.mininode import CTransaction, CTxOut, hash256

import sys
bchr = chr
bord = ord
if sys.version > '3':
    long = int
    bchr = lambda x: bytes([x])
    bord = lambda x: x

import struct
import binascii

from test_framework import bignum

MAX_SCRIPT_SIZE = 10000
MAX_SCRIPT_ELEMENT_SIZE = 520
MAX_SCRIPT_OPCODES = 201

OPCODE_NAMES = {}

_opcode_instances = []
class CScriptOp(int):
    """A single script opcode"""
    __slots__ = []

    @staticmethod
    def encode_op_pushdata(d):
        """Encode a PUSHDATA op, returning bytes"""
        if len(d) < 0x4c:
            return b'' + bchr(len(d)) + d # OP_PUSHDATA
        elif len(d) <= 0xff:
            return b'\x4c' + bchr(len(d)) + d # OP_PUSHDATA1
        elif len(d) <= 0xffff:
            return b'\x4d' + struct.pack(b'<H', len(d)) + d # OP_PUSHDATA2
        elif len(d) <= 0xffffffff:
            return b'\x4e' + struct.pack(b'<I', len(d)) + d # OP_PUSHDATA4
        else:
            raise ValueError("Data too long to encode in a PUSHDATA op")

    @staticmethod
    def encode_op_n(n):
        """Encode a small integer op, returning an opcode"""
        if not (0 <= n <= 16):
            raise ValueError('Integer must be in range 0 <= n <= 16, got %d' % n)

        if n == 0:
            return OP_0
        else:
            return CScriptOp(OP_1 + n-1)

    def decode_op_n(self):
        """Decode a small integer opcode, returning an integer"""
        if self == OP_0:
            return 0

        if not (self == OP_0 or OP_1 <= self <= OP_16):
            raise ValueError('op %r is not an OP_N' % self)

        return int(self - OP_1+1)

    def is_small_int(self):
        """Return true if the op pushes a small integer to the stack"""
        if 0x51 <= self <= 0x60 or self == 0:
            return True
        else:
            return False

    def __str__(self):
        return repr(self)

    def __repr__(self):
        if self in OPCODE_NAMES:
            return OPCODE_NAMES[self]
        else:
            return 'CScriptOp(0x%x)' % self

    def __new__(cls, n):
        try:
            return _opcode_instances[n]
        except IndexError:
            assert len(_opcode_instances) == n
            _opcode_instances.append(super(CScriptOp, cls).__new__(cls, n))
            return _opcode_instances[n]

# Populate opcode instance table
for n in range(0xff+1):
    CScriptOp(n)


# push value
OP_0 = CScriptOp(0x00)
OP_FALSE = OP_0
OP_PUSHDATA1 = CScriptOp(0x4c)
OP_PUSHDATA2 = CScriptOp(0x4d)
OP_PUSHDATA4 = CScriptOp(0x4e)
OP_1NEGATE = CScriptOp(0x4f)
OP_RESERVED = CScriptOp(0x50)
OP_1 = CScriptOp(0x51)
OP_TRUE=OP_1
OP_2 = CScriptOp(0x52)
OP_3 = CScriptOp(0x53)
OP_4 = CScriptOp(0x54)
OP_5 = CScriptOp(0x55)
OP_6 = CScriptOp(0x56)
OP_7 = CScriptOp(0x57)
OP_8 = CScriptOp(0x58)
OP_9 = CScriptOp(0x59)
OP_10 = CScriptOp(0x5a)
OP_11 = CScriptOp(0x5b)
OP_12 = CScriptOp(0x5c)
OP_13 =