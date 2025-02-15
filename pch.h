#ifndef PCH_H
#define PCH_H

// Standard Libraries
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <winsock2.h>
#include <deque>
#include <direct.h>  // Include for _mkdir on Windows
#include <fstream>

using namespace std;
// using namespace boost;

// Qt Libraries
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QTextStream>
#include <QMessageBox>

// RocksDB Libraries
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>

// OpenSSL Libraries
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// regex library
#include <boost/regex.hpp>

// json librayr
#include <nlohmann/json.hpp>


// Windows-specific Libraries (if you're targeting Windows)
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include <cryptopp/sha.h>        // For SHA256
#include <cryptopp/misc.h>       // For ByteReverse

// Boost Libraries (you mentioned boost is being used in multiple headers)
#include <boost/type_traits/is_fundamental.hpp>

// AI headers
#include "unistd.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ai_utils.h"
#include "ai_rand.h"
#include "ai_dataloader.h"
#include "ai_tokenizer.h"
#include "tokenizer.h"
using namespace TinyTokenizer;

// Project-Specific Headers
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "key.h"
#include "bignum.h"
#include "base58.h"
#include "script.h"
#include "train_gpt2.h"
#include "db.h"
#include "net.h"
#include "irc.h"
#include "main.h"
#include "market.h"


#endif  // PCH_H
