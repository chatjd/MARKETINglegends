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
            "  \"balance\": xxxxxxx,         (numeric) the total Vidulum balance