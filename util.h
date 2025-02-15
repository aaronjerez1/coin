#ifndef UTIL_H
#define UTIL_H

// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license

// Use standard int64_t and uint64_t instead of platform-specific typedefs
typedef int64_t  int64;
typedef uint64_t uint64;

// Modern C++ doesn't need this 'for' macro for old MSVC compilers

// Define __forceinline as inline for non-MSVC compilers
#ifndef _MSC_VER
#define __forceinline inline
#endif

// Use range-based for loops instead of BOOST_FOREACH
// #define foreach             for

// Infinite loop
#define loop                for (;;)

// Use std::begin and std::end for better portability
#define BEGIN(a)            (reinterpret_cast<char*>(&a))
#define END(a)              (reinterpret_cast<char*>(&((&a)[1])))
#define UBEGIN(a)           (reinterpret_cast<unsigned char*>(&a))
#define UEND(a)             (reinterpret_cast<unsigned char*>(&((&a)[1])))

// Use std::size for array length in modern C++
#define ARRAYLEN(array)     (std::size(array))

#ifdef _WINDOWS
// Redirect printf to OutputDebugStringF if desired (custom logging function)
#define printf              OutputDebugStringF
#endif

// Ensure snprintf is standard and portable
#ifdef snprintf
#undef snprintf
#endif
#define snprintf std::snprintf

// Define format specifiers for 64-bit integers
#ifndef PRId64
#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__MSVCRT__)
#define PRId64  "I64d"
#define PRIu64  "I64u"
#define PRIx64  "I64x"
#else
#define PRId64  "lld"
#define PRIu64  "llu"
#define PRIx64  "llx"
#endif
#endif

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

// Used to bypass the rule against non-const reference to temporary
template<typename T>
inline T& REF(const T& val) {
    return (T&)val;
}

extern bool fDebug;

void RandAddSeed(bool fPerfmon=false);
int my_snprintf(char* buffer, size_t limit, const char* format, ...);
std::string strprintf(const char* format, ...);
bool error(const char* format, ...);
void PrintException(std::exception* pex, const char* pszThread);
void ParseString(const string& str, char c, vector<string>& v);
std::string FormatMoney(int64 n, bool fPlus=false);
bool ParseMoney(const char* pszIn, int64& nRet);
bool FileExists(const char* psz);
int GetFilesize(FILE* file);
uint64 GetRand(uint64 nMax);
int64 GetTime();
int64 GetAdjustedTime();
void AddTimeData(unsigned int ip, int64 nTime);

class CCriticalBlock;  // Forward declare CCriticalBlock
class CTryCriticalBlock;
// Wrapper for QMutex (similar to your CCriticalSection)
class CCriticalSection
{
protected:
    QMutex myMutex;
public:
    const char* pszFile;
    int nLine;
    explicit CCriticalSection() = default;
    ~CCriticalSection() = default;
    void Enter() { myMutex.lock(); }
    void Leave() { myMutex.unlock(); }
    bool TryEnter() { return myMutex.tryLock(); }
    // Declare CCriticalBlock as a friend
    friend class CCriticalBlock;
    friend class CTryCriticalBlock;
    // QMutex* operator&() { return &mutex; }
};

// Automatically leave critical section when leaving block (exception safety)
class CCriticalBlock
{
protected:
    QMutex* pmutex;
public:
    CCriticalBlock(QMutex& mutexIn) { pmutex = &mutexIn; pmutex->lock(); }
    CCriticalBlock(CCriticalSection& csIn) { pmutex = &csIn.myMutex; pmutex->lock(); }
    ~CCriticalBlock() { pmutex->unlock(); }
};

// Modern version of TRY_CRITICAL_BLOCK using QMutex tryLock
class CTryCriticalBlock
{
protected:
    QMutex* pmutex;
public:

    CTryCriticalBlock(QMutex& mutexIn) { pmutex = (mutexIn.tryLock() ? &mutexIn : nullptr); }
    CTryCriticalBlock(CCriticalSection& csIn) { pmutex = (csIn.myMutex.tryLock() ? &csIn.myMutex : nullptr); }
    ~CTryCriticalBlock() { if (pmutex) pmutex->unlock(); }

    bool Entered() { return pmutex != nullptr; }
};

// CRITICAL_BLOCK Macro
#define CRITICAL_BLOCK(cs) \
for (bool fcriticalblockonce = true; fcriticalblockonce; assert(("break caught by CRITICAL_BLOCK!", !fcriticalblockonce)), fcriticalblockonce = false) \
        for (CCriticalBlock criticalblock(cs); fcriticalblockonce && (cs.pszFile = __FILE__, cs.nLine = __LINE__, true); fcriticalblockonce = false, cs.pszFile = nullptr, cs.nLine = 0)

// TRY_CRITICAL_BLOCK Macro
#define TRY_CRITICAL_BLOCK(cs) \
        for (bool fcriticalblockonce = true; fcriticalblockonce; assert(("break caught by TRY_CRITICAL_BLOCK!", !fcriticalblockonce)), fcriticalblockonce = false) \
        for (CTryCriticalBlock criticalblock(cs); fcriticalblockonce && (fcriticalblockonce = criticalblock.Entered()) && (cs.pszFile = __FILE__, cs.nLine = __LINE__, true); fcriticalblockonce = false, cs.pszFile = nullptr, cs.nLine = 0)



inline std::string i64tostr(int64 n) {
    return strprintf("%" PRId64, n);
}

inline std::string itostr(int n) {
    return strprintf("%d", n);
}

inline int64 atoi64(const char* psz) {
    return std::strtoll(psz, nullptr, 10);
}

inline int64 atoi64(const std::string& str) {
    return std::strtoll(str.c_str(), nullptr, 10);
}

inline int atoi(const std::string& str) {
    return std::atoi(str.c_str());
}

inline int roundint(double d) {
    return static_cast<int>(d > 0 ? d + 0.5 : d - 0.5);
}

// Function templates for serialization/deserialization
template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces = true) {
    const unsigned char* pbegin = reinterpret_cast<const unsigned char*>(&*itbegin);
    const unsigned char* pend = pbegin + (itend - itbegin) * sizeof(itbegin[0]);
    std::string str;
    for (const unsigned char* p = pbegin; p != pend; ++p) {
        str += strprintf((fSpaces && p != pend - 1) ? "%02x " : "%02x", *p);
    }
    return str;
}

template<typename T>
std::string HexNumStr(const T itbegin, const T itend, bool f0x = true)
{
    const unsigned char* pbegin = (const unsigned char*)&itbegin[0];
    const unsigned char* pend = pbegin + (itend - itbegin) * sizeof(itbegin[0]);
    std::string str = (f0x ? "0x" : "");

    // Loop through each byte from end to beginning
    for (const unsigned char* p = pend - 1; p >= pbegin; p--) {
        str += strprintf("%02X", *p);
    }

    return str;
}

template<typename T>
void PrintHex(const T pbegin, const T pend, const char* pszFormat = "%s", bool fSpaces = true) {
#ifdef _WINDOWS
    OutputDebugStringF(pszFormat, HexStr(pbegin, pend, fSpaces).c_str());
#else
    std::printf(pszFormat, HexStr(pbegin, pend, fSpaces).c_str());
#endif
}



/// TODO very weird the mutex over here
// QMutex myMutex;
inline int OutputDebugStringF(const char* pszFormat, ...)
{
#ifdef QT_DEBUG  // Only compile this code in debug builds
    QString formattedString;

    // Using a variable argument list (va_list) and QString::vsnprintf equivalent
    va_list args;
    va_start(args, pszFormat);

    // Allocate a temporary buffer to hold the formatted string
    char buffer[1024];  // Adjust size as necessary
    vsnprintf(buffer, sizeof(buffer), pszFormat, args);
    va_end(args);

    // Convert the formatted C-style string to a QString
    formattedString = QString::fromUtf8(buffer);

    // File output
    QFile fileOut("debug.log");
    if (fileOut.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&fileOut);
        out << formattedString;
        fileOut.close();
    }

    // Accumulate a line at a time (thread-safe)
    // QMutexLocker locker(&myMutex);
    static QString bufferAccumulator;
    bufferAccumulator += formattedString;

    while (bufferAccumulator.contains('\n')) {
        int index = bufferAccumulator.indexOf('\n');
        QString line = bufferAccumulator.left(index + 1); // Extract the line up to '\n'
        bufferAccumulator.remove(0, index + 1);  // Remove the extracted line from the buffer

        // Debug output (Qt provides this)
        qDebug() << line.trimmed();  // Print without newline at the end
    }
#endif

    // Console output if no QCoreApplication (can happen in non-GUI applications)
    if (!QCoreApplication::instance()) {
        va_list argsConsole;
        va_start(argsConsole, pszFormat);
        vprintf(pszFormat, argsConsole);
        va_end(argsConsole);
    }

    return 0;
}

inline void heapchk()
{
    if (_heapchk() != _HEAPOK)
        Q_ASSERT(false && "Heap corruption detected");
}

// Randomize the stack to help protect against buffer overrun exploits
#define IMPLEMENT_RANDOMIZE_STACK(ThreadFn)                         \
{                                                               \
        static char nLoops;                                         \
        if (nLoops <= 0)                                            \
        nLoops = GetRand(50) + 1;                               \
        if (nLoops-- > 1)                                           \
    {                                                           \
            ThreadFn;                                               \
            return;                                                 \
    }                                                           \
}

#define CATCH_PRINT_EXCEPTION(pszFn)     \
catch (std::exception& e) {          \
        PrintException(&e, (pszFn));     \
} catch (...) {                      \
        PrintException(NULL, (pszFn));   \
}




template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    uint256 hash1;
    SHA256((unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end)
{
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end,
                    const T3 p3begin, const T3 p3end)
{
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Update(&ctx, (unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T>
uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=VERSION)
{
    // Most of the time is spent allocating and deallocating CDataStream's
    // buffer.  If this ever needs to be optimized further, make a CStaticStream
    // class with its buffer on the stack.
    CDataStream ss(nType, nVersion);
    ss.reserve(10000);
    ss << obj;
    return Hash(ss.begin(), ss.end());
}

inline uint160 Hash160(const vector<unsigned char>& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

#endif // UTIL_H
