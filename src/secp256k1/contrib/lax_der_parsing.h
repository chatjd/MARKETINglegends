/**********************************************************************
 * Copyright (c) 2015 Pieter Wuille                                   *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

/****
 * Please do not link this file directly. It is not part of the libsecp256k1
 * project and does not promise any stability in its API, functionality or
 * presence. Projects which use this code should instead copy this header
 * and its accompanying .c file directly into their codebase.
 ****/

/* This file defines a function that parses DER with various errors and
 * violations. This is not a part of the library itself, because the allowed
 * violations are chosen arbitrarily and do not follow or establish an