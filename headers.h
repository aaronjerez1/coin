// #ifndef HEADERS_H
// #define HEADERS_H
// // Distributed under the MIT/X11 software license, see the accompanying
// // file license.txt or http://www.opensource.org/licenses/mit-license.php.

// #ifdef _MSC_VER
// #pragma warning(disable:4786)  // Disable long symbol name warnings
// #pragma warning(disable:4804)  // Disable unsafe conversion warnings
// #pragma warning(disable:4717)  // Disable recursion warnings
// #endif

// #ifdef _WIN32_WINNT
// #undef _WIN32_WINNT
// #endif
// #define _WIN32_WINNT 0x0500  // Target Windows 2000 or later

// #ifdef _WIN32_IE
// #undef _WIN32_IE
// #endif
// #define _WIN32_IE 0x0500  // Target Internet Explorer 5.0 or later

// #define WIN32_LEAN_AND_MEAN 1  // Exclude rarely-used services from Windows headers

// // Standard and system headers
// #include <windows.h>
// #include <winsock2.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <io.h>
// #include <math.h>
// #include <assert.h>
// #include <process.h>
// #include <shlobj.h>
// #include <memory>

// // OpenSSL headers for cryptographic functions
// #include <openssl/ecdsa.h>
// #include <openssl/evp.h>
// #include <openssl/rand.h>
// #include <openssl/sha.h>
// #include <openssl/ripemd.h>

// // STL headers
// #include <sstream>
// #include <string>
// #include <vector>
// #include <map>
// #include <set>
// #include <algorithm>

// // Boost headers (only include what's needed)
// #include <boost/foreach.hpp>
// #include <boost/lexical_cast.hpp>

// // Project-specific headers
// #include "serialize.h"
// #include "uint256.h"
// #include "util.h"
// #include "key.h"
// #include "bignum.h"
// #include "base58.h"
// #include "script.h"
// #include "db.h"
// #include "net.h"
// #include "main.h"

// // Stop precompiled header processing (if necessary for your project setup)
// #pragma hdrstop

// #endif // HEADERS_H
