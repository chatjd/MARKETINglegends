#!/usr/bin/env python2
# Copyright (c) 2014-2015 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test fee estimation code
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import start_node, connect_nodes, \
    sync_blocks, sync_mempools

import random
from decimal import Decimal, ROUND_DOWN

# Construct 2 trivial P2SH's and the ScriptSigs that spend them
# So we can create many many transactions without needing to spend
# time signing.
P2SH_1 = "2MySexEGVzZpRgNQ1JdjdP5bRETznm3roQ2" # P2SH of "OP_1 OP_DROP"
P2SH_2 = "2NBdpwq8Aoo1EEKEXPNrKvr5xQr3M9UfcZA" # P2SH of "OP_2 OP_DROP"
# Associated ScriptSig's to spend satisfy P2SH_1 and P2SH_2
# 4 bytes of OP_TRUE and push 2-byte redeem script of "OP_1 OP_DROP" or "OP_2 OP_DROP"
SCRIPT_SIG = ["0451025175", "0451025275"]

def satoshi_round(amount):
    return  Decimal(amount).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)

def small_txpuzzle_randfee(from_node, conflist, unconflist, amount, min_fee, fee_increment):
    '''
    Create and send a transaction with a random fee.
    The transaction pays to a trival P2SH script, and assumes that its inputs
    are of the same form.
    The function takes a list of confirmed outputs and unconfirmed outputs
    and attempts to use the confirmed list first for its inputs.
    It adds the newly created outputs to the unconfirmed list.
    Returns (raw transaction, fee)
    '''
    # It's best to exponentially distribute our random fees
    # because the buckets are exponentially spaced.
    # Exponentially distributed from 1-128 * fee_increment
    rand_fee = float(fee_increment)*(1.1892**random.randint(0,28))
    # Total fee ranges from min_fee to min_fee + 127*fee_increment
    fee = min_fee - fee_increment + satoshi_round(rand_fee)
    inputs = []
    total_in = Decimal("0.00000000")
    while total_in <= (amount + fee) and len(conflist) > 0:
        t = conflist.pop(0)
        total_in += t["amount"]
        inputs.append({ "txid" : t["txid"], "vout" : t["vout"]} )
    if total_in <= amount + fee:
        while total_in <= (amount + fee) and len(unconflist) > 0:
            t = unconflist.pop(0)
            total_in += t["amount"]
            inputs.append({ "txid" : t["txid"], "vout" : t["vout"]} )
        if total_in <= amount + fee:
            raise RuntimeError("Insufficient funds: need %d, have %d"%(amount+fee, total_in))
    outputs = {}
    outputs[P2SH_1] = total_in - amount - fee
    outputs[P2SH_2] = amount
    rawtx = from_node.createrawtransaction(inputs, outputs)
    # Createrawtransaction constructions a transaction that is ready to be signed
    # These transactions don't need to be signed, but we still have to insert the ScriptSig
    # that will satisfy the ScriptPubKey.
    completetx = rawtx[0:10]
    inputnum = 0
    for inp in inputs:
        completetx += rawtx[10+82*inputnum:82+82*inputnum]
        completetx += SCRIPT_SIG[inp["vout"]]
        completetx += rawtx[84+82*inputnum:92+82*inputnum]
        inputnum += 1
    completetx += rawtx[10+82*inputnum:]
    txid = from_node.sendrawtransaction(completetx, True)
    unconflist.append({ "txid" : txid, "vout" : 0 , "amount" : total_in - amount - fee})
    unconflist.append({ "txid" : txid, "vout" : 1 , "amount" : amount})

    return (completetx, fee)

def split_inputs(from_node, txins, txouts, initial_split = False):
    '''
    We need to generate a lot of very small inputs so we can generate a ton of transactions
    and they will have low priority.
    This function takes an input from txins, and creates and sends a transaction
    which splits the value into 2 outputs which are appended to txouts.
    '''
    prevtxout = txins.pop()
    inputs = []
    outputs = {}
    inputs.append({ "txid" : prevtxout["txid"], "vout" : prevtxout["vout"] })
    half_change = satoshi_round(prevtxout["amount"]/2)
    rem_change = prevtxout["amount"] - half_change  - Decimal("0.00001000")
    outputs[P2SH_1] = half_change
    outputs[P2SH_2] = rem_change
    rawtx = from_node.createrawtransaction(inputs, outputs)
    # If this is the initial split we actually need to sign the transaction
    # Otherwise we just need to insert the property ScriptSig
    if (initial_split) :
        completetx = from_node.signrawtransaction(rawtx)["hex"]
    else :
        completetx = rawtx[0:82] + SCRIPT_SIG[prevtxout["vout"]] + rawtx[84:]
    txid = from_node.sendrawtransaction(completetx, True)
    txouts.append({ "txid" : txid, "vout" : 0 , "amount" : half_change})
    txouts.append({ "txid" : txid, "vout" : 1 , "amount" : rem_change})

def check_estimates(node, fees_seen, max_invalid, print_estimates = True):
    '''
    This function calls estimatefee and verifies that the estimates
    meet certain invariants.
    '''
    all_estimates = [ node.estimatefee(i) for i in range(1,26) ]
    if print_estimates:
        print([str(all_estimates[e-1]) for e in [1,2,3,6,15,25]])
    delta = 1.0e-6 # account for rounding error
    last_e = max(fees_seen)
    for e in filter(lambda x: x >= 0, all_estimates):
        # Estimates should be within the bounds of what transactions fees actually were:
        if float(e)+delta < min(fees_seen) or float(e)-delta > max(fees_seen):
            raise AssertionError("Estimated fee (%f) out of range (%f,%f)"
                                 %(float(e), min(fees_seen), max(fees_seen)))
        # Estimates should be monotonically decreasing
        if float(e)-delta > last_e:
            raise AssertionError("Estimated fee (%f) larger than last fee (%f) for lower number of confirms"
                                 %(float(e),float(last_e)))
        last_e = e
    valid_estimate = False
    invalid_estimates = 0
    for e in all_estimates:
        if e >= 0:
            valid_estimate = True
        else:
            invalid_estimates += 1
        # Once we're at a high enough confirmation count that we can give an estimate
        # We should have estimates for all higher confirmation counts
        if valid_estimate and e < 0:
            raise AssertionError("Invalid estimate appears at higher confirm count than valid estimate")
    # Check on the expected number of different confirmation counts
    # that we mi