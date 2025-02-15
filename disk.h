// #ifndef DISK_H
// #define DISK_H



// class CDiskTxPos
// {
// public:
//     unsigned int nFile;
//     unsigned int nBlockPos;
//     unsigned int nTxPos;

//     CDiskTxPos()
//     {
//         SetNull();
//     }

//     CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
//     {
//         nFile = nFileIn;
//         nBlockPos = nBlockPosIn;
//         nTxPos = nTxPosIn;
//     }

//     IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
//     void SetNull() { nFile = -1; nBlockPos = 0; nTxPos = 0; }
//     bool IsNull() const { return (nFile == -1); }

//     friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
//     {
//         return (a.nFile     == b.nFile &&
//                 a.nBlockPos == b.nBlockPos &&
//                 a.nTxPos    == b.nTxPos);
//     }

//     friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
//     {
//         return !(a == b);
//     }

//     string ToString() const
//     {
//         if (IsNull())
//             return strprintf("null");
//         else
//             return strprintf("(nFile=%d, nBlockPos=%d, nTxPos=%d)", nFile, nBlockPos, nTxPos);
//     }

//     void print() const
//     {
//         printf("%s", ToString().c_str());
//     }
// };
// #endif // DISK_H
