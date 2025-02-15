// #ifndef BLOCK_H
// #define BLOCK_H
// #include <QFile>
// #include <QIODevice>
// #include <QString>
// #include <QDebug>
// #include <vector>
// #include <QString>
// #include <QDebug>
// #include <algorithm>
// #include "uint256.h"  // Assuming uint256 class is available
// #include "serialize.h"  // Assuming CTransaction class is available
// #include "bignum.h"   // Assuming CBigNum class is available
// #include <QCoreApplication>
// #include <QDir>

// class CBlock {
// public:
//     // Header fields
//     int nVersion;
//     uint256 hashPrevBlock;
//     uint256 hashMerkleRoot;
//     unsigned int nTime;
//     unsigned int nBits;
//     unsigned int nNonce;

//     // Network and disk
//     std::vector<CTransaction> vtx;

//     // Memory only
//     mutable std::vector<uint256> vMerkleTree;

//     // Constructor
//     CBlock() {
//         SetNull();
//     }

//     // Serialization placeholder (assuming this is implemented somewhere)
//     // void IMPLEMENT_SERIALIZE();

//     // Set to null values
//     void SetNull() {
//         nVersion = 1;
//         hashPrevBlock = uint256(0);
//         hashMerkleRoot = uint256(0);
//         nTime = 0;
//         nBits = 0;
//         nNonce = 0;
//         vtx.clear();
//         vMerkleTree.clear();
//     }

//     // Check if the block is null
//     bool IsNull() const {
//         return (nBits == 0);
//     }

//     // Get the block hash (Placeholder for actual hashing logic)
//     uint256 GetHash() const {
//         return Hash(BEGIN(nVersion), END(nNonce));  // Assuming Hash() is implemented elsewhere
//     }

//     // Build the Merkle tree from transactions
//     uint256 BuildMerkleTree() const {
//         vMerkleTree.clear();
//         for (const CTransaction& tx : vtx) {
//             vMerkleTree.push_back(tx.GetHash());
//         }

//         int j = 0;
//         for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2) {
//             for (int i = 0; i < nSize; i += 2) {
//                 int i2 = std::min(i + 1, nSize - 1);
//                 vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j + i]), END(vMerkleTree[j + i]),
//                                            BEGIN(vMerkleTree[j + i2]), END(vMerkleTree[j + i2])));
//             }
//             j += nSize;
//         }
//         return (vMerkleTree.empty() ? uint256(0) : vMerkleTree.back());
//     }

//     // Get the Merkle branch for a transaction
//     std::vector<uint256> GetMerkleBranch(int nIndex) const {
//         if (vMerkleTree.empty())
//             BuildMerkleTree();

//         std::vector<uint256> vMerkleBranch;
//         int j = 0;
//         for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2) {
//             int i = std::min(nIndex ^ 1, nSize - 1);
//             vMerkleBranch.push_back(vMerkleTree[j + i]);
//             nIndex >>= 1;
//             j += nSize;
//         }
//         return vMerkleBranch;
//     }

//     // Check the Merkle branch (static method)
//     static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex) {
//         if (nIndex == -1)
//             return uint256(0);

//         for (const uint256& otherside : vMerkleBranch) {
//             if (nIndex & 1)
//                 hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
//             else
//                 hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
//             nIndex >>= 1;
//         }
//         return hash;
//     }

//     // Print block information for debugging
//     QString ToString() const {
//         return QString("CBlock(hash=%1, ver=%2, hashPrevBlock=%3, hashMerkleRoot=%4, nTime=%5, nBits=%6, nNonce=%7, vtx=%8)")
//         .arg(QString::fromStdString(GetHash().ToString()).left(14))
//             .arg(nVersion)
//             .arg(QString::fromStdString(hashPrevBlock.ToString()).left(14))
//             .arg(QString::fromStdString(hashMerkleRoot.ToString()).left(6))
//             .arg(nTime)
//             .arg(nBits, 8, 16, QChar('0'))
//             .arg(nNonce)
//             .arg(vtx.size());
//     }

//     void print() const {
//         qDebug() << ToString();
//         for (int i = 0; i < vtx.size(); i++) {
//             qDebug() << "  Transaction:" << vtx[i].ToString();
//         }
//         qDebug() << "  vMerkleTree:";
//         for (int i = 0; i < vMerkleTree.size(); i++) {
//             qDebug() << vMerkleTree[i].ToString().left(6);
//         }
//     }

//     // Placeholder methods for disk operations and block validation
//     int64_t GetBlockValue(int64_t nFees) const;
//     bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
//     bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex);
//     bool ReadFromDisk(const CBlockIndex* blockindex, bool fReadTransactions);
//     bool AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos);
//     bool CheckBlock() const;
//     bool AcceptBlock();
// };
// // Define GetAppDir to return the application directory
// QString GetAppDir()
// {
//     // Gets the directory of the currently running application
//     return QDir(QCoreApplication::applicationDirPath()).absolutePath();
// }

// QFile* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, QIODevice::OpenMode mode)
// {
//     if (nFile == static_cast<unsigned int>(-1))
//         return nullptr;

//     // Format the file name using QString
//     QString fileName = QString("%1/blk%2.dat").arg(GetAppDir()).arg(nFile, 4, 10, QChar('0'));

//     // Create a QFile object
//     QFile* file = new QFile(fileName);

//     // Try to open the file in the given mode
//     if (!file->open(mode))
//     {
//         delete file;
//         return nullptr;
//     }

//     // Seek to the specific position if required
//     if (nBlockPos != 0 && !(mode & QIODevice::Append) && !(mode & QIODevice::WriteOnly))
//     {
//         if (!file->seek(nBlockPos))
//         {
//             file->close();
//             delete file;
//             return nullptr;
//         }
//     }

//     return file;
// }

// class CBlockIndex {
// public:
//     const uint256* phashBlock;
//     CBlockIndex* pprev;
//     CBlockIndex* pnext;
//     unsigned int nFile;
//     unsigned int nBlockPos;
//     int nHeight;

//     // Block header fields
//     int nVersion;
//     uint256 hashMerkleRoot;
//     unsigned int nTime;
//     unsigned int nBits;
//     unsigned int nNonce;

//     // Default constructor
//     CBlockIndex()
//         : phashBlock(nullptr),
//         pprev(nullptr),
//         pnext(nullptr),
//         nFile(0),
//         nBlockPos(0),
//         nHeight(0),
//         nVersion(0),
//         hashMerkleRoot(0),
//         nTime(0),
//         nBits(0),
//         nNonce(0) {}

//     // Constructor initializing from a block
//     CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, const CBlock& block)
//         : phashBlock(nullptr),
//         pprev(nullptr),
//         pnext(nullptr),
//         nFile(nFileIn),
//         nBlockPos(nBlockPosIn),
//         nHeight(0),
//         nVersion(block.nVersion),
//         hashMerkleRoot(block.hashMerkleRoot),
//         nTime(block.nTime),
//         nBits(block.nBits),
//         nNonce(block.nNonce) {}

//     // Get the block hash
//     uint256 GetBlockHash() const {
//         return *phashBlock;
//     }

//     // Check if the block is in the main chain
//     bool IsInMainChain() const {
//         return (pnext != nullptr || this == pindexBest);  // Assuming pindexBest is defined elsewhere
//     }

//     // Erase the block from disk
//     bool EraseBlockFromDisk() {
//         // Open history file
//         CAutoFile fileout = OpenBlockFile(nFile, nBlockPos, "rb+");  // Assuming OpenBlockFile is defined elsewhere
//         if (!fileout)
//             return false;

//         // Overwrite with empty null block
//         CBlock block;
//         block.SetNull();
//         fileout << block;

//         return true;
//     }

//     // Get the median time of the past blocks
//     enum { nMedianTimeSpan = 11 };

//     int64_t GetMedianTimePast() const {
//         unsigned int pmedian[nMedianTimeSpan];
//         unsigned int* pbegin = &pmedian[nMedianTimeSpan];
//         unsigned int* pend = &pmedian[nMedianTimeSpan];

//         const CBlockIndex* pindex = this;
//         for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
//             *(--pbegin) = pindex->nTime;

//         std::sort(pbegin, pend);
//         return pbegin[(pend - pbegin) / 2];
//     }

//     // Get the median time for this block
//     int64_t GetMedianTime() const {
//         const CBlockIndex* pindex = this;
//         for (int i = 0; i < nMedianTimeSpan / 2; i++) {
//             if (!pindex->pnext)
//                 return nTime;
//             pindex = pindex->pnext;
//         }
//         return pindex->GetMedianTimePast();
//     }

//     // Convert the block index to a string representation
//     QString ToString() const {
//         return QString("CBlockIndex(nprev=%1, pnext=%2, nFile=%3, nBlockPos=%4, nHeight=%5, merkle=%6, hashBlock=%7)")
//         .arg(reinterpret_cast<quintptr>(pprev), 8, 16, QChar('0'))
//             .arg(reinterpret_cast<quintptr>(pnext), 8, 16, QChar('0'))
//             .arg(nFile)
//             .arg(nBlockPos)
//             .arg(nHeight)
//             .arg(QString::fromStdString(hashMerkleRoot.ToString()).left(6))
//             .arg(QString::fromStdString(GetBlockHash().ToString()).left(14));
//     }

//     // Print the block index details to the debug output
//     void print() const {
//         qDebug() << ToString();
//     }
// };
// #endif // BLOCK_H
