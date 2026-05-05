#pragma once

#ifndef GLOBAL_CONFIG
#define GLOBAL_CONFIG
#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
using boost::multiprecision::uint128_t;
using boost::multiprecision::uint256_t;

#define LOCALHOST
#define DEBUG_MODE
#define USE_DICF
// #define TREE_COMM_PROFILE
#define __MORE_FLUSH
inline const char *IP[] = {"127.0.0.1", "127.0.0.1", "127.0.0.1"};

#ifdef FORCE_SHARE_VALUE_128
typedef __uint128_t ShareValue;
#else
typedef uint32_t ShareValue;
#endif
inline const int ShareValue_BitLength = sizeof(ShareValue) * 8;

#ifdef FORCE_LONG_SHARE_VALUE_256
typedef uint256_t LongShareValue;
#else
typedef uint128_t LongShareValue;
#endif
inline const int LongShareValue_BitLength = sizeof(LongShareValue) * 8;

#endif
