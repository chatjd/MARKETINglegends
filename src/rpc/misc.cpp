// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clientversion.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "metrics.h"
#include "masternode-sync.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "spork.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include <univalue.h>

#include "vidulum/Address.hpp"

using namespace std;

/**
 *Return current blockchain status, wallet balance, address balance and the last 200 transactions
**/
UniValue getalldata(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getalldata \"datatype transactiontype \"\n"
            "\nArguments:\n"
            "1. \"datatype\"     (integer, required) \n"
            "                    Value of 0: Return address, balance, transactions and blockchain info\n"
            "                    Value of 1: Return address, balance, blockchain info\n"
            "                    Value of 2: Return transactions and blockchain info\n"
            "2. \"transactiontype\"     (integer, optional) \n"
            "                    Value of 1: Return all transactions in the last 24 hours\n"
            "                    Value of 2: Return all transactions in the last 7 days\n"
            "                    Value of 3: Return all transactions in the last 30 days\n"
            "                    Other number: Return all transactions in the last 24 hours\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("getalldata", "0")
            + HelpExampleRpc("getalldata", "0")
        );

    LOCK(cs_main);

    UniValue returnObj(UniValue::VOBJ);
    int connectionCount = 0;
    {
        LOCK2(cs_main, cs_vNodes);
        connectionCount = (int)vNodes.size();
    }

    vector<COutput> vecOutputs;
    CAmount remainingValue = 0;

    pwalletMain->AvailableCoins(vecOutputs, true, NULL, false, true);

    // Find unspent coinbase utxos and update estimated size
    BOOST_FOREACH(const COutput& out, vecOutputs) {
        if (!out.fSpendable) {
            continue;
        }

        CTxDestination address;
        if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            continue;
        }

        if (!out.tx->IsCoinBase()) {
            continue;
        }

        CAmount nValue = out.tx->vout[out.i].nValue;

        remainingValue += nValue;
    }

    int nMinDepth = 1;
    CAmount nBalance = getBalanceTaddr("", nMinDepth, true);
    CAmount nPrivateBalance = getBalanceZaddr("", nMinDepth, true);
    CAmount nLockedCoin = pwalletMain->GetLockedCoins();

    CAmount nTotalBalance = nBalance + nPrivateBalance + nLockedCoin;

    returnObj.push_back(Pair("connectionCount", connectionCount));
    returnObj.push_back(Pair("besttime", chainActive.Tip()->GetBlockTime()));
    returnObj.push_back(Pair("bestblockhash", chainActive.Tip()->GetBlockHash().GetHex()));
    returnObj.push_back(Pair("transparentbalance", FormatMoney(nBalance)));
    returnObj.push_back(Pair("privatebalance", FormatMoney(nPrivateBalance)));
    returnObj.push_back(Pair("lockedbalance", FormatMoney(nLockedCoin)));
    returnObj.push_back(Pair("totalbalance", FormatMoney(nTotalBalance)));
    returnObj.push_back(Pair("remainingValue", ValueFromAmount(remainingValue)));
    returnObj.push_back(Pair("unconfirmedbalance", FormatMoney(pwalletMain->GetUnconfirmedBalance())));
    returnObj.push_back(Pair("immaturebalance", FormatMoney(pwalletMain->GetImmatureBalance())));

    //get address balance
    nBalance = 0;

    //get all t address
    UniValue addressbalance(UniValue::VARR);
    UniValue addrlist(UniValue::VOBJ);

    if (params.size() > 0 && (params[0].get_int() == 1 || params[0].get_int() == 0))
    {
      BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& item, pwalletMain->mapAddressBook)
      {
          UniValue addr(UniValue::VOBJ);
          const CTxDestination& dest = item.first;

          isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;

          const string& strName = item.second.name;
          nBalance = getBalanceTaddr(EncodeDestination(dest), nMinDepth, false);

          addr.push_back(Pair("amount", ValueFromAmount(nBalance)));
          addr.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
          addrlist.push_back(Pair(EncodeDestination(dest), addr));
      }

      //address grouping
      {
          LOCK2(cs_main, pwalletMain->cs_wallet);

          map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
          BOOST_FOREACH(set<CTxDestination> grouping, pwalletMain->GetAddressGroupings())
          {
              BOOST_FOREACH(CTxDestination address, grouping)
              {
                  UniValue addr(UniValue::VOBJ);
                  const string& strName = EncodeDestination(address);
                  if(addrlist.exists(strName))
                      continue;
                  isminetype mine = pwalletMain ? IsMine(*pwalletMain, address) : ISMINE_NO;

                  nBalance = getBalanceTaddr(strName, nMinDepth, false);

                  addr.push_back(Pair("amount", ValueFromAmount(nBalance)));
                  addr.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
                  addrlist.push_back(Pair(strName, addr));
              }
          }
      }

      //get all z address
      {
          std::set<libzcash::SproutPaymentAddress> addresses;
          pwalletMain->GetSproutPaymentAddresses(addresses);
          for (auto addr : addresses) {
              if (pwalletMain->HaveSproutSpendingKey(addr)) {
                  UniValue address(UniValue::VOBJ);
                  const string& strName = EncodePaymentAddress(addr);
                  nBalance = getBalanceZaddr(strName, nMinDepth, false);
                  address.push_back(Pair("amount", ValueFromAmount(nBalance)));
                  address.push_back(Pair("ismine", true));
                  addrlist.push_back(Pair(strName, address));
              }
              else
              {
                  UniValue address(UniValue::VOBJ);
                  const string& strName = EncodePaymentAddress(addr);
                  nBalance = getBalanceZaddr(strName, nMinDepth, false);
                  address.push_back(Pair("amount", ValueFromAmount(nBalance)));
                  address.push_back(Pair("ismine", false));
                  addrlist.push_back(Pair(strName, address));
              }
          }
      }
      {
          std::set<libzcash::SaplingPaymentAddress> addresses;
          pwalletMain->GetSaplingPaymentAddresses(addresses);
          libzcash::SaplingIncomingViewingKey ivk;
          libzcash::SaplingFullViewingKey fvk;
          for (auto addr : addresses) {
              if (pwalletMain->GetSaplingIncomingViewingKey(addr, ivk) &&
                  pwalletMain->GetSaplingFullViewingKey(ivk, fvk)) {
                  if(pwalletMain->HaveSaplingSpendingKey(fvk)) {
                      UniValue address(UniValue::VOBJ);
                      const string& strName = EncodePaymentAddress(addr);
                      nBalance = getBalanceZaddr(strName, nMinDepth, false);
                      address.push_back(Pair("amount", ValueFromAmount(nBalance)));
                      address.push_back(Pair("ismine", true));
                      addrlist.push_back(Pair(strName, address));
                  }
                  else
                  {
                      UniValue address(UniValue::VOBJ);
                      const string& strName = EncodePaymentAddress(addr);
                      nBalance = getBalanceZaddr(strName, nMinDepth, false);
                      address.push_back(Pair("amount", ValueFromAmount(nBalance)));
                      address.push_back(Pair("ismine", false));
                      addrlist.push_back(Pair(strName, address));
                  }
              }
          }
      }
    }

    addressbalance.push_back(addrlist);
    returnObj.push_back(Pair("addressbalance", addressbalance));


    //get transactions
    string strAccount = "";
    int nCount = 200;
    int nFrom = 0;
    isminefilter filter = ISMINE_SPENDABLE;

    UniValue trans(UniValue::VARR);
    UniValue transTime(UniValue::VARR);
    if (params.size() > 0 && (params[0].get_int() == 2 || params[0].get_int() == 0))
    {
        int day = 1;
        if(params.size() > 1)
        {
            if(params[1].get_int() == 1)
            {
                day = 1;
            }
            else if(params[1].get_int() == 2)
            {
                day = 7;
            }
            else if(params[1].get_int() == 3)
            {
                day = 30;
            }
            else if(params[1].get_int() == 4)
            {
                day = 90;
            }
            else if(params[1].get_int() == 5)
            {
                day = 365;
            }
        }

        std::list<CAccountingEntry> acentries;
        CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);
        uint64_t t = GetTime();
        // iterate backwards until we have nCount items to return:
        for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second.first;
            if (pwtx != 0)
                ListTransactions(*pwtx, strAccount, 0, true, trans, filter);
            CAccountingEntry *const pacentry = (*it).second.second;
            if (pacentry != 0)
                AcentryToJSON(*pacentry, strAccount, trans);
            int confirms = pwtx->GetDepthInMainChain();
            if(confirms > 0)
            {
                if (mapBlockIndex[pwtx->hashBlock]->GetBlockTime() <= (t - (day * 60 * 60 * 24)) && (int)trans.size() >= nCount) break;
            }
        }

        vector<UniValue> arrTmp = trans.getValues();

        std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

        trans.clear();
        trans.setArray();
        trans.push_backV(arrTmp);
    }

    returnObj.push_back(Pair("listtransactions", trans));
    return returnObj;
}

/**
 * @note Do not add or change anything in the information returned by this
 * method. `getinfo` exists for backwards-compatibility only. It combines
 * information from wildly different sources in the program, which is a mess,
 * and is thus planned to be deprecated eventually.
 *
 * Based on the source of the information, new information should be added to:
 * - `getblockchaininfo`,
 * - `getnetworkinfo` or
 * - `getwalletinfo`
 *
 * Or alternatively, create a specific query method for the information.
 **/
UniValue getinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "Returns an object containing various state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,           (numeric) the server version\n"
            "  \"protocolversion\": xxxxx,   (numeric) the protocol version\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total Vidulum balance of the wallet\n"
            "  \"blocks\": xxxxxx,           (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,        (numeric) the time offset\n"
            "  \"connections\": xxxxx,       (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",     (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,       (numeric) the current difficulty\n"
            "  \"testnet\": true|false,      (boolean) if the server is using testnet or not\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee set in " + CURRENCY_UNIT + "/kB\n"
            "  \"relayfee\": x.xxxx,         (numeric) minimum relay fee for non-free transactions in " + CURRENCY_UNIT + "/kB\n"
            "  \"errors\": \"...\"           (string) any error messages\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getinfo", "")
            + HelpExampleRpc("getinfo", "")
        );

#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
#else
    LOCK(cs_main);
#endif

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("version", CLIENT_VERSION));
    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    }
#endif
    obj.push_back(Pair("blocks",        (int)chainActive.Height()));
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (proxy.IsValid() ? proxy.proxy.ToStringIPPort() : string())));
    obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
    obj.push_back(Pair("networksolps",  GetNetworkHashPS(120, -1)));
    obj.push_back(Pair("testnet",       Params().TestnetToBeDeprecatedFieldRPC()));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
        obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    }
    if (pwalletMain && pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
#endif
    obj.push_back(Pair("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}

UniValue mnsync(const UniValue& params, bool fHelp)
{
    std::string strMode;
    if (params.size() == 1)
        strMode = params[0].get_str();

    if (fHelp || params.size() != 1 || (strMode != "status" && strMode != "reset")) {
        throw runtime_error(
            "mnsync \"status|reset\"\n"
            "\nReturns the sync status or resets sync.\n"

            "\nArguments:\n"
            "1. \"mode\"    (string, required) either 'status' or 'reset'\n"

            "\nResult ('status' mode):\n"
            "{\n"
            "  \"IsBlockchainSynced\": true|false,    (boolean) 'true' if blockchain is synced\n"
            "  \"lastMasternodeList\": xxxx,        (numeric) Timestamp of last MN list message\n"
            "  \"lastMasternodeWinner\": xxxx,      (numeric) Timestamp of last MN winner message\n"
            "  \"lastBudgetItem\": xxxx,            (numeric) Timestamp of last MN budget message\n"
            "  \"lastFailure\": xxxx,           (numeric) Timestamp of last failed sync\n"
            "  \"nCountFailures\": n,           (numeric) Number of failed syncs (total)\n"
            "  \"sumMasternodeList\": n,        (numeric) Number of MN list messages (total)\n"
            "  \"sumMasternodeWinner\": n,      (numeric) Number of MN winner messages (total)\n"
            "  \"sumBudgetItemProp\": n,        (numeric) Number of MN budget messages (total)\n"
            "  \"sumBudgetItemFin\": n,         (numeric) Number of MN budget finalization messages (total)\n"
            "  \"countMasternodeList\": n,      (numeric) Number of MN list messages (local)\n"
            "  \"countMasternodeWinner\": n,    (numeric) Number of MN winner messages (local)\n"
            "  \"countBudgetItemProp\": n,      (numeric) Number of MN budget messages (local)\n"
            "  \"countBudgetItemFin\": n,       (numeric) Number of MN budget finalization messages (local)\n"
            "  \"RequestedMasternodeAssets\": n, (numeric) Status code of last sync phase\n"
            "  \"RequestedMasternodeAttempt\": n, (numeric) Status code of last sync attempt\n"
            "}\n"

            "\nResult ('reset' mode):\n"
            "\"status\"     (string) 'success'\n"
            "\nExamples:\n" +
            HelpExampleCli("mnsync", "\"status\"") + HelpExampleRpc("mnsync", "\"status\""));
    }

    if (strMode == "status") {
        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("IsBlockchainSynced", masternodeSync.IsBlockchainSynced()));
        obj.push_back(Pair("lastMasternodeList", masternodeSync.lastMasternodeList));
        obj.push_back(Pair("lastMasternodeWinner", masternodeSync.lastMasternodeWinner));
        obj.push_back(Pair("lastBudgetItem", masternodeSync.lastBudgetItem));
        obj.push_back(Pair("lastFailure", masternodeSync.lastFailure));
        obj.push_back(Pair("nCountFailures", masternodeSync.nCountFailures));
        obj.push_back(Pair("sumMasternodeList", masternodeSync.sumMasternodeList));
        obj.push_back(Pair("sumMasternodeWinner", masternodeSync.sumMasternodeWinner));
        obj.push_back(Pair("sumBudgetItemProp", masternodeSync.sumBudgetItemProp));
        obj.push_back(Pair("sumBudgetItemFin", masternodeSync.sumBudgetItemFin));
        obj.push_back(Pair("countMasternodeList", masternodeSync.countMasternodeList));
        obj.push_back(Pair("countMasternodeWinner", masternodeSync.countMasternodeWinner));
        obj.push_back(Pair("countBudgetItemProp", masternodeSync.countBudgetItemProp));
        obj.push_back(Pair("countBudgetItemFin", masternodeSync.countBudgetItemFin));
        obj.push_back(Pair("RequestedMasternodeAssets", masternodeSync.RequestedMasternodeAssets));
        obj.push_back(Pair("RequestedMasternodeAttempt", masternodeSync.RequestedMasternodeAttempt));

        return obj;
    }

    if (strMode == "reset") {
        masternodeSync.Reset();
        return "success";
    }
    return "failure";
}

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const CNoDestination &dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID &keyID) const {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        if (pwalletMain && pwalletMain->GetPubKey(keyID, vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    UniValue operator()(const CScriptID &scriptID) const {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        if (pwalletMain && pwalletMain->GetCScript(scriptID, subscript)) {
            std::vector<CTxDestination> addresses;
            txnouttype whichType;
            int nRequired;
            ExtractDestinations(subscript, whichType, addresses, nRequired);
            obj.push_back(Pair("script", GetTxnOutputType(whichType)));
            obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
            UniValue a(UniValue::VARR);
            for (const CTxDestination& addr : addresses) {
                a.push_back(EncodeDestination(addr));
            }
            obj.push_back(Pair("addresses", a));
            if (whichType == TX_MULTISIG)
                obj.push_back(Pair("sigsrequired", nRequired));
        }
        return obj;
    }
};
#endif

/*
    Used for updating/reading spork settings on the network
*/
UniValue spork(const UniValue& params, bool fHelp)
{
    if (params.size() == 1 && params[0].get_str() == "show") {
        UniValue ret(UniValue::VOBJ);
        for (int nSporkID = SPORK_START; nSporkID <= SPORK_END; nSporkID++) {
            if (sporkManager.GetSporkNameByID(nSporkID) != "Unknown")
                ret.push_back(Pair(sporkManager.GetSporkNameByID(nSporkID), GetSporkValue(nSporkID)));
        }
        return ret;
    } else if (params.size() == 1 && params[0].get_str() == "active") {
        UniValue ret(UniValue::VOBJ);
        for (int nSporkID = SPORK_START; nSporkID <= SPORK_END; nSporkID++) {
            if (sporkManager.GetSporkNameByID(nSporkID) != "Unknown")
                ret.push_back(Pair(sporkManager.GetSporkNameByID(nSporkID), IsSporkActive(nSporkID)));
        }
        return ret;
    } else if (params.size() == 2) {
        int nSporkID = sporkManager.GetSporkIDByName(params[0].get_str());
        if (nSporkID == -1) {
            return "Invalid spork name";
        }

        // SPORK VALUE
        int64_t nValue = params[1].get_int();

        //broadcast new spork
        if (sporkManager.UpdateSpork(nSporkID, nValue)) {
            return "success";
        } else {
            return "failure";
        }
    }

    throw runtime_error(
        "spork <name> [<value>]\n"
        "<name> is the corresponding spork name, or 'show' to show all current spork settings, active to show which sporks are active"
        "<value> is a epoch datetime to enable or disable spork" +
        HelpRequiringPassphrase());
}
UniValue validateaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress \"vidulumaddress\"\n"
            "\nReturn information about the given Vidulum address.\n"
            "\nArguments:\n"
            "1. \"vidulumaddress\"     (string, required) The Vidulum address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,         (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"vidulumaddress\",   (string) The Vidulum address validated\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"ismine\" : true|false,          (boolean) If the address is yours or not\n"
            "  \"isscript\" : true|false,        (boolean) If the key is a script\n"
            "  \"pubkey\" : \"publickeyhex\",    (string) The hex value of the raw public key\n"
            "  \"iscompressed\" : true|false,    (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"         (string) DEPRECATED. The account associated with the address, \"\" is the default account\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        );

#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
#else
    LOCK(cs_main);
#endif

    CTxDestination dest = DecodeDestination(params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        std::string currentAddress = EncodeDestination(dest);
        ret.push_back(Pair("address", cu