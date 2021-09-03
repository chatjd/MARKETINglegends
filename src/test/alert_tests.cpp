// Copyright (c) 2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for alert system
//

#include "alert.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "data/alertTests.raw.h"

#include "main.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utilstrencodings.h"

#include "test/test_bitcoin.h"

#include <fstream>

#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include "key.h"
#include "alertkeys.h"
#include <iostream>

/*
 * If the alert key pairs have changed, the test suite will fail as the
 * test data is now invalid.  To create valid test data, signed with a
 * new alert private key, follow these steps:
 *
 * 1. Copy your private key into alertkeys.h.  Don't commit this file!
 *    See sendalert.cpp for more info.
 *
 * 2. Set the GENERATE_ALERTS_FLAG to true.
 *
 * 3. Build and run:
 *    test_bitcoin -t Generate_Alert_Test_Data
 *
 * 4. Test data is saved in your current directory as alertTests.raw.NEW
 *    Copy this file to: src/test/data/alertTests.raw
 *
 *    For debugging purposes, terminal output can be copied into:
 *    src/test/data/alertTests.raw.h
 *
 * 5. Clean up...
 *    - Set GENERATE_ALERTS_FLAG back to false.
 *    - Remove your private key from alertkeys.h
 *
 * 6. Build and verify the new test data:
 *    test_bitcoin -t Alert_tests
 *
 */
#define GENERATE_ALERTS_FLAG false

#if GENERATE_ALERTS_FLAG

// NOTE:
// A function SignAndSave() was used by Bitcoin Core to create alert test data
// but it has not been made publicly available.  So instead, we have adapted
// some publicly available code which achieves the intended result:
// https://gist.github.com/lukem512/9b272bd35e2cdefbf386


// Code to output a C-style array of values
template<typename T>
std::string HexStrArray(const T itbegin, const T itend, int lineLength)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    int i = 0;
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(it != itbegin)
        {
            if (i % lineLength == 0)
                rv.push_back('\n');
            else
                rv.push_back(' ');
        }
        rv.push_back('0');
        rv.push_back('x');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
        rv.push_back(',');
        i++;
    }

    return rv;
}

template<typename T>
inline std::string HexStrArray(const T& vch, int lineLength)
{
    return HexStrArray(vch.begin(), vch.end(), lineLength);
}


// Sign CAlert with alert private key
bool SignAlert(CAlert &alert)
{
    // serialize alert data
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << *(CUnsignedAlert*)&alert;
    alert.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    // sign alert
    std::vector<unsigned char> vchTmp(ParseHex(pszPrivKey));
    CPrivKey vchPrivKey(vchTmp.begin(), vchTmp.end());
    CKey key;
    if (!key.SetPrivKey(vchPrivKey, false))
    {
        printf("key.SetPrivKey failed\n");
        return false;
    }
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
    {
        printf("SignAlert() : key.Sign failed\n");
        return false;
    }
    return true;
}

// Sign a CAlert and serialize it
bool SignAndSerialize(CAlert &alert, CDataStream &buffer)
{
    // Sign
    if(!SignAlert(alert))
    {
        printf("SignAndSerialize() : could not sign alert\n");
        return false;
    }
    // ...and save!
    buffer << alert;
    return true;
}

void GenerateAlertTests()
{
    CDataStream sBuffer(SER_DISK, CLIENT_VERSION);

    CAlert alert;
    alert.nRelayUntil   = 60;
    alert.nExpiration   = 24 * 60 * 60;
    alert.nID           = 1;
    alert.nCancel       = 0;  // cancels previous messages up to this ID number
    alert.nMinVer       = 0;  // These versions are protocol versions
    alert.nMaxVer       = 999001;
    alert.nPriority     = 1;
    alert.strComment    = "Alert comment";
    alert.strStatusBar  = "Alert 1";

    // Replace SignAndSave with SignAndSerialize
    SignAndSerialize(alert, sBuffer);

    // More tests go here ...
    alert.setSubVer.insert(std::string("/MagicBean:0.1.0/"));
    alert.strStatusBar  = "Alert 1 for MagicBean 0.1.0";
    SignAndSerialize(alert, sBuffer);

    alert.setSubVer.insert(std::string("/MagicBean:0.2.0/"));
    alert.strStatusBar  = "Alert 1 for MagicBean 0.1.0, 0.2.0";
    SignAndSerialize(alert, sBuffer);

    alert.setSubVer.clear();
    ++alert.nID;
    alert.nCancel = 1;
    alert.nPriority = 100;
    alert.strStatusBar  = "Alert 2, cancels 1";
    SignAndSerialize(alert, sBuffer);

    alert.nExpiration += 60;
    ++alert.nID;
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nPriority = 5000;
    alert.strStatusBar  = "Alert 3, disables RPC";
    alert.strRPCError = "RPC disabled";
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nPriority = 5000;
    alert.strStatusBar  = "Alert 4, re-enables RPC";
    alert.strRPCError = "";
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nMinVer = 11;
    alert.nMaxVer = 22;
    alert.nPriority = 100;
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.strStatusBar  = "Alert 2 for MagicBean 0.1.0";
    alert.setSubVer.insert(std::string("/MagicBean:0.1.0/"));
    SignAndSerialize(alert, sBuffer);

    ++alert.nID;
    alert.nMinVer = 0;
    alert.nMaxVer = 999999;
    alert.strStatusBar  = "Evil Alert'; /bin/ls; echo '";
    alert.setSubVer.clear();
    bool b = SignAndSerialize(alert, sBuffer);

    if (b) {
        // Print the hex array, which will become the contents of alertTest.raw.h
        std::vector<unsigned char> vch = std::vector<unsigned char>(sBuffer.begin(), sBuffer.end());
        printf("%s\n", HexStrArray(vch, 8).c_str());

        // Write the data to alertTests.raw.NEW, to be copied to src/test/data/alertTests.raw
        std::ofstream outfile("alertTests.raw.NEW", std::ios::out | std::ios::binary);
        outfile.write((const char*)&vch[0], vch.size());
        outfile.close();
    }
}



struct GenerateAlertTestsFixture : public TestingS