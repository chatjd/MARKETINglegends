// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STREAMS_H
#define BITCOIN_STREAMS_H

#include "support/allocators/zeroafterfree.h"
#include "serialize.h"

#include <algorithm>
#include <assert.h>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <utility>
#include <vector>

template<typename Stream>
class OverrideStream
{
    Stream* stream;

    const int nType;
    const int nVersion;

public:
    OverrideStream(Stream* stream_, int nType_, int nVersion_) : stream(stream_), nType(nType_), nVersion(nVersion_) {}

    template<typename T>
    OverrideStream<Stream>& operator<<(const T& obj)
    {
        // Serialize to this stream
        ::Serialize(*this, obj);
        return (*this);
    }

    template<typename T>
    OverrideStream<Stream>& operator>>(T&& obj)
    {
        // Unserialize from this stream
        ::Unserialize(*this, obj);
        return (*this);
    }

    void write(const char* pch, size_t nSize)
    {
        stream->write(pch, nSize);
    }

    void read(char* pch, size_t nSize)
    {
        stream->read(pch, nSize);
    }

    int GetVersion() const { return nVersion; }
    int GetType() const { return nType; }
};

template<typename S>
OverrideStream<S> WithVersion(S* s, int nVersion)
{
    return OverrideStream<S>(s, s->GetType(), nVersion);
}

/** Double ended buffer combining vector and stream-like interfaces.
 *
 * >> and << read and write unformatted data using the above serialization templates.
 * Fills with data in linear time; some stringstream implementations take N^2 time.
 */
template<typename SerializeType>
class CBaseDataStream
{
protected:
    typedef SerializeType vector_type;
    vector_type vch;
    unsigned int nReadPos;

    int nType;
    int nVersion;
public:

    typedef typename vector_type::allocator_type   allocator_type;
    typedef typename vector_type::size_type        size_type;
    typedef typename vector_type::difference_type  difference_type;
    typedef typename vector_type::reference        reference;
    typedef typename vector_type::const_reference  const_reference;
    typedef typename vector_type::value_type       value_type;
    typedef typename vector_type::iterator         iterator;
    typedef typename vector_type::const_iterator   const_iterator;
    typedef typename vector_type::reverse_iterator reverse_iterator;

    explicit CBaseDataStream(int nTypeIn, int nVersionIn)
    {
        Init(nTypeIn, nVersionIn);
    }

    CBaseDataStream(const_iterator pbegin, const_iterator pend, int nTypeIn, int nVersionIn