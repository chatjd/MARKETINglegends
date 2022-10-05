// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serialize.h"
#include "streams.h"
#include "hash.h"
#include "test/test_bitcoin.h"
#include "utilstrencodings.h"

#include <array>
#include <stdint.h>

#include <boost/test/unit_test.hpp>
#include <boost/optional.hpp>

using namespace std;


template<typename T>
void check_ser_rep(T thing, std::vector<unsigned char> expected)
{
    CDataStream ss(SER_DISK, 0);
    ss << thing;

    BOOST_CHECK(GetSerializeSize(thing, 0, 0) == ss.size());

    std::vector<unsigned char> serialized_representation(ss.begin(), ss.end());

    BOOST_CHECK(serialized_representation == expected);

    T thing_deserialized;
    ss >> thing_deserialized;

    BOOST_CHECK(thing_deserialized == thing);
}

BOOST_FIXTURE_TEST_SUITE(serialize_tests, BasicTestingSetup)

class CSerializeMethodsTestSingle
{
protected:
    int intval;
    bool boolval;
    std::string stringval;
    const char* charstrval;
    CTransaction txval;
public:
    CSerializeMethodsTestSingle() = default;
    CSerializeMethodsTestSingle(int intvalin, bool boolvalin, std::string stringvalin, const char* charstrvalin, CTransaction txvalin) : intval(intvalin), boolval(boolvalin), stringval(std::move(stringvalin)), charstrval(charstrvalin), txval(txvalin){}
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(intval);
        READWRITE(boolval);
        READWRITE(stringval);
        READWRITE(FLATDATA(charstrval));
        READWRITE(txval);
    }

    bool operator==(const CSerializeMethodsTestSingle& rhs)
    {
        return  intval == rhs.intval && \
                boolval == rhs.boolval && \
                stringval == rhs.stringval && \
                strcmp(charstrval, rhs.charstrval) == 0 && \
                txval == rhs.txval;
    }
};

class CSerializeMethodsTestMany : public CSerializeMethodsTestSingle
{
public:
    using CSerializeMethodsTestSingle::CSerializeMethodsTestSingle;
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_