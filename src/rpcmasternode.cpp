// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2017-2018 The SnowGem developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "rpc/server.h"
#include "utilmoneystr.h"
#include "key_io.h"

#include <boost/tokenizer.hpp>

#include <fstream>

void SendMoney(const CTxDestination& address, CAmount nValue, CWalletTx& wtxNew, AvailableCoinsType coin_type = ALL_COINS)
{
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse Vidulum address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    if (!pwalletMain->CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired, strError, NULL, coin_type)) {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}

UniValue obfuscation(const UniValue& params, bool fHelp)
{
    throw runtime_error("Obfuscation is not supported any more. User Zerocoin\n");
    
    if (fHelp || params.size() == 0)
        throw runtime_error(
            "obfuscation <solarisaddress> <amount>\n"
            "solarisaddress, reset, or auto (AutoDenominate)"
            "<amount> is a real and will be rounded to the next 0.1" +
            HelpRequiringPassphrase());

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    if (params[0].get_str() == "auto") {
        if (fMasterNode)
            return "ObfuScation is not supported from masternodes";

        return "DoAutomaticDenominating " + (obfuScationPool.DoAutomaticDenominating() ? "successful" : ("failed: " + obfuScationPool.GetStatus()));
    }

    if (params[0].get_str() == "reset") {
        obfuScationPool.Reset();
        return "successfully reset obfuscation";
    }

    if (params.size() != 2)
        throw runtime_error(
            "obfuscation <solarisaddress> <amount>\n"
            "solarisaddress, denominate, or auto (AutoDenominate)"
            "<amount> is a real and will be rounded to the next 0.1" +
            HelpRequiringPassphrase());

    std::string strAddress = params[0].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Vidulum address");

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    //    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx, ONLY_DENOMINATED);
    SendMoney(dest, nAmount, wtx, ONLY_DENOMINATED);
    //    if (strError != "")
    //        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}

UniValue getpoolinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "\nReturns anonymous pool-related information\n"

            "\nResult:\n"
            "{\n"
            "  \"current\": \"addr\",    (string) Vidulum address of current masternode\n"
            "  \"state\": xxxx,        (string) unknown\n"
            "  \"entries\": xxxx,      (numeric) Number of entries\n"
            "  \"accepted\": xxxx,     (numeric) Number of entries accepted\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getpoolinfo", "") + HelpExampleRpc("getpoolinfo", ""));

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("current_masternode", mnodeman.GetCurrentMasterNode()->addr.ToString()));
    obj.push_back(Pair("state", obfuScationPool.GetState()));
    obj.push_back(Pair("entries", obfuScationPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted", obfuScationPool.GetCountEntriesAccepted()));
    return obj;
}

// This command is retained for backwards compatibility, but is depreciated.
// Future removal of this command is planned to keep things clean.
UniValue masternode(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "start-all" && strCommand != "start-missing" &&
            strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count" && strCommand != "enforce" &&
            strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" &&
            strCommand != "outputs" && strCommand != "status" && strCommand != "calcscore"))
        throw runtime_error(
            "masternode \"command\"...\n"
            "\nSet of commands to execute masternode related actions\n"
            "This command is depreciated, please see individual command documentation for future reference\n\n"

            "\nArguments:\n"
            "1. \"command\"        (string or set of strings, required) The command to execute\n"

            "\nAvailable commands:\n"
            "  count        - Print count information of all known masternodes\n"
            "  current      - Print info on current masternode winner\n"
            "  debug        - Print masternode status\n"
            "  genkey       - Generate new masternodeprivkey\n"
            "  outputs      - Print masternode compatible outputs\n"
            "  start        - Start masternode configured in vidulum.conf\n"
            "  start-alias  - Start single masternode by assigned alias configured in masternode.conf\n"
            "  start-<mode> - Start masternodes configured in masternode.conf (<mode>: 'all', 'missing', 'disabled')\n"
            "  status       - Print masternode status information\n"
            "  list         - Print list of all known masternodes (see masternodelist for more info)\n"
            "  list-conf    - Print masternode.conf in JSON format\n"
            "  winners      - Print list of masternode winners\n");

    if (strCommand == "list") {
		UniValue newParams(UniValue::VARR);
		
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
		
        return listmasternodes(newParams, fHelp);
    }

    if (strCommand == "connect") {
        UniValue newParams(UniValue::VARR);
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return masternodeconnect(newParams, fHelp);
    }

    if (strCommand == "count") {
        UniValue newParams(UniValue::VARR);
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmasternodecount(newParams, fHelp);
    }

    if (strCommand == "current") {
        UniValue newParams(UniValue::VARR);
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return masternodecurrent(newParams, fHelp);
    }

    if (strCommand == "debug") {
        UniValue newParams(UniValue::VARR);
        for (unsigned int i = 1; i <