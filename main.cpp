// #include "main.h"
// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

// #include "headers.h"
// #include "sha.h"





//
// Global state
//

CCriticalSection cs_main;

map<uint256, CTransaction> mapTransactions;
CCriticalSection cs_mapTransactions;
unsigned int nTransactionsUpdated = 0;
map<COutPoint, CInPoint> mapNextTx;

map<uint256, CBlockIndex*> mapBlockIndex;
const uint256 /*hashGenesisBlock("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f");*/
// hashGenesisBlock("0x006e06f58616f55038144f973323b5f0752a28b5b31ef7d0c8c4079bf8214f9e");
hashGenesisBlock("f1111738a22917f78aa841427dc870905efd8e6027d4468a4ebabbd5b394651e");
CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;
uint256 hashBestChain = 0;
CBlockIndex* pindexBest = NULL;

map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;

map<uint256, CDataStream*> mapOrphanTransactions;
multimap<uint256, CDataStream*> mapOrphanTransactionsByPrev;

map<uint256, CWalletTx> mapWallet;
vector<pair<uint256, bool> > vWalletUpdated;
CCriticalSection cs_mapWallet;

map<vector<unsigned char>, CPrivKey> mapKeys;
map<uint160, vector<unsigned char> > mapPubKeys;
CCriticalSection cs_mapKeys;
CKey keyUser;

string strSetDataDir;
int nDropMessagesTest = 0;

// Settings
int fGenerateCoins = 1;
int64 nTransactionFee = 0;
CAddress addrIncoming;

// AI MODEL

GPT2 gpt2model;
Tokenizer tokenizer;
DataLoader train_loader;
int B; // batch size
int T; // sequence lengt


//////////////////////////////////////////////////////////////////////////////
//
// mapKeys
//

bool AddKey(const CKey& key)
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        mapKeys[key.GetPubKey()] = key.GetPrivKey();
        mapPubKeys[Hash160(key.GetPubKey())] = key.GetPubKey();
    }
    return CWalletDB().WriteKey(key.GetPubKey(), key.GetPrivKey());
}

vector<unsigned char> GenerateNewKey()
{
    CKey key;
    key.MakeNewKey();
    if (!AddKey(key))
        throw runtime_error("GenerateNewKey() : AddKey failed\n");
    return key.GetPubKey();
}




//////////////////////////////////////////////////////////////////////////////
//
// mapWallet
//

bool AddToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
            wtx.nTimeReceived = GetAdjustedTime();

        //// debug print
        printf("AddToWallet %s  %s\n", wtxIn.GetHash().ToString().substr(0,6).c_str(), fInsertedNew ? "new" : "update");

        if (!fInsertedNew)
        {
            // Merge
            bool fUpdated = false;
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            if (wtxIn.fSpent && wtxIn.fSpent != wtx.fSpent)
            {
                wtx.fSpent = wtxIn.fSpent;
                fUpdated = true;
            }
            if (!fUpdated)
                return true;
        }

        // Write to disk
        if (!wtx.WriteToDisk())
            return false;

        // Notify UI
        vWalletUpdated.push_back(make_pair(hash, fInsertedNew));
    }

    // Refresh UI
    // MainFrameRepaint();
    return true;
}

bool AddToWalletIfMine(const CTransaction& tx, const CBlock* pblock)
{
    if (tx.IsMine() || mapWallet.count(tx.GetHash()))
    {
        CWalletTx wtx(tx);
        // Get merkle branch if transaction was found in a block
        if (pblock)
            wtx.SetMerkleBranch(pblock);
        return AddToWallet(wtx);
    }
    return true;
}

bool EraseFromWallet(uint256 hash)
{
    CRITICAL_BLOCK(cs_mapWallet)
    {
        if (mapWallet.erase(hash))
            CWalletDB().EraseTx(hash);
    }
    return true;
}









//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

void AddOrphanTx(const CDataStream& vMsg)
{
    CTransaction tx;
    CDataStream(vMsg) >> tx;
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return;
    CDataStream* pvMsg = mapOrphanTransactions[hash] = new CDataStream(vMsg);
    foreach(const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev.insert(make_pair(txin.prevout.hash, pvMsg));
}

void EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CDataStream* pvMsg = mapOrphanTransactions[hash];
    CTransaction tx;
    CDataStream(*pvMsg) >> tx;
    foreach(const CTxIn& txin, tx.vin)
    {
        for (multimap<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev.lower_bound(txin.prevout.hash);
             mi != mapOrphanTransactionsByPrev.upper_bound(txin.prevout.hash);)
        {
            if ((*mi).second == pvMsg)
                mapOrphanTransactionsByPrev.erase(mi++);
            else
                mi++;
        }
    }
    delete pvMsg;
    mapOrphanTransactions.erase(hash);
}








//////////////////////////////////////////////////////////////////////////////
//
// CTransaction
//

bool CTxIn::IsMine() const
{
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (prevout.n < prev.vout.size())
                if (prev.vout[prevout.n].IsMine())
                    return true;
        }
    }
    return false;
}

int64 CTxIn::GetDebit() const
{
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (prevout.n < prev.vout.size())
                if (prev.vout[prevout.n].IsMine())
                    return prev.vout[prevout.n].nValue;
        }
    }
    return 0;
}

int64 CWalletTx::GetTxTime() const
{
    if (!fTimeReceivedIsTxTime && hashBlock != 0)
    {
        // If we did not receive the transaction directly, we rely on the block's
        // time to figure out when it happened.  We use the median over a range
        // of blocks to try to filter out inaccurate block times.
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (pindex)
                return pindex->GetMedianTime();
        }
    }
    return nTimeReceived;
}






int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    if (fClient)
    {
        if (hashBlock == 0)
            return 0;
    }
    else
    {
        CBlock blockTmp;
        if (pblock == NULL)
        {
            // Load the block this tx is in
            CTxIndex txindex;
            if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
                return 0;
            if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, true))
                return 0;
            pblock = &blockTmp;
        }

        // Update the tx's hashBlock
        hashBlock = pblock->GetHash();

        // Locate the transaction
        for (nIndex = 0; nIndex < pblock->vtx.size(); nIndex++)
            if (pblock->vtx[nIndex] == *(CTransaction*)this)
                break;
        if (nIndex == pblock->vtx.size())
        {
            vMerkleBranch.clear();
            nIndex = -1;
            printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
            return 0;
        }

        // Fill in merkle branch
        vMerkleBranch = pblock->GetMerkleBranch(nIndex);
    }

    // Is the tx in a block that's in the main chain
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return pindexBest->nHeight - pindex->nHeight + 1;
}



void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        foreach(const CTxIn& txin, vin)
            vWorkQueue.push_back(txin.prevout.hash);

        // This critsect is OK because txdb is already open
        CRITICAL_BLOCK(cs_mapWallet)
        {
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                if (mapWallet.count(hash))
                {
                    tx = mapWallet[hash];
                    foreach(const CMerkleTx& txWalletPrev, mapWallet[hash].vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (!fClient && txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    printf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                    foreach(const CTxIn& txin, tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}











bool CTransaction::AcceptTransaction(CTxDB& txdb, bool fCheckInputs, bool* pfMissingInputs)
{
    if (pfMissingInputs)
        *pfMissingInputs = false;

    // Coinbase is only valid in a block, not as a loose transaction
    if (IsCoinBase())
        return error("AcceptTransaction() : coinbase as individual tx");

    if (!CheckTransaction())
        return error("AcceptTransaction() : CheckTransaction failed");

    // Do we already have it?
    uint256 hash = GetHash();
    CRITICAL_BLOCK(cs_mapTransactions)
    if (mapTransactions.count(hash))
        return false;
    if (fCheckInputs)
        if (txdb.ContainsTx(hash))
            return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = NULL;
    for (int i = 0; i < vin.size(); i++)
    {
        COutPoint outpoint = vin[i].prevout;
        if (mapNextTx.count(outpoint))
        {
            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = mapNextTx[outpoint].ptx;
            if (!IsNewerThan(*ptxOld))
                return false;
            for (int i = 0; i < vin.size(); i++)
            {
                COutPoint outpoint = vin[i].prevout;
                if (!mapNextTx.count(outpoint) || mapNextTx[outpoint].ptx != ptxOld)
                    return false;
            }
            break;
        }
    }

    // Check against previous transactions
    map<uint256, CTxIndex> mapUnused;
    int64 nFees = 0;
    if (fCheckInputs && !ConnectInputs(txdb, mapUnused, CDiskTxPos(1,1,1), 0, nFees, false, false))
    {
        if (pfMissingInputs)
            *pfMissingInputs = true;
        return error("AcceptTransaction() : ConnectInputs failed %s", hash.ToString().substr(0,6).c_str());
    }

    // Store transaction in memory
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        if (ptxOld)
        {
            printf("mapTransaction.erase(%s) replacing with new version\n", ptxOld->GetHash().ToString().c_str());
            mapTransactions.erase(ptxOld->GetHash());
        }
        AddToMemoryPool();
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallet(ptxOld->GetHash());

    printf("AcceptTransaction(): accepted %s\n", hash.ToString().substr(0,6).c_str());
    return true;
}


bool CTransaction::AddToMemoryPool()
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptTransaction to properly check the transaction first.
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        uint256 hash = GetHash();
        mapTransactions[hash] = *this;
        for (int i = 0; i < vin.size(); i++)
            mapNextTx[vin[i].prevout] = CInPoint(&mapTransactions[hash], i);
        nTransactionsUpdated++;
    }
    return true;
}


bool CTransaction::RemoveFromMemoryPool()
{
    // Remove transaction from memory pool
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        foreach(const CTxIn& txin, vin)
        mapNextTx.erase(txin.prevout);
        mapTransactions.erase(GetHash());
        nTransactionsUpdated++;
    }
    return true;
}






int CMerkleTx::GetDepthInMainChain() const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;

    // Find the block it claims to be in
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    // Make sure the merkle branch connects to this block
    if (!fMerkleVerified)
    {
        if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
    }

    return pindexBest->nHeight - pindex->nHeight + 1;
}


int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return max(0, (COINBASE_MATURITY+20) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptTransaction(CTxDB& txdb, bool fCheckInputs)
{
    if (fClient)
    {
        if (!IsInMainChain() && !ClientConnectInputs())
            return false;
        return CTransaction::AcceptTransaction(txdb, false);
    }
    else
    {
        return CTransaction::AcceptTransaction(txdb, fCheckInputs);
    }
}



bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs)
{
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        for (const CMerkleTx& tx : vtxPrev)  // Use const reference since container elements are const
        {
            if (!tx.IsCoinBase())
            {
                uint256 hash = tx.GetHash();
                if (!mapTransactions.count(hash) && !txdb.ContainsTx(hash))
                    const_cast<CMerkleTx&>(tx).AcceptTransaction(txdb, fCheckInputs);  // Cast to non-const if modification is needed
            }
        }
        if (!IsCoinBase())
            return AcceptTransaction(txdb, fCheckInputs);
    }
    return true;
}


void ReacceptWalletTransactions()
{
    // Reaccept any transactions of ours that aren't already in a block
    CTxDB txdb("r");

    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Use range-based for loop to iterate over mapWallet
        for (auto& item : mapWallet)  // Replaces foreach
        {
            CWalletTx& wtx = item.second;
            if (!wtx.IsCoinBase() && !txdb.ContainsTx(wtx.GetHash()))
                wtx.AcceptWalletTransaction(txdb, false);
        }
    }
}



void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    foreach(const CMerkleTx& tx, vtxPrev)
    {
        if (!tx.IsCoinBase())
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayMessage(CInv(MSG_TX, hash), (CTransaction)tx);
        }
    }
    if (!IsCoinBase())
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            printf("Relaying wtx %s\n", hash.ToString().substr(0,6).c_str());
            RelayMessage(CInv(MSG_TX, hash), (CTransaction)*this);
        }
    }
}

void RelayWalletTransactions()
{
    static int64 nLastTime;
    if (GetTime() - nLastTime < 10 * 60)
        return;
    nLastTime = GetTime();

    // Rebroadcast any of our transactions that aren't in a block yet
    printf("RelayWalletTransactions()\n");
    CTxDB txdb("r");

    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;

        // First loop: Populate the sorted map
        for (auto& item : mapWallet)  // Use range-based for loop
        {
            CWalletTx& wtx = item.second;
            mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }

        // Second loop: Relay the wallet transactions
        for (auto& item : mapSorted)  // Use range-based for loop
        {
            CWalletTx& wtx = *item.second;
            wtx.RelayWalletTransaction(txdb);
        }
    }
}











//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool CBlock::ReadFromDisk(const CBlockIndex* pblockindex, bool fReadTransactions)
{
    return ReadFromDisk(pblockindex->nFile, pblockindex->nBlockPos, fReadTransactions);
}

uint256 GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}

int64 CBlock::GetBlockValue(int64 nFees) const
{
    int64 nSubsidy = 50 * COIN;

    // Subsidy is cut in half every 4 years
    nSubsidy >>= (nBestHeight / 210000);

    return nSubsidy + nFees;
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast)
{
    const unsigned int nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const unsigned int nTargetSpacing = 10 * 60;
    const unsigned int nInterval = nTargetTimespan / nTargetSpacing;

    // Genesis block
    if (pindexLast == NULL)
        return bnProofOfWorkLimit.GetCompact();

    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
        return pindexLast->nBits;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    unsigned int nActualTimespan = pindexLast->nTime - pindexFirst->nTime;
    printf("  nActualTimespan = %d  before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    /// debug print
    printf("\n\n\nGetNextWorkRequired RETARGET *****\n");
    printf("nTargetTimespan = %d    nActualTimespan = %d\n", nTargetTimespan, nActualTimespan);
    printf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString().c_str());
    printf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());

    return bnNew.GetCompact();
}









bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase())
    {
        foreach(const CTxIn& txin, vin)
        {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            txdb.UpdateTxIndex(prevout.hash, txindex);
        }
    }

    // Remove transaction from index
    if (!txdb.EraseTxIndex(*this))
        return error("DisconnectInputs() : EraseTxPos failed");

    return true;
}


bool CTransaction::ConnectInputs(CTxDB& txdb, map<uint256, CTxIndex>& mapTestPool, CDiskTxPos posThisTx, int nHeight, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee)
{
    // Take over previous transactions' spent pointers
    if (!IsCoinBase())
    {
        int64 nValueIn = 0;
        for (int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;

            // Read txindex
            CTxIndex txindex;
            bool fFound = true;
            if (fMiner && mapTestPool.count(prevout.hash))
            {
                // Get txindex from current proposed changes
                txindex = mapTestPool[prevout.hash];
            }
            else
            {
                // Read txindex from txdb
                fFound = txdb.ReadTxIndex(prevout.hash, txindex);
            }
            if (!fFound && (fBlock || fMiner))
                return fMiner ? false : error("ConnectInputs() : %s prev tx %s index entry not found", GetHash().ToString().substr(0,6).c_str(),  prevout.hash.ToString().substr(0,6).c_str());

            // Read txPrev
            CTransaction txPrev;
            if (!fFound || txindex.pos == CDiskTxPos(1,1,1))
            {
                // Get prev tx from single transactions in memory
                CRITICAL_BLOCK(cs_mapTransactions)
                {
                    if (!mapTransactions.count(prevout.hash))
                        return error("ConnectInputs() : %s mapTransactions prev not found %s", GetHash().ToString().substr(0,6).c_str(),  prevout.hash.ToString().substr(0,6).c_str());
                    txPrev = mapTransactions[prevout.hash];
                }
                if (!fFound)
                    txindex.vSpent.resize(txPrev.vout.size());
            }
            else
            {
                // Get prev tx from disk
                if (!txPrev.ReadFromDisk(txindex.pos))
                    return error("ConnectInputs() : %s ReadFromDisk prev tx %s failed", GetHash().ToString().substr(0,6).c_str(),  prevout.hash.ToString().substr(0,6).c_str());
            }

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
                return error("ConnectInputs() : %s prevout.n out of range %d %d %d", GetHash().ToString().substr(0,6).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size());

            // If prev is coinbase, check that it's matured
            if (txPrev.IsCoinBase())
                for (CBlockIndex* pindex = pindexBest; pindex && nBestHeight - pindex->nHeight < COINBASE_MATURITY-1; pindex = pindex->pprev)
                    if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
                        return error("ConnectInputs() : tried to spend coinbase at depth %d", nBestHeight - pindex->nHeight);

            // Verify signature
            if (!VerifySignature(txPrev, *this, i))
                return error("ConnectInputs() : %s VerifySignature failed", GetHash().ToString().substr(0,6).c_str());

            // Check for conflicts
            if (!txindex.vSpent[prevout.n].IsNull())
                return fMiner ? false : error("ConnectInputs() : %s prev tx already used at %s", GetHash().ToString().substr(0,6).c_str(), txindex.vSpent[prevout.n].ToString().c_str());

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock)
                txdb.UpdateTxIndex(prevout.hash, txindex);
            else if (fMiner)
                mapTestPool[prevout.hash] = txindex;

            nValueIn += txPrev.vout[prevout.n].nValue;
        }

        // Tally transaction fees
        int64 nTxFee = nValueIn - GetValueOut();
        if (nTxFee < 0)
            return error("ConnectInputs() : %s nTxFee < 0", GetHash().ToString().substr(0,6).c_str());
        if (nTxFee < nMinFee)
            return false;
        nFees += nTxFee;
    }

    if (fBlock)
    {
        // Add transaction to disk index
        if (!txdb.AddTxIndex(*this, posThisTx, nHeight))
            return error("ConnectInputs() : AddTxPos failed");
        // Add transaction toknes to disk index
        if((!txdb.))
    }
    else if (fMiner)
    {
        // Add transaction to test pool
        mapTestPool[GetHash()] = CTxIndex(CDiskTxPos(1,1,1), vout.size());
    }

    return true;
}


bool CTransaction::ClientConnectInputs()
{
    if (IsCoinBase())
        return false;

    // Take over previous transactions' spent pointers
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        int64 nValueIn = 0;
        for (int i = 0; i < vin.size(); i++)
        {
            // Get prev tx from single transactions in memory
            COutPoint prevout = vin[i].prevout;
            if (!mapTransactions.count(prevout.hash))
                return false;
            CTransaction& txPrev = mapTransactions[prevout.hash];

            if (prevout.n >= txPrev.vout.size())
                return false;

            // Verify signature
            if (!VerifySignature(txPrev, *this, i))
                return error("ConnectInputs() : VerifySignature failed");

            ///// this is redundant with the mapNextTx stuff, not sure which I want to get rid of
            ///// this has to go away now that posNext is gone
            // // Check for conflicts
            // if (!txPrev.vout[prevout.n].posNext.IsNull())
            //     return error("ConnectInputs() : prev tx already used");
            //
            // // Flag outpoints as used
            // txPrev.vout[prevout.n].posNext = posThisTx;

            nValueIn += txPrev.vout[prevout.n].nValue;
        }
        if (GetValueOut() > nValueIn)
            return false;
    }

    return true;
}




bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        txdb.WriteBlockIndex(blockindexPrev);
    }

    return true;
}
bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    //// issue here: it doesn't know the version
    unsigned int nTxPos = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK) - 1 + GetSizeOfCompactSize(vtx.size());

    map<uint256, CTxIndex> mapUnused;
    int64 nFees = 0;

    // Use const reference since vtx elements are const
    for (const CTransaction& tx : vtx)
    {
        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        nTxPos += ::GetSerializeSize(tx, SER_DISK);

        // Use const_cast if you need to modify the transaction
        if (!const_cast<CTransaction&>(tx).ConnectInputs(txdb, mapUnused, posThisTx, pindex->nHeight, nFees, true, false))
            return false;
    }

    if (vtx[0].GetValueOut() > GetBlockValue(nFees))
        return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        txdb.WriteBlockIndex(blockindexPrev);
    }

    // Watch for transactions paying to me
    for (const CTransaction& tx : vtx)
        AddToWalletIfMine(tx, this);

    return true;
}


// ROCKS DB COMMITS DUE

bool Reorganize(CTxDB& txdb, CBlockIndex* pindexNew)
{
    printf("*** REORGANIZE ***\n");

    // Find the fork
    CBlockIndex* pfork = pindexBest;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger)
    {
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
    }

    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);

    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());

    // Disconnect shorter branch
    vector<CTransaction> vResurrect;
    foreach(CBlockIndex* pindex, vDisconnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos, true))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock failed");

        // Queue memory transactions to resurrect
        foreach(const CTransaction& tx, block.vtx)
            if (!tx.IsCoinBase())
                vResurrect.push_back(tx);
    }

    // Connect longer branch
    vector<CTransaction> vDelete;
    for (int i = 0; i < vConnect.size(); i++)
    {
        CBlockIndex* pindex = vConnect[i];
        CBlock block;
        if (!block.ReadFromDisk(pindex->nFile, pindex->nBlockPos, true))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex))
        {
            // Invalid block, delete the rest of this branch
            txdb.TxnAbort();
            for (int j = i; j < vConnect.size(); j++)
            {
                CBlockIndex* pindex = vConnect[j];
                pindex->EraseBlockFromDisk();
                txdb.EraseBlockIndex(pindex->GetBlockHash());
                mapBlockIndex.erase(pindex->GetBlockHash());
                delete pindex;
            }
            return error("Reorganize() : ConnectBlock failed");
        }

        // Queue memory transactions to delete
        foreach(const CTransaction& tx, block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");

    // Commit now because resurrecting could take some time
    txdb.TxnCommit(); // PENDING THE TRANSACTION OF THE DB

    // Disconnect shorter branch
    foreach(CBlockIndex* pindex, vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = NULL;

    // Connect longer branch
    foreach(CBlockIndex* pindex, vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    for (const CTransaction& tx : vResurrect)
    {
        const_cast<CTransaction&>(tx).AcceptTransaction(txdb, false);  // Cast away const if modification is needed
    }

    // Delete redundant memory transactions that are in the connected branch
    for (const CTransaction& tx : vDelete)
    {
        const_cast<CTransaction&>(tx).RemoveFromMemoryPool();  // Cast away const if modification is needed
    }


    return true;
}


bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().substr(0,14).c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    CTxDB txdb;
    txdb.TxnBegin();
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

    // New best
    if (pindexNew->nHeight > nBestHeight)
    {
        if (pindexGenesisBlock == NULL && hash == hashGenesisBlock)
        {
            pindexGenesisBlock = pindexNew;
            txdb.WriteHashBestChain(hash);
        }
        else if (hashPrevBlock == hashBestChain)
        {
            // Adding to current best branch
            if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
            {
                txdb.TxnAbort();
                pindexNew->EraseBlockFromDisk();
                mapBlockIndex.erase(pindexNew->GetBlockHash());
                delete pindexNew;
                return error("AddToBlockIndex() : ConnectBlock failed");
            }
            txdb.TxnCommit(); // PENDING THE TRANSACTION OF THE DB
            pindexNew->pprev->pnext = pindexNew;

            // Delete redundant memory transactions
            for (const CTransaction& tx : vtx)
            {
                const_cast<CTransaction&>(tx).RemoveFromMemoryPool();  // Cast away const to allow modification
            }

        }
        else
        {
            // New best branch
            if (!Reorganize(txdb, pindexNew))
            {
                txdb.TxnAbort();
                return error("AddToBlockIndex() : Reorganize failed");
            }
        }

        // New best link
        hashBestChain = hash;
        pindexBest = pindexNew;
        nBestHeight = pindexBest->nHeight;
        nTransactionsUpdated++;
        printf("AddToBlockIndex: new best=%s  height=%d\n", hashBestChain.ToString().substr(0,14).c_str(), nBestHeight);
    }

    txdb.TxnCommit(); // PENDING THE TRANSACTION OF THE DB
    txdb.Close();

    // Relay wallet transactions that haven't gotten in yet
    if (pindexNew == pindexBest)
        RelayWalletTransactions();

    // MainFrameRepaint();
    return true;
}




bool CBlock::CheckBlock() const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_SIZE || ::GetSerializeSize(*this, SER_DISK) > MAX_SIZE)
        return error("CheckBlock() : size limits failed");

    // Check timestamp
    if (nTime > GetAdjustedTime() + 2 * 60 * 60)
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return error("CheckBlock() : first tx is not coinbase");
    for (int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return error("CheckBlock() : more than one coinbase");

    // Check transactions
    for (const CTransaction& tx : vtx) {
        if (!tx.CheckTransaction()) {
            return error("CheckBlock() : CheckTransaction failed");
        }
    }

    // Check proof of work matches claimed amount
    if (CBigNum().SetCompact(nBits) > bnProofOfWorkLimit)
        return error("CheckBlock() : nBits below minimum work");
    if (GetHash() > CBigNum().SetCompact(nBits).getuint256())
        return error("CheckBlock() : hash doesn't match nBits");

    // Check merkleroot
    if (hashMerkleRoot != BuildMerkleTree())
        return error("CheckBlock() : hashMerkleRoot mismatch");

    return true;
}

bool CBlock::AcceptBlock()
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return error("AcceptBlock() : prev block not found");
    CBlockIndex* pindexPrev = (*mi).second;

    // Check timestamp against prev
    if (nTime <= pindexPrev->GetMedianTimePast())
        return error("AcceptBlock() : block's timestamp is too early");

    // Check proof of work
    if (nBits != GetNextWorkRequired(pindexPrev))
        return error("AcceptBlock() : incorrect proof of work");

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile;
    unsigned int nBlockPos;
    if (!WriteToDisk(!fClient, nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos))
        return error("AcceptBlock() : AddToBlockIndex failed");

    if (hashBestChain == hash)
        RelayInventory(CInv(MSG_BLOCK, hash));

    // // Add atoms to user reviews for coins created
    // vector<unsigned char> vchPubKey;
    // if (ExtractPubKey(vtx[0].vout[0].scriptPubKey, false, vchPubKey))
    // {
    //     unsigned short nAtom = GetRand(USHRT_MAX - 100) + 100;
    //     vector<unsigned short> vAtoms(1, nAtom);
    //     AddAtomsAndPropagate(Hash(vchPubKey.begin(), vchPubKey.end()), vAtoms, true);
    // }

    return true;
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock)
{
    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().substr(0,14).c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().substr(0,14).c_str());

    // Preliminary checks
    if (!pblock->CheckBlock())
    {
        delete pblock;
        return error("ProcessBlock() : CheckBlock FAILED");
    }

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().substr(0,14).c_str());
        mapOrphanBlocks.insert(make_pair(hash, pblock));
        mapOrphanBlocksByPrev.insert(make_pair(pblock->hashPrevBlock, pblock));

        // Ask this guy to fill in what we're missing
        if (pfrom)
            pfrom->PushMessage("getblocks", CBlockLocator(pindexBest), GetOrphanRoot(pblock));
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock())
    {
        delete pblock;
        return error("ProcessBlock() : AcceptBlock FAILED");
    }
    delete pblock;

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = (*mi).second;
            if (pblockOrphan->AcceptBlock())
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    printf("ProcessBlock: ACCEPTED\n");
    return true;
}

// template<typename Stream>
// bool ScanMessageStart(Stream& s)
// {
//     // Scan ahead to the next pchMessageStart, which should normally be immediately
//     // at the file pointer.  Leaves file pointer at end of pchMessageStart.
//     s.clear(0);
//     short prevmask = s.exceptions(0);
//     const char* p = BEGIN(pchMessageStart);
//     try
//     {
//         loop
//         {
//             char c;
//             s.read(&c, 1);
//             if (s.fail())
//             {
//                 s.clear(0);
//                 s.exceptions(prevmask);
//                 return false;
//             }
//             if (*p != c)
//                 p = BEGIN(pchMessageStart);
//             if (*p == c)
//             {
//                 if (++p == END(pchMessageStart))
//                 {
//                     s.clear(0);
//                     s.exceptions(prevmask);
//                     return true;
//                 }
//             }
//         }
//     }
//     catch (...)
//     {
//         s.clear(0);
//         s.exceptions(prevmask);
//         return false;
//     }
// }

string GetAppDir()
{
    string strDir;
    if (!strSetDataDir.empty())
    {
        strDir = strSetDataDir;
    }
    else if (getenv("APPDATA"))
    {
        strDir = strprintf("%s\\", getenv("APPDATA"));
    }
    else if (getenv("USERPROFILE"))
    {
        string strAppData = strprintf("%s\\Application Data", getenv("USERPROFILE"));
        static bool fMkdirDone;
        if (!fMkdirDone)
        {
            fMkdirDone = true;
            _mkdir(strAppData.c_str());
        }
        strDir = strprintf("%s\\", strAppData.c_str());
    }
    else
    {
        return ".";
    }
    static bool fMkdirDone;
    if (!fMkdirDone)
    {
        fMkdirDone = true;
        _mkdir(strDir.c_str());
    }
    return strDir;
}


// Declare the Shutdown function
void Shutdown()
{
    // You can implement cleanup logic here if needed
    qDebug("Shutting down the application...");

    // Use Qt's method to close the application
    QCoreApplication::quit();
}

bool CheckDiskSpace(int64 nAdditionalBytes)
{
    uint64 nFreeBytesAvailable = 0;     // bytes available to caller
    uint64 nTotalNumberOfBytes = 0;     // bytes on disk
    uint64 nTotalNumberOfFreeBytes = 0; // free bytes on disk

    // Use GetDiskFreeSpaceExA for narrow-character string support
    if (!GetDiskFreeSpaceExA(GetAppDir().c_str(),
                             (PULARGE_INTEGER)&nFreeBytesAvailable,
                             (PULARGE_INTEGER)&nTotalNumberOfBytes,
                             (PULARGE_INTEGER)&nTotalNumberOfFreeBytes))
    {
        printf("ERROR: GetDiskFreeSpaceExA() failed\n");
        return true;
    }

    // Check for 15MB because the database could create another 10MB log file at any time
    if ((int64)nFreeBytesAvailable < 15000000 + nAdditionalBytes)
    {
        fShutdown = true;
        QMessageBox::warning(nullptr, "", "Warning: Your disk space is low", QMessageBox::Ok);
        // _beginthread(Shutdown, 0, NULL);
        Shutdown();

        return false;
    }
    return true;
}

FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if (nFile == -1)
        return NULL;
    FILE* file = fopen(strprintf("%s\\blk%04d.dat", GetAppDir().c_str(), nFile).c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return NULL;
        }
    }
    return file;
}

static unsigned int nCurrentBlockFile = 1;

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    loop
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
        // FAT32 filesize max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < 0x7F000000 - MAX_SIZE)
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
        nCurrentBlockFile++;
    }
}

bool LoadBlockIndex(bool fAllowNew)
{
    //
    // Load block index
    //
    CTxDB txdb("cr");
    if (!txdb.LoadBlockIndex())
        return false;
    txdb.Close();

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;


        // Genesis Block:
        // GetHash()      = 0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f
        // hashMerkleRoot = 0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b
        // txNew.vin[0].scriptSig     = 486604799 4 0x736B6E616220726F662074756F6C69616220646E6F63657320666F206B6E697262206E6F20726F6C6C65636E61684320393030322F6E614A2F33302073656D695420656854
        // txNew.vout[0].nValue       = 5000000000
        // txNew.vout[0].scriptPubKey = 0x5F1DF16B2B704C8A578D0BBAF74D385CDE12C11EE50455F3C438EF4C3FBCF649B6DE611FEAE06279A60939E028A8D65C10B73071A6F16719274855FEB0FD8A6704 OP_CHECKSIG
        // block.nVersion = 1
        // block.nTime    = 1231006505
        // block.nBits    = 0x1d00ffff
        // block.nNonce   = 2083236893
        // CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
        //   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
        //     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
        //     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
        //   vMerkleTree: 4a5e1e

        // Genesis block
        // const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        // CTransaction txNew;
        // txNew.vin.resize(1);
        // txNew.vout.resize(1);
        // txNew.vin[0].scriptSig     = CScript() << 486604799 << CBigNum(4) << vector<unsigned char>((unsigned char*)pszTimestamp, (unsigned char*)pszTimestamp + strlen(pszTimestamp));
        // txNew.vout[0].nValue       = 50 * COIN;
        // txNew.vout[0].scriptPubKey = CScript() << CBigNum("0x5F1DF16B2B704C8A578D0BBAF74D385CDE12C11EE50455F3C438EF4C3FBCF649B6DE611FEAE06279A60939E028A8D65C10B73071A6F16719274855FEB0FD8A6704") << OP_CHECKSIG;
        // CBlock block;
        // block.vtx.push_back(txNew);
        // block.hashPrevBlock = 0;
        // block.hashMerkleRoot = block.BuildMerkleTree();
        // block.nVersion = 1;
        // block.nTime    = 1231006505;
        // block.nBits    = 0x1d00ffff;
        // block.nNonce   = 2083236893;


        const char* pszTimestamp = "The most beautiful test";
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig     = CScript() << 536912855 << CBigNum(4) << vector<unsigned char>((unsigned char*)pszTimestamp, (unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue       = 50 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << CBigNum("0xE886ECFF5F10F89D78D5B814B45F1A0A97B333AD5FA5020DA14C9B161E6E4754A977848B9CC748E079D7D7534BB29CCADA48E5752CE6C26F0B636C87E8084A1804") << OP_CHECKSIG;
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock = 0;
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion = 1;
        block.nTime    = 1231006505;
        block.nBits    = 0x2000a3d7;
        block.nNonce   = 908;

        //// debug print, delete this later
        printf("%s\n", block.GetHash().ToString().c_str());
        printf("%s\n", block.hashMerkleRoot.ToString().c_str());
        printf("%s\n", hashGenesisBlock.ToString().c_str());
        txNew.vout[0].scriptPubKey.print();
        block.print();
        // assert(block.hashMerkleRoot == uint256("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));
        // assert(block.hashMerkleRoot == uint256("0x6f748db34e07815ae60201e89f7c17ad6b1f9f9079e3a464ad17e726a1c406ff"));
        // assert(block.hashMerkleRoot == uint256("684245"));
        assert(block.hashMerkleRoot == uint256("684245dadc1e68600856e16d801165b1710570f53be8bf37dd6302f92c80ee12"));
        assert(block.GetHash() == hashGenesisBlock);

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!block.WriteToDisk(!fClient, nFile, nBlockPos))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!block.AddToBlockIndex(nFile, nBlockPos))
            return error("LoadBlockIndex() : genesis block not accepted");
    }

    return true;
}


// logic exclusively to add the initial .bin file or a general logic that falls into place with or without the file?
// start with the first one and then add the cases to make the second req true.
bool LoadGenesisGPT2(GPT2 *model) // this is to be called in the CTxDB::LoadModel()
{
    // if genesis model:
    // read in model from a checkpoint file
    FILE *model_file = fopenCheck("C:/Users/aaron/Documents/coin/resources/gpt2_124M.bin", "rb");
    if (model_file == NULL) { printf("Error opening model file\n"); exit(1); }
    int model_header[256];
    freadCheck(model_header, sizeof(int), 256, model_file);
    if (model_header[0] != 20240326) { printf("Bad magic model file\n"); exit(1); }
    if (model_header[1] != 3) {
        printf("Bad version in model file\n");
        printf("---> HINT: try to re-run `python train_gpt2.py`\n");
        exit(1);
    }

    // read in hyperparameters
    size_t maxT, V, Vp, L, NH, C; // size_t to prevent int overflow
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

    // allocate space for all the parameters and read them in
    fill_in_parameter_sizes(model->param_sizes,  model->config);

    // count the number of parameters
    size_t num_parameters = 0;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        num_parameters += model->param_sizes[i];
    }
    printf("num_parameters: %zu\n", num_parameters);
    model->num_parameters = num_parameters;

    // read in all the parameters from file
    model->params_memory = malloc_and_point_parameters(&model->params, model->param_sizes);
    freadCheck(model->params_memory, sizeof(float), num_parameters, model_file);
    fcloseCheck(model_file);

    // put the params_memory in the db
    // Save model header and parameters to the database
    CTxDB txdb;
    txdb.TxnBegin();

    txdb.WriteModelHeader(model);
    txdb.WriteModelParameters(model);

    txdb.TxnCommit(); // PENDING THE TRANSACTION OF THE DB
    txdb.Close();

    return true;
}

bool LoadGPT2() {
    //
    // Load GPT2
    //
    B = 4; // batch size
    T = 64; // sequence length
    CTxDB txdb("cr");
    if (!txdb.LoadGPT2(&gpt2model))
        return false;
    txdb.Close();

    //
    // Init with genesis AI
    //
    // First, check if model already exists in the database
    LoadGenesisGPT2(&gpt2model);
    return true;
}


bool LoadGP2Tokenizer() {
    //
    // Load GPT2 tokenizer
    //
    tokenizer_init(&tokenizer, "C:/Users/aaron/Documents/blockchain/llm.c/gpt2_tokenizer.bin");

    return tokenizer.init_ok;
}

bool LoadGP2DataLoader() {
    //
    // Load GPT2 data loader
    //
    const char* tiny_stories_train = "dev/data/tinystories/TinyStories_train.bin";
    // const char* tiny_shakespeare_train = "dev/data/test/test_train.bin";
    // const char* train_tokens = access(tiny_shakespeare_train, F_OK) != -1 ? tiny_shakespeare_train : tiny_stories_train;

    dataloader_init(&train_loader, tiny_stories_train, B, T, 0, 1, 1);

    return train_loader.init_ok;
}

// Function to convert a Unix timestamp to a readable date-time string
std::string DateTimeStr(int64_t nTime)
{
    time_t timeValue = (time_t)nTime;
    struct tm* ptm = gmtime(&timeValue);  // Use gmtime for UTC or localtime for local time
    char buffer[32];
    // Format: YYYY-MM-DD HH:MM:SS
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ptm);
    return std::string(buffer);
}

void PrintBlockTree()
{
    // precompute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = (*mi).second;
        mapNext[pindex->pprev].push_back(pindex);
        // test
        //while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++)
                printf("| ");
            printf("|\\\n");
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
        }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex, true);
        printf("%d (%u,%u) %s  %s  tx %d",
               pindex->nHeight,
               pindex->nFile,
               pindex->nBlockPos,
               block.GetHash().ToString().substr(0,14).c_str(),
               DateTimeStr(block.nTime).c_str(),
               block.vtx.size());

        CRITICAL_BLOCK(cs_mapWallet)
        {
            if (mapWallet.count(block.vtx[0].GetHash()))
            {
                CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
                printf("    mine:  %d  %d  %d", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
            }
        }
        printf("\n");


        // put the main timechain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}










//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool AlreadyHave(CTxDB& txdb, const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:        return mapTransactions.count(inv.hash) || txdb.ContainsTx(inv.hash);
    case MSG_BLOCK:     return mapBlockIndex.count(inv.hash) || mapOrphanBlocks.count(inv.hash);
    case MSG_REVIEW:    return true;
    // case MSG_PRODUCT:   return mapProducts.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}







// bool ProcessMessages(CNode* pfrom)
// {
//     CDataStream& vRecv = pfrom->vRecv;
//     if (vRecv.empty())
//         return true;
//     printf("ProcessMessages(%d bytes)\n", vRecv.size());

//     //
//     // Message format
//     //  (4) message start
//     //  (12) command
//     //  (4) size
//     //  (x) data
//     //

//     loop
//     {
//         // Scan for message start
//         CDataStream::iterator pstart = search(vRecv.begin(), vRecv.end(), BEGIN(pchMessageStart), END(pchMessageStart));
//         if (vRecv.end() - pstart < sizeof(CMessageHeader))
//         {
//             if (vRecv.size() > sizeof(CMessageHeader))
//             {
//                 printf("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND\n\n");
//                 vRecv.erase(vRecv.begin(), vRecv.end() - sizeof(CMessageHeader));
//             }
//             break;
//         }
//         if (pstart - vRecv.begin() > 0)
//             printf("\n\nPROCESSMESSAGE SKIPPED %d BYTES\n\n", pstart - vRecv.begin());
//         vRecv.erase(vRecv.begin(), pstart);

//         // Read header
//         CMessageHeader hdr;
//         vRecv >> hdr;
//         if (!hdr.IsValid())
//         {
//             printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
//             continue;
//         }
//         string strCommand = hdr.GetCommand();

//         // Message size
//         unsigned int nMessageSize = hdr.nMessageSize;
//         if (nMessageSize > vRecv.size())
//         {
//             // Rewind and wait for rest of message
//             ///// need a mechanism to give up waiting for overlong message size error
//             printf("MESSAGE-BREAK\n");
//             vRecv.insert(vRecv.begin(), BEGIN(hdr), END(hdr));
//             Sleep(100);
//             break;
//         }

//         // Copy message to its own buffer
//         CDataStream vMsg(vRecv.begin(), vRecv.begin() + nMessageSize, vRecv.nType, vRecv.nVersion);
//         vRecv.ignore(nMessageSize);

//         // Process message
//         bool fRet = false;
//         try
//         {
//             CheckForShutdown(2);
//             CRITICAL_BLOCK(cs_main)
//             fRet = ProcessMessage(pfrom, strCommand, vMsg);
//             CheckForShutdown(2);
//         }
//         CATCH_PRINT_EXCEPTION("ProcessMessage()")
//         if (!fRet)
//             printf("ProcessMessage(%s, %d bytes) from %s to %s FAILED\n", strCommand.c_str(), nMessageSize, pfrom->addr.ToString().c_str(), addrLocalHost.ToString().c_str());
//     }

//     vRecv.Compact();
//     return true;
// }




bool ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    static map<unsigned int, vector<unsigned char> > mapReuseKey;
    printf("received: %-12s (%d bytes)  ", strCommand.c_str(), vRecv.size());
    for (int i = 0; i < static_cast<int>(std::min(vRecv.size(), static_cast<size_t>(20))); i++)
        printf("%02x ", vRecv[i] & 0xff);

    printf("\n");
    if (nDropMessagesTest > 0 && GetRand(nDropMessagesTest) == 0)
    {
        printf("dropmessages DROPPING RECV MESSAGE\n");
        return true;
    }



    if (strCommand == "version")
    {
        // Can only do this once
        if (pfrom->nVersion != 0)
            return false;

        int64 nTime;
        CAddress addrMe;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion == 0)
            return false;

        pfrom->vSend.SetVersion(min(pfrom->nVersion, VERSION));
        pfrom->vRecv.SetVersion(min(pfrom->nVersion, VERSION));

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);
        if (pfrom->fClient)
        {
            pfrom->vSend.nType |= SER_BLOCKHEADERONLY;
            pfrom->vRecv.nType |= SER_BLOCKHEADERONLY;
        }

        AddTimeData(pfrom->addr.ip, nTime);

        // Ask the first connected node for block updates
        static bool fAskedForBlocks;
        if (!fAskedForBlocks && !pfrom->fClient)
        {
            fAskedForBlocks = true;
            pfrom->PushMessage("getblocks", CBlockLocator(pindexBest), uint256(0));
        }

        printf("version message: %s has version %d, addrMe=%s\n", pfrom->addr.ToString().c_str(), pfrom->nVersion, addrMe.ToString().c_str());
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        return false;
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Store the new addresses
        CAddrDB addrdb;
        foreach(const CAddress& addr, vAddr)
        {
            if (fShutdown)
                return true;
            if (AddAddress(addrdb, addr))
            {
                // Put on lists to send to other nodes
                pfrom->setAddrKnown.insert(addr);
                CRITICAL_BLOCK(cs_vNodes)
                foreach(CNode* pnode, vNodes)
                    if (!pnode->setAddrKnown.count(addr))
                        pnode->vAddrToSend.push_back(addr);
            }
        }
    }


    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;

        CTxDB txdb("r");
        foreach(const CInv& inv, vInv)
        {
            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            printf("  got inventory: %s  %s\n", inv.ToString().c_str(), fAlreadyHave ? "have" : "new");

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash))
                pfrom->PushMessage("getblocks", CBlockLocator(pindexBest), GetOrphanRoot(mapOrphanBlocks[inv.hash]));
        }
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;

        foreach(const CInv& inv, vInv)
        {
            if (fShutdown)
                return true;
            printf("received getdata for: %s\n", inv.ToString().c_str());

            if (inv.type == MSG_BLOCK)
            {
                // Send block from disk
                map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    //// could optimize this to send header straight from blockindex for client
                    CBlock block;
                    block.ReadFromDisk((*mi).second, !pfrom->fClient);
                    pfrom->PushMessage("block", block);
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                CRITICAL_BLOCK(cs_mapRelay)
                {
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end())
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                }
            }
        }
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        // Find the first block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        printf("getblocks %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,14).c_str());
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                printf("  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,14).c_str());
                break;
            }

            // Bypass setInventoryKnown in case an inventory message got lost
            CRITICAL_BLOCK(pfrom->cs_inventory)
            {
                CInv inv(MSG_BLOCK, pindex->GetBlockHash());
                // returns true if wasn't already contained in the set
                if (pfrom->setInventoryKnown2.insert(inv).second)
                {
                    pfrom->setInventoryKnown.erase(inv);
                    pfrom->vInventoryToSend.push_back(inv);
                }
            }
        }
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        CDataStream vMsg(vRecv);
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fMissingInputs = false;
        if (tx.AcceptTransaction(true, &fMissingInputs))
        {
            AddToWalletIfMine(tx, NULL);
            RelayMessage(inv, vMsg);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (multimap<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev.lower_bound(hashPrev);
                     mi != mapOrphanTransactionsByPrev.upper_bound(hashPrev);
                     ++mi)
                {
                    const CDataStream& vMsg = *((*mi).second);
                    CTransaction tx;
                    CDataStream(vMsg) >> tx;
                    CInv inv(MSG_TX, tx.GetHash());

                    if (tx.AcceptTransaction(true))
                    {
                        printf("   accepted orphan tx %s\n", inv.hash.ToString().substr(0,6).c_str());
                        AddToWalletIfMine(tx, NULL);
                        RelayMessage(inv, vMsg);
                        mapAlreadyAskedFor.erase(inv);
                        vWorkQueue.push_back(inv.hash);
                    }
                }
            }

            foreach(uint256 hash, vWorkQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            printf("storing orphan tx %s\n", inv.hash.ToString().substr(0,6).c_str());
            AddOrphanTx(vMsg);
        }
    }


    // else if (strCommand == "review")
    // {
    //     CDataStream vMsg(vRecv);
    //     CReview review;
    //     vRecv >> review;

    //     CInv inv(MSG_REVIEW, review.GetHash());
    //     pfrom->AddInventoryKnown(inv);

    //     if (review.AcceptReview())
    //     {
    //         // Relay the original message as-is in case it's a higher version than we know how to parse
    //         RelayMessage(inv, vMsg);
    //         mapAlreadyAskedFor.erase(inv);
    //     }
    // }


    // else if (strCommand == "block")
    // {
    //     auto_ptr<CBlock> pblock(new CBlock);
    //     vRecv >> *pblock;

    //     //// debug print
    //     printf("received block:\n"); pblock->print();

    //     CInv inv(MSG_BLOCK, pblock->GetHash());
    //     pfrom->AddInventoryKnown(inv);

    //     if (ProcessBlock(pfrom, pblock.release()))
    //         mapAlreadyAskedFor.erase(inv);
    // }


    else if (strCommand == "getaddr")
    {
        pfrom->vAddrToSend.clear();
        int64 nSince = GetAdjustedTime() - 5 * 24 * 60 * 60; // in the last 5 days
        CRITICAL_BLOCK(cs_mapAddresses)
        {
            unsigned int nSize = mapAddresses.size();

            // Replacing 'foreach' with a C++11 range-based for loop
            for (const auto& item : mapAddresses)
            {
                if (fShutdown)
                    return true;

                const CAddress& addr = item.second;
                // Will need this if we lose IRC
                // if (addr.nTime > nSince || (rand() % nSize) < 500)
                if (addr.nTime > nSince)
                    pfrom->vAddrToSend.push_back(addr);
            }
        }
    }


    else if (strCommand == "checkorder")
    {
        uint256 hashReply;
        CWalletTx order;
        vRecv >> hashReply >> order;

        /// we have a chance to check the order here

        // Keep giving the same key to the same ip until they use it
        if (!mapReuseKey.count(pfrom->addr.ip))
            mapReuseKey[pfrom->addr.ip] = GenerateNewKey();

        // Send back approval of order and pubkey to use
        CScript scriptPubKey;
        scriptPubKey << mapReuseKey[pfrom->addr.ip] << OP_CHECKSIG;
        pfrom->PushMessage("reply", hashReply, (int)0, scriptPubKey);
    }


    else if (strCommand == "submitorder")
    {
        uint256 hashReply;
        CWalletTx wtxNew;
        vRecv >> hashReply >> wtxNew;

        // Broadcast
        if (!wtxNew.AcceptWalletTransaction())
        {
            pfrom->PushMessage("reply", hashReply, (int)1);
            return error("submitorder AcceptWalletTransaction() failed, returning error 1");
        }
        wtxNew.fTimeReceivedIsTxTime = true;
        AddToWallet(wtxNew);
        wtxNew.RelayWalletTransaction();
        mapReuseKey.erase(pfrom->addr.ip);

        // Send back confirmation
        pfrom->PushMessage("reply", hashReply, (int)0);
    }


    else if (strCommand == "reply")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        CRequestTracker tracker;
        CRITICAL_BLOCK(pfrom->cs_mapRequests)
        {
            map<uint256, CRequestTracker>::iterator mi = pfrom->mapRequests.find(hashReply);
            if (mi != pfrom->mapRequests.end())
            {
                tracker = (*mi).second;
                pfrom->mapRequests.erase(mi);
            }
        }
        if (!tracker.IsNull())
            tracker.fn(tracker.param1, vRecv);
    }


    else
    {
        // Ignore unknown commands for extensibility
        printf("ProcessMessage(%s) : Ignored unknown message\n", strCommand.c_str());
    }


    if (!vRecv.empty())
        printf("ProcessMessage(%s) : %d extra bytes\n", strCommand.c_str(), vRecv.size());

    return true;
}









bool SendMessages(CNode* pto)
{
    CheckForShutdown(2);
    CRITICAL_BLOCK(cs_main)
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;


        //
        // Message: addr
        //
        vector<CAddress> vAddrToSend;
        vAddrToSend.reserve(pto->vAddrToSend.size());
        foreach(const CAddress& addr, pto->vAddrToSend)
            if (!pto->setAddrKnown.count(addr))
                vAddrToSend.push_back(addr);
        pto->vAddrToSend.clear();
        if (!vAddrToSend.empty())
            pto->PushMessage("addr", vAddrToSend);


        //
        // Message: inventory
        //
        vector<CInv> vInventoryToSend;
        CRITICAL_BLOCK(pto->cs_inventory)
        {
            vInventoryToSend.reserve(pto->vInventoryToSend.size());
            foreach(const CInv& inv, pto->vInventoryToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                    vInventoryToSend.push_back(inv);
            }
            pto->vInventoryToSend.clear();
            pto->setInventoryKnown2.clear();
        }
        if (!vInventoryToSend.empty())
            pto->PushMessage("inv", vInventoryToSend);


        //
        // Message: getdata
        //
        vector<CInv> vAskFor;
        int64 nNow = GetTime() * 1000000;
        CTxDB txdb("r");
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            printf("sending getdata: %s\n", inv.ToString().c_str());
            if (!AlreadyHave(txdb, inv))
                vAskFor.push_back(inv);
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vAskFor.empty())
            pto->PushMessage("getdata", vAskFor);

    }
    return true;
}














//////////////////////////////////////////////////////////////////////////////
//
// Miner
//

int FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

using CryptoPP::ByteReverse;
static int detectlittleendian = 1;

void BlockSHA256(const void* pin, unsigned int nBlocks, void* pout)
{
    unsigned int* pinput = (unsigned int*)pin;
    unsigned int* pstate = (unsigned int*)pout;

    CryptoPP::SHA256::InitState(pstate);

    if (*(char*)&detectlittleendian != 0)
    {
        for (int n = 0; n < nBlocks; n++)
        {
            unsigned int pbuf[16];
            for (int i = 0; i < 16; i++)
                pbuf[i] = ByteReverse(pinput[n * 16 + i]);
            CryptoPP::SHA256::Transform(pstate, pbuf);
        }
        for (int i = 0; i < 8; i++)
            pstate[i] = ByteReverse(pstate[i]);
    }
    else
    {
        for (int n = 0; n < nBlocks; n++)
            CryptoPP::SHA256::Transform(pstate, pinput + n * 16);
    }
}



bool CoinMiner()
{
    printf("Miner started\n");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    CKey key;
    key.MakeNewKey();
    CBigNum bnExtraNonce = 0;
    while (fGenerateCoins)
    {
        // Sleep(50);
        // CheckForShutdown(3);
        // while (vNodes.empty())
        // {
        //     Sleep(1000);
        //     CheckForShutdown(3);
        // }

        unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
        CBlockIndex* pindexPrev = pindexBest;
        unsigned int nBits = GetNextWorkRequired(pindexPrev);


        //
        // Create coinbase tx
        //
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vin[0].scriptSig << nBits << ++bnExtraNonce;
        txNew.vout.resize(1);
        txNew.vout[0].scriptPubKey << key.GetPubKey() << OP_CHECKSIG;


        //
        // Create new block
        //
        std::unique_ptr<CBlock> pblock(new CBlock());  // Use unique_ptr instead of auto_ptr
        if (!pblock.get())
            return false;

        // Add our coinbase tx as first transaction
        pblock->vtx.push_back(txNew);

        // Collect the latest transactions into the block
        int64 nFees = 0;
        CRITICAL_BLOCK(cs_main)
        CRITICAL_BLOCK(cs_mapTransactions)
        {
            CTxDB txdb("r");
            map<uint256, CTxIndex> mapTestPool;
            vector<char> vfAlreadyAdded(mapTransactions.size());
            bool fFoundSomething = true;
            unsigned int nBlockSize = 0;
            while (fFoundSomething && nBlockSize < MAX_SIZE/2)
            {
                fFoundSomething = false;
                unsigned int n = 0;
                for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi, ++n) //map transaction needs to have something
                {
                    if (vfAlreadyAdded[n])
                        continue;
                    CTransaction& tx = (*mi).second;
                    if (tx.IsCoinBase() || !tx.IsFinal())
                        continue;

                    // Transaction fee requirements, mainly only needed for flood control
                    // Under 10K (about 80 inputs) is free for first 100 transactions
                    // Base rate is 0.01 per KB
                    int64 nMinFee = tx.GetMinFee(pblock->vtx.size() < 100);

                    map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
                    if (!tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), 0, nFees, false, true, nMinFee))
                        continue;
                    swap(mapTestPool, mapTestPoolTmp);

                    pblock->vtx.push_back(tx);
                    nBlockSize += ::GetSerializeSize(tx, SER_NETWORK);
                    vfAlreadyAdded[n] = true;
                    fFoundSomething = true;
                }
            }
        }
        pblock->nBits = nBits;
        pblock->vtx[0].vout[0].nValue = pblock->GetBlockValue(nFees);
        printf("\n\nRunning CoinMiner with %d transactions in block\n", pblock->vtx.size());


        //
        // Prebuild hash buffer
        //

        unnamed1 tmp;

        tmp.block.nVersion              = pblock->nVersion;
        tmp.block.hashPrevBlock         = pblock->hashPrevBlock         = (pindexPrev ? pindexPrev->GetBlockHash() : 0);
        tmp.block.hashMerkleRoot        = pblock->hashMerkleRoot        = pblock->BuildMerkleTree();
        tmp.block.hashContextMerkleRoot = pblock->hashContextMerkleRoot = pblock->BuildMerkleTree();
        tmp.block.nTime                 = pblock->nTime                 = max((pindexPrev ? pindexPrev->GetMedianTimePast()+1 : 0), GetAdjustedTime());
        tmp.block.nBits                 = pblock->nBits          = nBits;
        tmp.block.nNonce                = pblock->nNonce         = 1;//564369126; // here make it infere the first nonce

        unsigned int nBlocks0 = FormatHashBlocks(&tmp.block, sizeof(tmp.block));
        unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

        //
        // Inferece
        //

        // Incorporating GPT nonce inference into your proof-of-work loop
        unsigned int inferred_nonce = gpt_predict_nonce(tmp.block);

        // Start with the inferred nonce and perform traditional proof-of-work validation
        tmp.block.nNonce = pblock->nNonce = inferred_nonce;


        unnamed1 tmp2 = tmp; // deep copy
        //
        // Search
        //


        unsigned int nStart = GetTime();
        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
        uint256 hash;
        loop
        {
            BlockSHA256(&tmp.block, nBlocks0, &tmp.hash1);
            BlockSHA256(&tmp.hash1, nBlocks1, &hash);

            if (hash <= hashTarget) //
            {
                pblock->nNonce = tmp.block.nNonce;
                assert(hash == pblock->GetHash());

                //// debug print
                printf("CoinMiner:\n");
                printf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
                pblock->print(); // this

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                CRITICAL_BLOCK(cs_main)
                {
                    // Save key
                    if (!AddKey(key))
                        return false;
                    key.MakeNewKey();
                    // finetune model on the recently found nonce
                    // Process this block the same as if we had received it from another node
                    if (!ProcessBlock(NULL, pblock.release()))
                    {
                        printf("ERROR in CoinMiner, ProcessBlock, block not accepted\n");
                    }
                    // dataloader_next_batch(&train_loader, tmp.block);
                    train(tmp.block);
                }
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

                Sleep(500);
                break;
            }

            // Update nTime every few seconds
            if ((++tmp.block.nNonce & 0x3ffff) == 0)
            {
                CheckForShutdown(3);
                if (tmp.block.nNonce == 0)
                    break;
                if (pindexPrev != pindexBest)
                    break;
                if (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (!fGenerateCoins)
                    break;
                tmp.block.nTime = pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
            }
        }
    }

    return true;
}



bool MinerTool() {
    printf("Genesis Block Miner started\n");

    CKey key;
    key.MakeNewKey();

    // Set a lower difficulty target for faster mining
    unsigned int nBits = 0x2000a3d7;  // Adjusted for 10x easier mining

    //
    // Create the coinbase transaction
    //
    const char* pszTimestamp = "The most beautiful test";
    CTransaction txNew;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vin[0].scriptSig << nBits << CBigNum(4) << vector<unsigned char>((unsigned char*)pszTimestamp, (unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = 50 * COIN;
    txNew.vout[0].scriptPubKey << key.GetPubKey() << OP_CHECKSIG;

    //
    // Initialize genesis block
    //
    std::unique_ptr<CBlock> pblock(new CBlock());
    pblock->vtx.push_back(txNew);
    pblock->hashPrevBlock = 0;  // No previous block for genesis
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
    pblock->hashContextMerkleRoot = pblock->
    pblock->nVersion = 1;
    pblock->nTime = 1231006505;  // Fixed timestamp
    pblock->nBits = nBits;
    pblock->nNonce = 0;  // Start nonce at 0

    // Set the target difficulty for mining
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
    uint256 hash;

    // Mining loop
    printf("Mining genesis block...\n");
    while (true) {
        pblock->nNonce++;
        hash = pblock->GetHash();

        if (hash <= hashTarget) {  // Check if hash meets difficulty
            printf("Genesis Block Mined Successfully!\n");
            printf("Genesis Block Hash: %s\n", hash.GetHex().c_str());
            printf("Nonce: %u\n", pblock->nNonce);
            printf("Merkle Root: %s\n", pblock->hashMerkleRoot.ToString().c_str());
            printf("Time: %u\n", pblock->nTime);
            printf("Bits (Difficulty): %x\n", pblock->nBits);
            printf("Coinbase Transaction ScriptSig: %s\n", txNew.vin[0].scriptSig.ToString().c_str());
            printf("Coinbase Transaction Value: %ld\n", txNew.vout[0].nValue);
            printf("Coinbase Transaction ScriptPubKey: %s\n", txNew.vout[0].scriptPubKey.ToString().c_str());

            // Save the mined genesis block
            // ProcessBlock(NULL, pblock.release());
            break;
        }

        // Increment time if nonce overflows
        if (pblock->nNonce == 0) {
            pblock->nTime++;
        }
    }

    return true;
}












//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64 GetBalance()
{
    int64 nStart, nEnd;
    QueryPerformanceCounter((LARGE_INTEGER*)&nStart);

    int64 nTotal = 0;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || pcoin->fSpent)
                continue;
            nTotal += pcoin->GetCredit();
        }
    }

    QueryPerformanceCounter((LARGE_INTEGER*)&nEnd);
    ///printf(" GetBalance() time = %16I64d\n", nEnd - nStart);
    return nTotal;
}



bool SelectCoins(int64 nTargetValue, set<CWalletTx*>& setCoinsRet)
{
    setCoinsRet.clear();

    // List of values less than target
    int64 nLowestLarger = _I64_MAX;
    CWalletTx* pcoinLowestLarger = NULL;
    vector<pair<int64, CWalletTx*> > vValue;
    int64 nTotalLower = 0;

    CRITICAL_BLOCK(cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            CWalletTx* pcoin = &(*it).second;
            if (!pcoin->IsFinal() || pcoin->fSpent)
                continue;
            int64 n = pcoin->GetCredit();
            if (n <= 0)
                continue;
            if (n < nTargetValue)
            {
                vValue.push_back(make_pair(n, pcoin));
                nTotalLower += n;
            }
            else if (n == nTargetValue)
            {
                setCoinsRet.insert(pcoin);
                return true;
            }
            else if (n < nLowestLarger)
            {
                nLowestLarger = n;
                pcoinLowestLarger = pcoin;
            }
        }
    }

    if (nTotalLower < nTargetValue)
    {
        if (pcoinLowestLarger == NULL)
            return false;
        setCoinsRet.insert(pcoinLowestLarger);
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend());
    vector<char> vfIncluded;
    vector<char> vfBest(vValue.size(), true);
    int64 nBest = nTotalLower;

    for (int nRep = 0; nRep < 1000 && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64 nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (int i = 0; i < vValue.size(); i++)
            {
                if (nPass == 0 ? rand() % 2 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }

    // If the next larger is still closer, return it
    if (pcoinLowestLarger && nLowestLarger - nTargetValue <= nBest - nTargetValue)
        setCoinsRet.insert(pcoinLowestLarger);
    else
    {
        for (int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                setCoinsRet.insert(vValue[i].second);

        //// debug print
        printf("SelectCoins() best subset: ");
        for (int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                printf("%s ", FormatMoney(vValue[i].first).c_str());
        printf("total %s\n", FormatMoney(nBest).c_str());
    }

    return true;
}




bool CreateTransaction(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, int64& nFeeRequiredRet)
{
    nFeeRequiredRet = 0;
    CRITICAL_BLOCK(cs_main)
    {
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapWallet)
        {
            int64 nFee = nTransactionFee;
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                if (nValue < 0)
                    return false;
                int64 nValueOut = nValue;
                nValue += nFee;

                // Choose coins to use
                set<CWalletTx*> setCoins;
                if (!SelectCoins(nValue, setCoins))
                    return false;
                int64 nValueIn = 0;
                foreach(CWalletTx* pcoin, setCoins)
                    nValueIn += pcoin->GetCredit();

                // Fill vout[0] to the payee
                wtxNew.vout.push_back(CTxOut(nValueOut, scriptPubKey));

                // Fill vout[1] back to self with any change
                if (nValueIn > nValue)
                {
                    /// todo: for privacy, should randomize the order of outputs,
                    //        would also have to use a new key for the change.
                    // Use the same key as one of the coins
                    vector<unsigned char> vchPubKey;
                    CTransaction& txFirst = *(*setCoins.begin());
                    foreach(const CTxOut& txout, txFirst.vout)
                        if (txout.IsMine())
                            if (ExtractPubKey(txout.scriptPubKey, true, vchPubKey))
                                break;
                    if (vchPubKey.empty())
                        return false;

                    // Fill vout[1] to ourself
                    CScript scriptPubKey;
                    scriptPubKey << vchPubKey << OP_CHECKSIG;
                    wtxNew.vout.push_back(CTxOut(nValueIn - nValue, scriptPubKey));
                }

                // Fill vin
                foreach(CWalletTx* pcoin, setCoins)
                    for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
                        if (pcoin->vout[nOut].IsMine())
                            wtxNew.vin.push_back(CTxIn(pcoin->GetHash(), nOut));

                // Sign
                int nIn = 0;
                foreach(CWalletTx* pcoin, setCoins)
                    for (int nOut = 0; nOut < pcoin->vout.size(); nOut++)
                        if (pcoin->vout[nOut].IsMine())
                            SignSignature(*pcoin, wtxNew, nIn++);

                // Check that enough fee is included
                if (nFee < wtxNew.GetMinFee(true))
                {
                    nFee = nFeeRequiredRet = wtxNew.GetMinFee(true);
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

// Call after CreateTransaction unless you want to abort
bool CommitTransactionSpent(const CWalletTx& wtxNew)
{
    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(cs_mapWallet)
    {
        //// todo: make this transactional, never want to add a transaction
        ////  without marking spent transactions

        // Add tx to wallet, because if it has change it's also ours,
        // otherwise just for transaction history.
        AddToWallet(wtxNew);

        // Mark old coins as spent
        set<CWalletTx*> setCoins;
        foreach(const CTxIn& txin, wtxNew.vin)
            setCoins.insert(&mapWallet[txin.prevout.hash]);
        foreach(CWalletTx* pcoin, setCoins)
        {
            pcoin->fSpent = true;
            pcoin->WriteToDisk();
            vWalletUpdated.push_back(make_pair(pcoin->GetHash(), false));
        }
    }
    // MainFrameRepaint();
    return true;
}




bool SendMoney(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew)
{
    CRITICAL_BLOCK(cs_main)
    {
        int64 nFeeRequired;
        if (!CreateTransaction(scriptPubKey, nValue, wtxNew, nFeeRequired))
        {
            string strError;
            if (nValue + nFeeRequired > GetBalance())
                strError = strprintf("Error: This is an oversized transaction that requires a transaction fee of %s  ", FormatMoney(nFeeRequired).c_str());
            else
                strError = "Error: Transaction creation failed  ";
            QMessageBox::warning(nullptr, "Sending...", "Error ");

            return error("SendMoney() : %s\n", strError.c_str());
        }
        if (!CommitTransactionSpent(wtxNew))
        {
            QMessageBox::warning(nullptr, "Sending...", "Error finalizing transaction  ");
            return error("SendMoney() : Error finalizing transaction");
        }

        printf("SendMoney: %s\n", wtxNew.GetHash().ToString().substr(0,6).c_str());

        // Broadcast
        if (!wtxNew.AcceptTransaction())
        {
            // This must not fail. The transaction has already been signed and recorded.
            throw runtime_error("SendMoney() : wtxNew.AcceptTransaction() failed\n");
            QMessageBox::warning(nullptr, "Sending...", "Error: Transaction not valid");
            return error("SendMoney() : Error: Transaction not valid");
        }
        wtxNew.RelayWalletTransaction();
    }
    // MainFrameRepaint();
    return true;
}
