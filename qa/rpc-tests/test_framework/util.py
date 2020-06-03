# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


#
# Helpful routines for regression testing
#

# Add python-bitcoinrpc to module search path:
import os
import sys

from binascii import hexlify, unhexlify
from base64 import b64encode
from decimal import Decimal, ROUND_DOWN
import json
import random
import shutil
import subprocess
import time
import re

from authproxy import AuthServiceProxy

def p2p_port(n):
    return 11000 + n + os.getpid()%999
def rpc_port(n):
    return 12000 + n + os.getpid()%999

def check_json_precision():
    """Make sure json library being used does not lose precision converting BTC values"""
    n = Decimal("20000000.00000003")
    satoshis = int(json.loads(json.dumps(float(n)))*1.0e8)
    if satoshis != 2000000000000003:
        raise RuntimeError("JSON encode/decode loses precision")

def bytes_to_hex_str(byte_str):
    return hexlify(byte_str).decode('ascii')

def hex_str_to_bytes(hex_str):
    return unhexlify(hex_str.encode('ascii'))

def str_to_b64str(string):
    return b64encode(string.encode('utf-8')).decode('ascii')

def sync_blocks(rpc_connections, wait=1):
    """
    Wait until everybody has the same block count
    """
    while True:
        counts = [ x.getblockcount() for x in rpc_connections ]
        if counts == [ counts[0] ]*len(counts):
            break
        time.sleep(wait)

def sync_mempools(rpc_connections, wait=1):
    """
    Wait until everybody has the same transactions in their memory
    pools
    """
    while True:
        pool = set(rpc_connections[0].getrawmempool())
        num_match = 1
        for i in range(1, len(rpc_connections)):
            if set(rpc_connections[i].getrawmempool()) == pool:
                num_match = num_match+1
        if num_match == len(rpc_connections):
            break
        time.sleep(wait)

bitcoind_processes = {}

def initialize_datadir(dirname, n):
    datadir = os.path.join(dirname, "node"+str(n))
    if not os.path.isdir(datadir):
        os.makedirs(datadir)
    with open(os.path.join(datadir, "vidulum.conf"), 'w') as f:
        f.write("regtest=1\n");
        f.write("showmetrics=0\n");
        f.write("rpcuser=rt\n");
        f.write("rpcpassword=rt\n");
        f.write("port="+str(p2p_port(n))+"\n");
        f.write("rpcport="+str(rpc_port(n))+"\n");
        f.write("listenonion=0\n");
    return datadir

def initialize_chain(test_dir):
    """
    Create (or copy from cache) a 200-block-long chain and
    4 wallets.
    bitcoind and bitcoin-cli must be in search path.
    """

    if not os.path.isdir(os.path.join("cache", "node0")):
        devnull = open("/dev/null", "w+")
        # Create cache directories, run bitcoinds:
        for i in range(4):
            datadir=initialize_datadir("cache", i)
            args = [ os.getenv("BITCOIND", "bitcoind"), "-keypool=1", "-datadir="+datadir, "-discover=0" ]
            if i > 0:
                args.append("-connect=127.0.0.1:"+str(p2p_port(0)))
            bitcoind_processes[i] = subprocess.Popen(args)
            if os.getenv("PYTHON_DEBUG", ""):
                print "initialize_chain: bitcoind started, calling bitcoin-cli -rpcwait getblockcount"
            subprocess.check_call([ os.getenv("BITCOINCLI", "bitcoin-cli"), "-datadir="+datadir,
                                    "-rpcwait", "getblockcount"], stdout=devnull)
            if os.getenv("PYTHON_DEBUG", ""):
                print "initialize_chain: bitcoin-cli -rpcwait getblockcount completed"
        devnull.close()
        rpcs = []
        for i in range(4):
            try:
                url = "http://rt:rt@127.0.0.1:%d"%(rpc_port(i),)
                rpcs.append(AuthServiceProxy(url))
            except:
                sys.stderr.write("Error connecting to "+url+"\n")
                sys.exit(1)

        # Create a 200-block-long chain; each of the 4 nod