#!/usr/bin/python
#
# linearize-data.py: Construct a linear, no-fork version of the chain.
#
# Copyright (c) 2013-2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

from __future__ import print_function, division
import json
import struct
import re
import os
import os.path
import base64
import httplib
import sys
import hashlib
import datetime
import time
from collections import namedtuple

settings = {}

def uint32(x):
	return x & 0xffffffffL

def bytereverse(x):
	return uint32(( ((x) << 24) | (((x) << 8) & 0x00ff0000) |
		       (((x) >> 8) & 0x0000ff00) | ((x) >> 24) ))

def bufreverse(in_buf):
	out_words = []
	for i in range(0, len(in_buf), 4):
		word = struct.unpack('@I', in_buf[i:i+4])[0]
		out_words.append(struct.pack('@I', bytereverse(word)))
	return ''.join(out_words)

def wordreverse(in_buf):
	out_words = []
	for i in range(0, len(in_buf), 4):
		out_words.append(in_buf[i:i+4])
	out_words.reverse()
	return ''.join(out_words)

def calc_hdr_hash(blk_hdr):
	hash1 = hashlib.sha256()
	hash1.update(blk_hdr)
	hash1_o = hash1.digest()

	hash2 = hashlib.sha256()
	hash2.update(hash1_o)
	hash2_o = hash2.digest()

	return hash2_o

def calc_hash_str(blk_hdr):
	hash = calc_hdr_hash(blk_hdr)
	hash = bufreverse(hash)
	hash = wordreverse(hash)
	hash_str = hash.encode('hex')
	return hash_str

def get_blk_dt(blk_hdr):
	members = struct.unpack("<I", blk_hdr[68:68+4])
	nTime = members[0]
	dt = datetime.datetime.fromtimestamp(nTime)
	dt_ym = datetime.datetime(dt.year, dt.month, 1)
	return (dt_ym, nTime)

def get_block_hashes(settings):
	blkindex = []
	f = open(settings['hashlist'], "r")
	for line in f:
		line = line.rstrip()
		blkindex.append(line)

	print("Read " + str(len(blkindex)) + " hashes")

	return blkindex

def mkblockmap(blkindex):
	blkmap = {}
	for height,hash in enumerate(blkindex):
		blkmap[hash] = height
	return blkmap

#