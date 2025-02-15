#ifndef DB_H
#define DB_H


class CTransaction;
class CTxIndex;
class CDiskBlockIndex;
class CDiskTxPos;
class COutPoint;
// class CUser;
class CReview;
class CAddress;
class CWalletTx;

using namespace std;

// Declare RocksDB environment
extern rocksdb::DB* dbenv; // Declare dbenv as extern (RocksDB doesn't need an environment object)
extern map<string, string> mapAddressBook;
extern bool fClient;

extern void DBFlush(bool fShutdown); // Needs reimplementation for RocksDB

// Serialization functions for RocksDB
template <typename T>
std::string Serialize(const T& obj) {
    std::ostringstream oss;
    oss.write(reinterpret_cast<const char*>(&obj), sizeof(T));
    return oss.str();
}

template <typename T>
void Deserialize(const std::string& data, T& obj) {
    std::istringstream iss(data);
    iss.read(reinterpret_cast<char*>(&obj), sizeof(T));
}

// Modernized `CDB` class using RocksDB
class CDB {
protected:
    rocksdb::DB* pdb;  // RocksDB instance
    std::string strFile;
    std::vector<rocksdb::WriteBatch> vTxn;  // Transaction batches

    explicit CDB(const char* pszFile, bool fTxn = false);
    ~CDB() { Close(); }

public:
    void Close();

    // Delete copy constructor and assignment operator to avoid copying the database handle
    CDB(const CDB&) = delete;
    void operator=(const CDB&) = delete;

protected:
    // Generic read/write/erase/exists functions for RocksDB
    template<typename K, typename T>
    bool Read(const K& key, T& value) {
        if (!pdb)
            return false;

        std::string strKey = Serialize(key);
        std::string strValue;

        // RocksDB read operation
        rocksdb::Status s = pdb->Get(rocksdb::ReadOptions(), strKey, &strValue);
        if (!s.ok())
            return false;

        Deserialize(strValue, value);
        return true;
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true) {
        if (!pdb)
            return false;

        std::string strKey = Serialize(key);
        std::string strValue = Serialize(value);

        // Write to RocksDB
        rocksdb::WriteOptions write_options;
        rocksdb::Status s = pdb->Put(write_options, strKey, strValue);
        return s.ok();
    }

    template<typename K>
    bool Erase(const K& key) {
        if (!pdb)
            return false;

        std::string strKey = Serialize(key);

        // Erase key-value pair from RocksDB
        rocksdb::WriteOptions write_options;
        rocksdb::Status s = pdb->Delete(write_options, strKey);
        return s.ok();
    }

    template<typename K>
    bool Exists(const K& key) {
        if (!pdb)
            return false;

        std::string strKey = Serialize(key);
        std::string strValue;

        // Check if key exists in RocksDB
        rocksdb::Status s = pdb->Get(rocksdb::ReadOptions(), strKey, &strValue);
        return s.ok();
    }

    // RocksDB transactions
    rocksdb::WriteBatch GetWriteBatch() {
        return rocksdb::WriteBatch();
    }

public:
    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort();
    bool ReadVersion(int& nVersion);
    bool WriteVersion(int nVersion);
};


// Transaction Database class using RocksDB
class CTxDB : public CDB {
public:
    CTxDB(const char* pszMode = "r+", bool fTxn = false) : CDB(!fClient ? "blkindex.dat" : nullptr, fTxn) {}

    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ReadOwnerTxes(uint160 hash160, int nHeight, vector<CTransaction>& vtx);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool EraseBlockIndex(uint256 hash);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool WriteModelHeader(const GPT2* model);
    bool WriteModelParameters(const GPT2* model);
    bool LoadBlockIndex();
    bool LoadGPT2(GPT2 *model);
};


// Address Database class using RocksDB
class CAddrDB : public CDB {
public:
    CAddrDB(const char* pszMode = "r+", bool fTxn = false) : CDB("addr.dat", fTxn) {}

    bool WriteAddress(const CAddress& addr);
    bool LoadAddresses();
};

bool LoadAddresses();

// Wallet Database class using RocksDB
class CWalletDB : public CDB {
public:
    CWalletDB(const char* pszMode = "r+", bool fTxn = false) : CDB("wallet.dat", fTxn) {}

    bool ReadName(const string& strAddress, string& strName) {
        strName = "";
        return Read(make_pair(string("name"), strAddress), strName);
    }

    bool WriteName(const string& strAddress, const string& strName) {
        mapAddressBook[strAddress] = strName;
        return Write(make_pair(string("name"), strAddress), strName);
    }

    bool EraseName(const string& strAddress) {
        mapAddressBook.erase(strAddress);
        return Erase(make_pair(string("name"), strAddress));
    }

    bool ReadTx(uint256 hash, CWalletTx& wtx) {
        return Read(make_pair(string("tx"), hash), wtx);
    }

    bool WriteTx(uint256 hash, const CWalletTx& wtx) {
        return Write(make_pair(string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash) {
        return Erase(make_pair(string("tx"), hash));
    }

    bool ReadKey(const vector<unsigned char>& vchPubKey, CPrivKey& vchPrivKey) {
        vchPrivKey.clear();
        return Read(make_pair(string("key"), vchPubKey), vchPrivKey);
    }

    bool WriteKey(const vector<unsigned char>& vchPubKey, const CPrivKey& vchPrivKey) {
        return Write(make_pair(string("key"), vchPubKey), vchPrivKey, false);
    }

    bool ReadDefaultKey(vector<unsigned char>& vchPubKey) {
        vchPubKey.clear();
        return Read(string("defaultkey"), vchPubKey);
    }

    bool WriteDefaultKey(const vector<unsigned char>& vchPubKey) {
        return Write(string("defaultkey"), vchPubKey);
    }

    template<typename T>
    bool ReadSetting(const string& strKey, T& value) {
        return Read(make_pair(string("setting"), strKey), value);
    }

    template<typename T>
    bool WriteSetting(const string& strKey, const T& value) {
        return Write(make_pair(string("setting"), strKey), value);
    }

    bool LoadWallet(vector<unsigned char>& vchDefaultKeyRet);
};

bool LoadWallet();

inline bool SetAddressBookName(const string& strAddress, const string& strName) {
    return CWalletDB().WriteName(strAddress, strName);
}

#endif // DB_H
