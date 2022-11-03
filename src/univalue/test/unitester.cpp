// Copyright 2014 BitPay Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cassert>
#include <string>
#include "univalue.h"

#ifndef JSON_TEST_SRC
#error JSON_TEST_SRC must point to test source directory
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

using namespace std;
string srcdir(JSON_TEST_SRC);
static bool test_failed = false;

#define d_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s failed\n", filename.c_str()); } }
#define f_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s failed\n", __func__); } }

static std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

static void runtest(string filename, const 