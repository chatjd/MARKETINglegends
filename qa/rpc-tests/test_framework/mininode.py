# mininode.py - Bitcoin P2P network half-a-node
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# This python code was modified from ArtForz' public domain  half-a-node, as
# found in the mini-node branch of http://github.com/jgarzik/pynode.
#
# NodeConn: an object which manages p2p connectivity to a bitcoin node
# NodeConnCB: a base class that describes the interface for receiving
#             callbacks with network messages from a NodeConn
# CBlock, CTransaction, CBlockHeader, CTxIn, CTxOut, etc....:
#     data structures that should map to corresponding structures in
#     bitcoin/primitives
# msg_block, msg_tx, msg_headers, etc.:
#     data structures that represent network messages
# ser_*, deser_*: functions that handle serialization/deserialization


import struct
import socket
import asyncore
import binascii
import time
import sys
import random
import cStringIO
import hashlib
from threading import RLock
from threading import Thread
import logging
import copy
from pyblake2 import blake2b

from .equihash import (
    gbp_basic,
    gbp_validate,
    hash_nonce,
    vidulum_person,
)

BIP0031_VERSION = 60000
MY_VERSION = 170002  # past bip-31 for ping/pong
MY_SUBVERSION = "/python-mininode-tester:0.0.1/"

MAX_INV_SZ = 50000


COIN = 150000000 # 1 VDL in zatoshis

# Keep our own socket map for asyncore, so that we can track disconnects
# ourselves (to workaround an issue with closing an asyncore socket when
# using select)
mininode_socket_map = dict()

# One lock for synchronizing all data access between the networking thread (see
# NetworkThread below) and the thread running the test logic.  For simplicity,
# NodeConn acquires this lock whenever delivering a message to to a NodeConnCB,
# and whenever adding anything to the send buffer (in send_message()).  This
# lock should be acquired in the thread running the test logic to synchronize
# access to any data shared with the NodeConnCB or NodeConn.
mininode_lock = RLock()

# Serialization/deserialization tools
def sha256(s):
    return hashlib.new('sha256', s).digest()


def hash256(s):
    return sha256(sha256(s))


def deser_string(f):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    return f.read(nit)


def ser_string(s):
    if len(s) < 253:
        return chr(len(s)) + s
    elif len(s) < 0x10000:
        return chr(253) + struct.pack("<H", len(s)) + s
    elif len(s) < 0x100000000L:
        return chr(254) + struct.pack("<I", len(s)) + s
    return chr(255) + struct.pack("<Q", len(s)) + s


def deser_uint256(f):
    r = 0L
    for i in xrange(8):
        t = struct.unpack("<I", f.read(4))[0]
        r += t << (i * 32)
    return r


def ser_uint256(u):
    rs = ""
    for i in xrange(8):
        rs += struct.pack("<I", u & 0xFFFFFFFFL)
        u >>= 32
    return rs


def uint256_from_str(s):
    r = 0L
    t = struct.unpack("<IIIIIIII", s[:32])
    for i in xrange(8):
        r += t[i] << (i * 32)
    return r


def uint256_from_compact(c):
    nbytes = (c >> 24) & 0xFF
    v = (c & 0xFFFFFFL) << (8 * (nbytes - 3))
    return v


def deser_vector(f, c):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = c()
        t.deserialize(f)
        r.append(t)
    return r


def ser_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<H", len(l))
    elif len(l) < 0x100000000L:
        r = chr(254) + struct.pack("<I", len(l))
    else:
        r = chr(255) + struct.pack("<Q", len(l))
    for i in l:
        r += i.serialize()
    return r


def deser_uint256_vector(f):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = deser_uint256(f)
        r.append(t)
    return r


def ser_uint256_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<H", len(l))
    elif len(l) < 0x100000000L:
        r = chr(254) + struct.pack("<I", len(l))
    else:
        r = chr(255) + struct.pack("<Q", len(l))
    for i in l:
        r += ser_uint256(i)
    return r


def deser_string_vector(f):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = deser_string(f)
        r.append(t)
    return r


def ser_string_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<H", len(l))
    elif len(l) < 0x100000000L:
        r = chr(254) + struct.pack("<I", len(l))
    else:
        r = chr(255) + struct.pack("<Q", len(l))
    for sv in l:
        r += ser_string(sv)
    return r


def deser_int_vector(f):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = struct.unpack("<i", f.read(4))[0]
        r.append(t)
    return r


def ser_int_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<H", len(l))
    elif len(l) < 0x100000000L:
        r = chr(254) + struct.pack("<I", len(l))
    else:
        r = chr(255) + struct.pack("<Q", len(l))
    for i in l:
        r += struct.pack("<i", i)
    return r


def deser_char_vector(f):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = struct.unpack("<B", f.read(1))[0]
        r.append(t)
    return r


def ser_char_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<H", len(l))
    elif len(l) < 0x100000000L:
        r = chr(254) + struct.pack("<I", len(l))
    else:
        r = chr(255) + struct.pack("<Q", len(l))
    for i in l:
        r += chr(i)
    return r


# Objects that map to bitcoind objects, which can be serialized/deserialized

class CAddress(object):
    def __init__(self):
        self.nServices = 1
        self.pchReserved = "\x00" * 10 + "\xff" * 2
        self.ip = "0.0.0.0"
        self.port = 0

    def deserialize(self, f):
        self.nServices = struct.unpack("<Q", f.read(8))[0]
        self.pchReserved = f.read(12)
        self.ip = socket.inet_ntoa(f.read(4))
        self.port = struct.unpack(">H", f.read(2))[0]

    def serialize(self):
        r = ""
        r += struct.pack("<Q", self.nServices)
        r += self.pchReserved
        r += socket.inet_aton(self.ip)
        r += struct.pack(">H", self.port)
        return r

    def __repr__(self):
        return "CAddress(nServices=%i ip=%s port=%i)" % (self.nServices,
                                                         self.ip, self.port)


class CInv(object):
    typemap = {
        0: "Error",
        1: "TX",
        2: "Block"}

    def __init__(self, t=0, h=0L):
        self.type = t
        self.hash = h

    def deserialize(self, f):
        self.type = struct.unpack("<i", f.read(4))[0]
        self.hash = deser_uint256(f)

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.type)
        r += ser_uint256(self.hash)
        return r

    def __repr__(self):
        return "CInv(type=%s hash=%064x)" \
            % (self.typemap[self.type], self.hash)


class CBlockLocator(object):
    def __init__(self):
        self.nVersion = MY_VERSION
        self.vHave = []

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        self.vHave = deser_uint256_vector(f)

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nVersion)
        r += ser_uint256_vector(self.vHave)
        return r

    def __repr__(self):
        return "CBlockLocator(nVersion=%i vHave=%s)" \
            % (self.nVersion, repr(self.vHave))


G1_PREFIX_MASK = 0x02
G2_PREFIX_MASK = 0x0a

class ZCProof(object):
    def __init__(self):
        self.g_A = None
        self.g_A_prime = None
        self.g_B = None
        self.g_B_prime = None
        self.g_C = None
        self.g_C_prime = None
        self.g_K = None
        self.g_H = None

    def deserialize(self, f):
        def deser_g1(self, f):
            leadingByte = struct.unpack("<B", f.read(1))[0]
            return {
                'y_lsb': leadingByte & 1,
                'x': f.read(32),
            }
        def deser_g2(self, f):
            leadingByte = struct.unpack("<B", f.read(1))[0]
            return {
                'y_gt': leadingByte & 1,
                'x': f.read(64),
            }
        self.g_A = deser_g1(f)
        self.g_A_prime = deser_g1(f)
        self.g_B = deser_g2(f)
        self.g_B_prime = deser_g1(f)
        self.g_C = deser_g1(f)
        self.g_C_prime = deser_g1(f)
        self.g_K = deser_g1(f)
        self.g_H = deser_g1(f)

    def serialize(self):
        def ser_g1(self, p):
            return chr(G1_PREFIX_MASK | p['y_lsb']) + p['x']
        def ser_g2(self, p):
            return chr(G2_PREFIX_MASK | p['y_gt']) + p['x']
        r = ""
        r += ser_g1(self.g_A)
        r += ser_g1(self.g_A_prime)
        r += ser_g2(self.g_B)
        r += ser_g1(self.g_B_prime)
        r += ser_g1(self.g_C)
        r += ser_g1(self.g_C_prime)
        r += ser_g1(self.g_K)
        r += ser_g1(self.g_H)
        return r

    def __repr__(self):
        return "ZCProof(g_A=%s g_A_prime=%s g_B=%s g_B_prime=%s g_C=%s g_C_prime=%s g_K=%s g_H=%s)" \
            % (repr(self.g_A), repr(self.g_A_prime),
               repr(self.g_B), repr(self.g_B_prime),
               repr(self.g_C), repr(self.g_C_prime),
               repr(self.g_K), repr(self.g_H))


ZC_NUM_JS_INPUTS = 2
ZC_NUM_JS_OUTPUTS = 2

ZC_NOTEPLAINTEXT_LEADING = 1
ZC_V_SIZE = 8
ZC_RHO_SIZE = 32
ZC_R_SIZE = 32
ZC_MEMO_SIZE = 512

ZC_NOTEPLAINTEXT_SIZE = (
  ZC_NOTEPLAINTEXT_LEADING +
  ZC_V_SIZE +
  ZC_RHO_SIZE +
  ZC_R_SIZE +
  ZC_MEMO_SIZE
)

NOTEENCRYPTION_AUTH_BYTES = 16

ZC_NOTECIPHERTEXT_SIZE = (
  ZC_NOTEPLAINTEXT_SIZE +
  NOTEENCRYPTION_AUTH_BYTES
)

class JSDescription(object):
    def __init__(self):
        self.vpub_old = 0
        self.vpub_new = 0
        self.anchor = 0
        self.nullifiers = [0] * ZC_NUM_JS_INPUTS
        self.commitments = [0] * ZC_NUM_JS_OUTPUTS
        self.onetimePubKey = 0
        self.randomSeed = 0
        self.macs = [0] * ZC_NUM_JS_INPUTS
        self.proof = None
        self.ciphertexts = [None] * ZC_NUM_JS_OUTPUTS

    def deserialize(self, f):
        self.vpub_old = struct.unpack("<q", f.read(8))[0]
        self.vpub_new = struct.unpack("<q", f.read(8))[0]
        self.anchor = deser_uint256(f)

        self.nullifiers = []
        for i in range(ZC_NUM_JS_INPUTS):
            self.nullifiers.append(deser_uint256(f))

        self.commitments = []
        for i in range(ZC_NUM_JS_OUTPUTS):
            self.commitments.append(deser_uint256(f))

        self.onetimePubKey = deser_uint256(f)
        self.randomSeed = deser_uint256(f)

        self.macs = []
        for i in range(ZC_NUM_JS_INPUTS):
            self.macs.append(deser_uint256(f))

        self.proof = ZCProof()
        self.proof.deserialize(f)

        self.ciphertexts = []
        for i in range(ZC_NUM_JS_OUTPUTS):
            self.ciphertexts.append(f.read(ZC_NOTECIPHERTEXT_SIZE))

    def serialize(self):
        r = ""
        r += struct.pack("<q", self.vpub_old)
        r += struct.pack("<q", self.vpub_new)
        r += ser_uint256(self.anchor)
        for i in range(ZC_NUM_JS_INPUTS):
            r += ser_uint256(self.nullifiers[i])
        for i in range(ZC_NUM_JS_OUTPUTS):
            r += ser_uint256(self.commitments[i])
        r += ser_uint256(self.onetimePubKey)
        r += ser_uint256(self.randomSeed)
        for i in range(ZC_NUM_JS_INPUTS):
            r += ser_uint256(self.macs[i])
        r += self.proof.serialize()
        for i in range(ZC_NUM_JS_OUTPUTS):
            r += ser_uint256(self.ciphertexts[i])
        return r

    def __repr__(self):
        return "JSDescription(vpub_old=%i.%08i vpub_new=%i.%08i anchor=%064x onetimePubKey=%064x randomSeed=%064x proof=%s)" \
            % (self.vpub_old, self.vpub_new, self.anchor,
               self.onetimePubKey, self.randomSeed, repr(self.proof))


class COutPoint(object):
    def __init__(self, hash=0, n=0):
        self.hash = hash
        self.n = n

    def deserialize(self, f):
        self.hash = deser_uint256(f)
        self.n = struct.unpack("<I", f.read(4))[0]

    def serialize(self):
        r = ""
        r += ser_uint256(self.hash)
        r += struct.pack("<I", self.n)
        return r

    def __repr__(self):
        return "COutPoint(hash=%064x n=%i)" % (self.hash, self.n)


class CTxIn(object):
    def __init__(self, outpoint=None, scriptSig="", nSequence=0):
        if outpoint is None:
            self.prevout = COutPoint()
        else:
            self.prevout = outpoint
        self.scriptSig = scriptSig
        self.nSequence = nSequence

    def deserialize(self, f):
        self.prevout = COutPoint()
        self.prevout.deserialize(f)
        self.scriptSig = deser_string(f)
        self.nSequence = struct.unpack("<I", f.read(4))[0]

    def serialize(self):
        r = ""
        r += self.prevout.serialize()
        r += ser_string(self.scriptSig)
        r += struct.pack("<I", self.nSequence)
        return r

    def __repr__(self):
        return "CTxIn(prevout=%s scriptSig=%s nSequence=%i)" \
            % (repr(self.prevout), binascii.hexlify(self.scriptSig),
               self.nSequence)


class CTxOut(object):
    def __init__(self, nValue=0, scriptPubKey=""):
        self.nValue = nValue
        self.scriptPubKey = scriptPubKey

    def deserialize(self, f):
        self.nValue = struct.unpack("<q", f.read(8))[0]
        self.scriptPubKey = deser_string(f)

    def serialize(self):
        r = ""
        r += struct.pack("<q", self.nValue)
        r += ser_string(self.scriptPubKey)
        return r

    def __repr__(self):
        return "CTxOut(nValue=%i.%08i scriptPubKey=%s)" \
            % (self.nValue // 100000000, self.nValue % 100000000,
               binascii.hexlify(self.scriptPubKey))


class CTransaction(object):
    def __init__(self, tx=None):
        if tx is None:
            self.nVersion = 1
            self.vin = []
            self.vout = []
            self.nLockTime = 0
            self.vjoinsplit = []
            self.joinSplitPubKey = None
            self.joinSplitSig = None
            self.sha256 = None
            self.hash = None
        else:
            self.nVersion = tx.nVersion
            self.vin = copy.deepcopy(tx.vin)
            self.vout = copy.deepcopy(tx.vout)
            self.nLockTime = tx.nLockTime
            self.vjoinsplit = copy.deepcopy(tx.vjoinsplit)
            self.joinSplitPubKey = tx.joinSplitPubKey
            self.joinSplitSig = tx.joinSplitSig
            self.sha256 = None
            self.hash = None

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        self.vin = deser_vector(f, CTxIn)
        self.vout = deser_vector(f, CTxOut)
        self.nLockTime = struct.unpack("<I", f.read(4))[0]
        if self.nVersion >= 2:
            self.vjoinsplit = deser_vector(f, JSDescription)
            if len(self.vjoinsplit) > 0:
                self.joinSplitPubKey = deser_uint256(f)
                self.joinSplitSig = f.read(64)
        self.sha256 = None
        self.hash = None

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nVersion)
        r += ser_vector(self.vin)
        r += ser_vector(self.vout)
        r += struct.pack("<I", self.nLockTime)
        if self.nVersion >= 2:
            r += ser_vector(self.vjoinsplit)
            if len(self.vjoinsplit) > 0:
                r += ser_uint256(self.joinSplitPubKey)
                r += self.joinSplitSig
        return r

    def rehash(self):
        self.sha256 = None
        self.calc_sha256()

    def calc_sha256(self):
        if self.sha256 is None:
            self.sha256 = uint256_from_str(hash256(self.serialize()))
        self.hash = hash256(self.serialize())[::-1].encode('hex_codec')

    def is_valid(self):
        self.calc_sha256()
        for tout in self.vout:
            if tout.nValue < 0 or tout.nValue > 21000000L * 100000000L:
                return False
        return True

    def __repr__(self):
        r = "CTransaction(nVersion=%i vin=%s vout=%s nLockTime=%i" \
            % (self.nVersion, repr(self.vin), repr(self.vout), self.nLockTime)
        if self.nVersion >= 2:
            r += " vjoinsplit=%s" % repr(self.vjoinsplit)
            if len(self.vjoinsplit) > 0:
                r += " joinSplitPubKey=%064x joinSplitSig=%064x" \
                    (self.joinSplitPubKey, self.joinSplitSig)
        r += ")"
        return r


class CBlockHeader(object):
    def __init__(self, header=None):
        if header is None:
            self.set_null()
        else:
            self.nVersion = header.nVersion
            self.hashPrevBlock = header.hashPrevBlock
            self.hashMerkleRoot = header.hashMerkleRoot
            self.hashReserved = header.hashReserved
            self.nTime = header.nTime
            self.nBits = header.nBits
            self.nNonce = header.nNonce
            self.nSolution = header.nSolution
            self.sha256 = header.sha256
            self.hash = header.hash
            self.calc_sha256()

    def set_null(self):
        self.nVersion = 4
        self.hashPrevBlock = 0
        self.hashMerkleRoot = 0
        self.hashReserved = 0
        self.nTime = 0
        self.nBits = 0
        self.nNonce = 0
        self.nSolution = []
        self.sha256 = None
        self.hash = None

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        self.hashPrevBlock = deser_uint256(f)
        self.hashMerkleRoot = deser_uint256(f)
        self.hashReserved = deser_uint256(f)
        self.nTime = struct.unpack("<I", f.read(4))[0]
        self.nBits = struct.unpack("<I", f.read(4))[0]
        self.nNonce = deser_uint256(f)
        self.nSolution = deser_char_vector(f)
        self.sha256 = None
        self.hash = None

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nVersion)
        r += ser_uint256(self.hashPrevBlock)
        r += ser_uint256(self.hashMerkleRoot)
        r += ser_uint256(self.hashReserved)
        r += struct.pack("<I", self.nTime)
        r += struct.pack("<I", self.nBits)
        r += ser_uint256(self.nNonce)
        r += ser_char_vector(self.nSolution)
        return r

    def calc_sha256(self):
        if self.sha256 is None:
            r = ""
            r += struct.pack("<i", self.nVersion)
            r += ser_uint256(self.hashPrevBlock)
            r += ser_uint256(self.hashMerkleRoot)
            r += ser_uint256(self.hashReserved)
            r += struct.pack("<I", self.nTime)
            r += struct.pack("<I", self.nBits)
            r += ser_uint256(self.nNonce)
            r += ser_char_vector(self.nSolution)
            self.sha256 = uint256_from_str(hash256(r))
            self.hash = hash256(r)[::-1].encode('hex_codec')

    def rehash(self):
        self.sha256 = None
        self.calc_sha256()
        return self.sha256

    def __repr__(self):
        return "CBlockHeader(nVersion=%i hashPrevBlock=%064x hashMerkleRoot=%064x hashReserved=%064x nTime=%s nBits=%08x nNonce=%064x nSolution=%s)" \
            % (self.nVersion, self.hashPrevBlock, self.hashMerkleRoot, self.hashReserved,
               time.ctime(self.nTime), self.nBits, self.nNonce, repr(self.nSolution))


class CBlock(CBlockHeader):
    def __init__(self, header=None):
        super(CBlock, self).__init__(header)
        self.vtx = []

    def deserialize(self, f):
        super(CBlock, self).deserialize(f)
        self.vtx = deser_vector(f, CTransaction)

    def serialize(self):
        r = ""
        r += super(CBlock, self).serialize()
        r += ser_vector(self.vtx)
        return r

    def calc_merkle_root(self):
        hashes = []
        for tx in self.vtx:
            tx.calc_sha256()
            hashes.append(ser_uint256(tx.sha256))
        while len(hashes) > 1:
            newhashes = []
            for i in xrange(0, len(hashes), 2):
                i2 = min(i+1, len(hashes)-1)
                newhashes.append(hash256(hashes[i] + hashes[i2]))
            hashes = newhashes
        return uint256_from_str(hashes[0])

    def is_valid(self, n=48, k=5):
        # H(I||...
        digest = blake2b(digest_size=(512/n)*n/8, person=vidulum_person(n, k))
        digest.update(super(CBlock, self).serialize()[:108])
        hash_nonce(digest, self.nNonce)
        if not gbp_validate(self.nSolution, digest, n, k):
            return False
        self.calc_sha256()
        target = uint256_from_compact(self.nBits)
        if self.sha256 > target:
            return False
        for tx in self.vtx:
            if not tx.is_valid():
                return False
        if self.calc_merkle_root() != self.hashMerkleRoot:
            return False
        return True

    def solve(self, n=48, k=5):
        target = uint256_from_compact(self.nBits)
        # H(I||...
        digest = blake2b(digest_size=(512/n)*n/8, person=vidulum_person(n, k))
        digest.update(super(CBlock, self).serialize()[:108])
        self.nNonce = 0
        while True:
            # H(I||V||...
            curr_digest = digest.copy()
            hash_nonce(curr_digest, self.nNonce)
            # (x_1, x_2, ...) = A(I, V, n, k)
            solns = gbp_basic(curr_digest, n, k)
            for soln in solns:
                assert(gbp_validate(curr_digest, soln, n, k))
                self.nS