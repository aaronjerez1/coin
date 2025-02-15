

namespace fs = std::filesystem;

// Mutex to ensure thread safety for RocksDB operations
static std::mutex cs_db;
static bool fDbEnvInit = false;
static std::map<std::string, int> mapFileUseCount;
map<string, string> mapAddressBook;

// Define the global RocksDB environment
rocksdb::DB* dbenv = nullptr;

class CDBInit {
public:
    CDBInit() {}
    ~CDBInit() {
        if (fDbEnvInit) {
            delete dbenv;
            fDbEnvInit = false;
        }
    }
};
CDBInit instance_of_cdbinit;

// Constructor for CDB class/
CDB::CDB(const char* pszFile, bool fTxn) : pdb(nullptr) {
    if (pszFile == nullptr) {
        return;
    }

    bool fCreate = true;  // RocksDB always allows creating a new DB if it doesn't exist

    std::lock_guard<std::mutex> lock(cs_db);

    if (!fDbEnvInit) {
        std::string strAppDir = "database";
        if (!fs::exists(strAppDir)) {
            fs::create_directory(strAppDir);
        }
        std::cout << "dbenv.open strAppDir=" << strAppDir << std::endl;

        rocksdb::Options options;
        options.create_if_missing = true;
        options.create_missing_column_families = true;

        rocksdb::Status status = rocksdb::DB::Open(options, strAppDir, &dbenv);
        if (!status.ok()) {
            throw std::runtime_error("CDB() : error opening database environment: " + status.ToString());
        }
        fDbEnvInit = true;
    }

    strFile = pszFile;
    ++mapFileUseCount[strFile];

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::Status status = rocksdb::DB::Open(options, pszFile, &pdb);
    if (!status.ok()) {
        std::cerr << "Error opening RocksDB: " << status.ToString() << std::endl;
        pdb = nullptr;
        --mapFileUseCount[strFile];
        throw std::runtime_error("CDB() : can't open database file " + strFile + ", error: " + status.ToString());
    }

    // Ensure that version is written to the database
    if (fCreate && !Exists(std::string("version"))) {
        WriteVersion(1);
    }
}

// Destructor for CDB class
// CDB::~CDB() {
//     Close();
// }

// Method to close the database
void CDB::Close() {
    if (!pdb) {
        return;
    }

    delete pdb;
    pdb = nullptr;

    std::lock_guard<std::mutex> lock(cs_db);
    --mapFileUseCount[strFile];

    // Flush remaining changes to disk
    if (dbenv) {
        dbenv->FlushWAL(true);
    }
}

// Commit a transaction batch

// Implementation of TxnBegin
bool CDB::TxnBegin() {
    if (!pdb)
        return false;
    // Create a new WriteBatch transaction
    vTxn.push_back(rocksdb::WriteBatch());
    return true;
}

// Implementation of TxnCommit
bool CDB::TxnCommit() {
    if (!pdb || vTxn.empty())
        return false;

    rocksdb::WriteBatch& batch = vTxn.back();  // Get the last transaction batch
    rocksdb::WriteOptions write_options;
    rocksdb::Status s = pdb->Write(write_options, &batch);  // Commit the transaction
    vTxn.pop_back();  // Remove the last transaction batch

    return s.ok();
}


// Implementation of TxnAbort
bool CDB::TxnAbort() {
    if (!pdb || vTxn.empty())
        return false;

    // We simply discard the last transaction by popping it
    vTxn.pop_back();
    return true;
}

// Implementation of ReadVersion
bool CDB::ReadVersion(int& nVersion) {
    nVersion = 0;
    return Read(std::string("version"), nVersion);
}

// Implementation of WriteVersion
bool CDB::WriteVersion(int nVersion) {
    return Write(std::string("version"), nVersion);
}



// Function to flush the database environment
void DBFlush(bool fShutdown) {
    std::lock_guard<std::mutex> lock(cs_db);
    if (dbenv) {
        // Perform a WAL checkpoint to persist all the writes
        dbenv->FlushWAL(true);

        if (fShutdown) {
            delete dbenv;
            dbenv = nullptr;
            fDbEnvInit = false;
        }
    }
}



bool CAddrDB::WriteAddress(const CAddress& addr) {
    // Serialize the address key and the address object
    return Write(std::make_pair(std::string("addr"), addr.GetKey()), addr);
}
bool CAddrDB::LoadAddresses() {
    // Locks for critical sections can be handled with std::mutex if necessary
    CRITICAL_BLOCK(cs_mapIRCAddresses)
    CRITICAL_BLOCK(cs_mapAddresses)
    {

        // Load user-provided addresses from a file (addr.txt)
        std::ifstream filein("addr.txt");
        if (filein.is_open()) {
            std::string line;
            while (std::getline(filein, line)) {
                CAddress addr(line.c_str(), NODE_NETWORK);
                if (addr.ip != 0) {
                    AddAddress(*this, addr);
                    mapIRCAddresses.insert(std::make_pair(addr.GetKey(), addr));
                }
            }
            filein.close();
        }

        // Create a RocksDB iterator to read the data from the database
        rocksdb::Iterator* it = pdb->NewIterator(rocksdb::ReadOptions());

        // We want to find all keys starting with "addr"
        std::string prefix = "addr";
        for (it->Seek(prefix); it->Valid() && it->key().ToString().compare(0, prefix.size(), prefix) == 0; it->Next()) {
            // Convert the RocksDB key and value into a byte stream
            std::string strKey = it->key().ToString();
            std::string strValue = it->value().ToString();

            // Construct CDataStream objects using the appropriate constructor
            CDataStream ssKey((const char*)strKey.data(), (const char*)strKey.data() + strKey.size(), SER_DISK, VERSION);
            CDataStream ssValue((const char*)strValue.data(), (const char*)strValue.data() + strValue.size(), SER_DISK, VERSION);

            std::string strType;
            ssKey >> strType;

            if (strType == "addr") {
                CAddress addr;
                ssValue >> addr;
                mapAddresses.insert(std::make_pair(addr.GetKey(), addr));
            }
        }



        // Clean up the iterator
        delete it;

        // Debug print
        printf("mapAddresses:\n");
        for (const auto& item : mapAddresses) {
            item.second.print();
        }
        printf("-----\n");

    }

    return true;
}

//
// CTxDB
//

bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    txindex.SetNull();
    return Read(std::make_pair(std::string("tx"), hash), txindex);
}

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    return Write(std::make_pair(std::string("tx"), hash), txindex);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(std::make_pair(std::string("tx"), hash), txindex);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();
    return Erase(std::make_pair(std::string("tx"), hash));
}

bool CTxDB::ContainsTx(uint256 hash)
{
    return Exists(std::make_pair(std::string("tx"), hash));
}
bool CTxDB::ReadOwnerTxes(uint160 hash160, int nMinHeight, std::vector<CTransaction>& vtx)
{
    vtx.clear();

    rocksdb::Iterator* it = pdb->NewIterator(rocksdb::ReadOptions());
    std::string prefix = Serialize(std::make_pair(std::string("owner"), hash160));

    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
        // Convert the RocksDB key and value from string to a vector of unsigned chars
        std::vector<unsigned char> keyData(it->key().data(), it->key().data() + it->key().size());
        std::vector<unsigned char> valueData(it->value().data(), it->value().data() + it->value().size());

        // Create CDataStream objects from these vectors
        CDataStream ssKey(keyData, SER_DISK, VERSION);
        CDataStream ssValue(valueData, SER_DISK, VERSION);

        std::string strType;
        uint160 hashItem;
        CDiskTxPos pos;
        ssKey >> strType >> hashItem >> pos;

        int nItemHeight;
        ssValue >> nItemHeight;

        if (nItemHeight >= nMinHeight)
        {
            vtx.resize(vtx.size() + 1);
            if (!vtx.back().ReadFromDisk(pos))
                return false;
        }
    }

    delete it;
    return true;
}


bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}




bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(std::make_pair(std::string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool CTxDB::EraseBlockIndex(uint256 hash)
{
    return Erase(std::make_pair(std::string("blockindex"), hash));
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(std::string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(std::string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteModelParameters(const GPT2* model) {
    std::string paramsKey = "modelParams";

    // Serialize the parameters into a binary format
    CDataStream ssParams(SER_DISK, VERSION);
    ssParams.write((char*)model->params_memory, model->num_parameters * sizeof(float));

    return Write(paramsKey, ssParams.str());
}

bool CTxDB::WriteModelHeader(const GPT2* model) {
    std::string headerKey = "modelHeader";

    // Serialize the header data (e.g., hyperparameters like max_seq_len, vocab_size, etc.)
    CDataStream ssHeader(SER_DISK, VERSION);
    ssHeader << model->config.max_seq_len << model->config.vocab_size
             << model->config.padded_vocab_size << model->config.num_layers
             << model->config.num_heads << model->config.channels;

    return Write(headerKey, ssHeader.str());
}


CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    auto mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw std::runtime_error("LoadBlockIndex() : new CBlockIndex failed");

    mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}


bool CTxDB::LoadBlockIndex() {
    // Create a RocksDB iterator
    rocksdb::Iterator* it = pdb->NewIterator(rocksdb::ReadOptions());

    // Prefix for block index entries
    std::string prefix = Serialize(std::make_pair(std::string("blockindex"), uint256(0)));

    // Use the iterator to seek and load block index records
    for (it->Seek(prefix); it->Valid(); it->Next()) {
        // Read the key and value from the RocksDB iterator
        std::vector<unsigned char> keyData(it->key().data(), it->key().data() + it->key().size());
        std::vector<unsigned char> valueData(it->value().data(), it->value().data() + it->value().size());

        // Ensure that keyData and valueData are not empty before deserialization
        if (keyData.empty() || valueData.empty()) {
            std::cerr << "Error: Empty key or value data, skipping..." << std::endl;
            continue;
        }

        try {
            // Construct CDataStream objects using raw pointers
            CDataStream ssKey((const char*)keyData.data(), (const char*)keyData.data() + keyData.size(), SER_DISK, VERSION);
            CDataStream ssValue((const char*)valueData.data(), (const char*)valueData.data() + valueData.size(), SER_DISK, VERSION);

            std::string strType;
            ssKey >> strType;  // Deserialize the string that represents the type of data

            // Check if the key is for a block index
            if (strType == "blockindex") {
                CDiskBlockIndex diskindex;
                ssValue >> diskindex;  // Deserialize the disk index

                // Insert the new block index into the map
                CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev = InsertBlockIndex(diskindex.hashPrev);
                pindexNew->pnext = InsertBlockIndex(diskindex.hashNext);
                pindexNew->nFile = diskindex.nFile;
                pindexNew->nBlockPos = diskindex.nBlockPos;
                pindexNew->nHeight = diskindex.nHeight;
                pindexNew->nVersion = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime = diskindex.nTime;
                pindexNew->nBits = diskindex.nBits;
                pindexNew->nNonce = diskindex.nNonce;

                // Check for the genesis block
                if (pindexGenesisBlock == NULL && diskindex.GetBlockHash() == hashGenesisBlock) {
                    pindexGenesisBlock = pindexNew;
                }
            } else {
                break;  // If we encounter a non-blockindex key, exit the loop
            }
        } catch (const std::exception& e) {
            // Handle any deserialization errors
            std::cerr << "Error deserializing block index: " << e.what() << std::endl;
            continue;
        }
    }

    // Clean up the iterator
    delete it;

    // Read the best chain hash from the database
    if (!ReadHashBestChain(hashBestChain)) {
        if (pindexGenesisBlock == NULL)
            return true;  // If there's no genesis block, just return true
        throw std::runtime_error("CTxDB::LoadBlockIndex() : hashBestChain not found");
    }

    // Ensure the best chain block index exists
    if (!mapBlockIndex.count(hashBestChain)) {
        throw std::runtime_error("CTxDB::LoadBlockIndex() : blockindex for hashBestChain not found");
    }

    // Set the best block
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;

    // Log the results
    std::cout << "LoadBlockIndex(): hashBestChain=" << hashBestChain.ToString().substr(0, 14)
              << "  height=" << nBestHeight << std::endl;

    return true;
}


// i think this is an incorrect version because it doesn't reads keys and values. but let's play with it.
// need to have a way to get a "model" object in here
bool CTxDB::LoadGPT2(GPT2 *model) {

    rocksdb::Iterator* it = pdb->NewIterator(rocksdb::ReadOptions());

    // Start by loading the model header to validate it
    std::string headerKey = "modelHeader";
    std::string headerValue;
    // rocksdb::Status status = pdb->Get(rocksdb::ReadOptions(), headerKey, &headerValue);

    if (headerValue.empty()) {
        std::cerr << "Error: Model header not found or is invalid" << std::endl;
        // Clean up the iterator
        delete it;

        return true;
    }

    // Deserialize the model header
    int model_header[256];
    memcpy(model_header, headerValue.data(), sizeof(int) * 256);

    // Validate the header
    if (model_header[0] != 20240326) {
        std::cerr << "Bad magic in model header." << std::endl;
        return false;
    }
    if (model_header[1] != 3) {
        std::cerr << "Incorrect model version." << std::endl;
        return false;
    }

    // Read hyperparameters
    size_t maxT, V, Vp, L, NH, C;
    model->config.max_seq_len = maxT = model_header[2];
    model->config.vocab_size = V = model_header[3];
    model->config.num_layers = L = model_header[4];
    model->config.num_heads = NH = model_header[5];
    model->config.channels = C = model_header[6];
    model->config.padded_vocab_size = Vp = model_header[7];

    printf("[GPT-2]\n");
    printf("max_seq_len: %zu\n", maxT);
    printf("vocab_size: %zu\n", V);
    printf("padded_vocab_size: %zu\n", Vp);
    printf("num_layers: %zu\n", L);
    printf("num_heads: %zu\n", NH);
    printf("channels: %zu\n", C);

    // Set up the model parameter sizes
    fill_in_parameter_sizes(model->param_sizes, model->config);

    // Calculate the total number of parameters
    size_t num_parameters = 0;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        num_parameters += model->param_sizes[i];
    }
    model->num_parameters = num_parameters;

    // Allocate memory for model parameters
    model->params_memory = malloc_and_point_parameters(&model->params, model->param_sizes);

    //
    // Now, iterate over the parameters in RocksDB to populate params_memory
    //

    std::string paramPrefix = "modelParams";
    for (it->Seek(paramPrefix); it->Valid() && it->key().starts_with(paramPrefix); it->Next()) {
        std::string paramValue = it->value().ToString();
        size_t paramIndex = std::stoi(it->key().ToString().substr(paramPrefix.size()));

        // Check if the index is within bounds
        if (paramIndex >= num_parameters) {
            std::cerr << "Parameter index out of bounds: " << paramIndex << std::endl;
            continue;
        }

        // Copy the parameter data into the appropriate place in params_memory
        memcpy(model->params_memory + paramIndex, paramValue.data(), paramValue.size());
    }

    delete it;

    // Initialize other fields
    model->acts_memory = nullptr;
    model->grads_memory = nullptr;
    model->m_memory = nullptr;
    model->v_memory = nullptr;
    model->grads_acts_memory = nullptr;
    model->inputs = nullptr;
    model->targets = nullptr;
    model->batch_size = 0;
    model->seq_len = 0;
    model->mean_loss = -1.0f;  // Default loss value

    return true;
}


bool LoadAddresses()
{
    return CAddrDB("cr+").LoadAddresses();
}


// UI settings and their default values
int minimizeToTray = 1;
int closeToTray = 1;
int startOnSysBoot = 1;
int askBeforeClosing = 1;
int alwaysShowTrayIcon = 1;

bool CWalletDB::LoadWallet(std::vector<unsigned char>& vchDefaultKeyRet)
{
    vchDefaultKeyRet.clear();

    // Use the old CRITICAL_BLOCK for thread safety
    CRITICAL_BLOCK(cs_mapKeys)
    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Create a RocksDB iterator
        rocksdb::Iterator* it = pdb->NewIterator(rocksdb::ReadOptions());

        // Iterate over all the entries in the wallet
        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            // Convert the RocksDB key and value into strings
            std::string strKey = it->key().ToString();
            std::string strValue = it->value().ToString();

            // Create CDataStream objects to deserialize the key-value pairs
            CDataStream ssKey(strKey.data(), strKey.data() + strKey.size(), SER_DISK, VERSION);
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, VERSION);

            // Unserialize key type
            std::string strType;
            ssKey >> strType;

            // Handle each type of entry in the wallet
            if (strType == "name")
            {
                std::string strAddress;
                ssKey >> strAddress;
                ssValue >> mapAddressBook[strAddress];
            }
            else if (strType == "tx")
            {
                uint256 hash;
                ssKey >> hash;
                CWalletTx& wtx = mapWallet[hash];
                ssValue >> wtx;

                if (wtx.GetHash() != hash)
                    printf("Error in wallet.dat, hash mismatch\n");
            }
            else if (strType == "key")
            {
                std::vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                CPrivKey vchPrivKey;
                ssValue >> vchPrivKey;

                mapKeys[vchPubKey] = vchPrivKey;
                mapPubKeys[Hash160(vchPubKey)] = vchPubKey;
            }
            else if (strType == "defaultkey")
            {
                ssValue >> vchDefaultKeyRet;
            }
            else if (strType == "setting")
            {
                std::string strSettingKey;
                ssKey >> strSettingKey;

                if (strSettingKey == "fGenerateCoins")  ssValue >> fGenerateCoins;
                if (strSettingKey == "nTransactionFee")    ssValue >> nTransactionFee;
                if (strSettingKey == "addrIncoming")       ssValue >> addrIncoming;
                if (strSettingKey == "minimizeToTray")     ssValue >> minimizeToTray;
                if (strSettingKey == "closeToTray")        ssValue >> closeToTray;
                if (strSettingKey == "startOnSysBoot")     ssValue >> startOnSysBoot;
                if (strSettingKey == "askBeforeClosing")   ssValue >> askBeforeClosing;
                if (strSettingKey == "alwaysShowTrayIcon") ssValue >> alwaysShowTrayIcon;
            }
        }

        // Clean up the iterator
        delete it;

        // Debug print
        printf("fGenerateCoins = %d\n", fGenerateCoins);
        printf("nTransactionFee = %I64d\n", nTransactionFee);
        printf("addrIncoming = %s\n", addrIncoming.ToString().c_str());
    }

    return true;
}

bool LoadWallet()
{
    std::vector<unsigned char> vchDefaultKey;

    // Read the wallet from RocksDB (similar to CWalletDB("cr").LoadWallet(vchDefaultKey))
    if (!CWalletDB("cr").LoadWallet(vchDefaultKey)) {
        return false;
    }

    // Check if the default key is already present in the mapKeys
    if (mapKeys.count(vchDefaultKey)) {
        // Set the public and private keys for keyUser
        keyUser.SetPubKey(vchDefaultKey);
        keyUser.SetPrivKey(mapKeys[vchDefaultKey]);
    } else {
        // Generate a new keyUser as the default key
        RandAddSeed(true);  // Adding entropy for random number generation
        keyUser.MakeNewKey();  // Create a new key

        // Add the new key to the wallet
        if (!AddKey(keyUser)) {
            return false;
        }

        // Set the address book name for the default key
        if (!SetAddressBookName(PubKeyToAddress(keyUser.GetPubKey()), "Your Address")) {
            return false;
        }

        // Write the new default key to the wallet database
        if (!CWalletDB().WriteDefaultKey(keyUser.GetPubKey())) {
            return false;
        }
    }

    return true;
}
