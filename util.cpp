////// to be moved to a main file
// static const unsigned int MAX_SIZE = 0x02000000;
// static const int64 COIN = 100000000;
// static const int64 CENT = 1000000;
// static const int COINBASE_MATURITY = 100;


bool fDebug = false;  // Global debug flag

// OpenSSL lock callbacks for multithreading
static std::vector<std::unique_ptr<std::mutex>> lock_cs;

void locking_callback(int mode, int type, const char* file, int line)
{
    if (mode & CRYPTO_LOCK)
        lock_cs[type]->lock();
    else
        lock_cs[type]->unlock();
}


// OpenSSL init and shutdown
class CInit
{
public:
    CInit()
    {
        // Init OpenSSL library multithreading support (pre-OpenSSL 1.1.0)
        int numLocks = CRYPTO_num_locks();
        lock_cs.resize(numLocks);

        // Allocate a new mutex for each entry
        for (int i = 0; i < numLocks; ++i) {
            lock_cs[i] = std::make_unique<std::mutex>();
        }

        // Set the locking callback (pre-OpenSSL 1.1.0)
        CRYPTO_set_locking_callback(locking_callback);

        // Seed random number generator with system entropy
        if (RAND_poll() == 0) {
            std::fprintf(stderr, "RAND_poll failed, insufficient entropy\n");
        }

        RandAddSeed(true);
    }

    ~CInit()
    {
        // Shutdown OpenSSL library multithreading support (pre-OpenSSL 1.1.0)
        CRYPTO_set_locking_callback(nullptr);
        lock_cs.clear();  // Clean up the vector of locks
    }
};


// Instantiate the CInit object to initialize OpenSSL
// CInit instance_of_cinit;




void RandAddSeed(bool fPerfmon)
{
    // Windows-specific entropy collection
#ifdef _WIN32
    LARGE_INTEGER PerformanceCount;
    QueryPerformanceCounter(&PerformanceCount);
    RAND_add(&PerformanceCount, sizeof(PerformanceCount), 1.5);
    memset(&PerformanceCount, 0, sizeof(PerformanceCount));

    static int64_t nLastPerfmon;
    if (fPerfmon || GetTime() > nLastPerfmon + 5 * 60)
    {
        nLastPerfmon = GetTime();

        // Seed with the entire set of performance data (Windows-specific)
        unsigned char pdata[250000];
        memset(pdata, 0, sizeof(pdata));
        unsigned long nSize = sizeof(pdata);

        // Open the performance data registry key
        HKEY hKey;
        long ret = RegOpenKeyExA(HKEY_PERFORMANCE_DATA, NULL, 0, KEY_READ, &hKey);
        if (ret != ERROR_SUCCESS) {
            std::fprintf(stderr, "Failed to open HKEY_PERFORMANCE_DATA\n");
            return;
        }

        // Query the performance data using the ANSI version of RegQueryValueEx
        ret = RegQueryValueExA(hKey, "Global", NULL, NULL, pdata, &nSize);
        RegCloseKey(hKey);  // Close the key after usage

        if (ret == ERROR_SUCCESS)
        {
            uint256 hash;
            SHA256(pdata, nSize, (unsigned char*)&hash);
            RAND_add(&hash, sizeof(hash), min(nSize/500.0, (double)sizeof(hash)));
            memset(&hash, 0, sizeof(hash));
            memset(pdata, 0, nSize);

            time_t nTime;
            time(&nTime);
            struct tm* ptmTime = gmtime(&nTime);
            char pszTime[200];
            strftime(pszTime, sizeof(pszTime), "%x %H:%M:%S", ptmTime);
            OutputDebugStringF("%s  RandAddSeed() got %lu bytes of performance data\n", pszTime, nSize);
        }
    }
#endif
}




// Safer snprintf
//  - prints up to limit-1 characters
//  - output string is always null-terminated even if limit reached
//  - return value is the number of characters actually printed
int my_snprintf(char* buffer, size_t limit, const char* format, ...)
{
    if (limit == 0)
        return 0;

    va_list arg_ptr;
    va_start(arg_ptr, format);

    // Use the standard C vsnprintf function, which is cross-platform
    int ret = vsnprintf(buffer, limit, format, arg_ptr);

    va_end(arg_ptr);

    // Check if the return value is negative or if it exceeds the buffer limit
    if (ret < 0 || static_cast<size_t>(ret) >= limit)
    {
        ret = limit - 1;
        buffer[limit - 1] = '\0';  // Ensure null-termination
    }

    return ret;
}


std::string strprintf(const char* format, ...)
{
    char buffer[1024];  // Initial buffer size
    std::string result;
    int limit = sizeof(buffer);

    while (true)
    {
        va_list arg_ptr;
        va_start(arg_ptr, format);

        // Use vsnprintf to safely format the string
        int ret = vsnprintf(buffer, limit, format, arg_ptr);
        va_end(arg_ptr);

        // If the output fits in the buffer, we're done
        if (ret >= 0 && ret < limit)
        {
            result.assign(buffer, ret);  // Assign the formatted result to the string
            break;
        }

        // Otherwise, we need to increase the buffer size
        if (ret >= 0) {
            // If vsnprintf tells us the required size, use it
            limit = ret + 1;
        } else {
            // Otherwise, double the buffer size
            limit *= 2;
        }

        // Allocate a new buffer dynamically with the updated size
        std::unique_ptr<char[]> dynamicBuffer(new char[limit]);

        va_start(arg_ptr, format);
        ret = vsnprintf(dynamicBuffer.get(), limit, format, arg_ptr);
        va_end(arg_ptr);

        if (ret >= 0 && ret < limit)
        {
            result.assign(dynamicBuffer.get(), ret);
            break;
        }
    }

    return result;
}




bool error(const char* format, ...)
{
    char buffer[1024];  // Reasonable buffer size for error messages
    int limit = sizeof(buffer);

    // Start processing the variable arguments
    va_list arg_ptr;
    va_start(arg_ptr, format);

    // Use vsnprintf to format the string safely
    int ret = vsnprintf(buffer, limit, format, arg_ptr);

    va_end(arg_ptr);

    // Ensure that the buffer is null-terminated and handle errors
    if (ret < 0 || ret >= limit)
    {
        // If the output was truncated, null-terminate the string
        buffer[limit - 1] = '\0';
    }

    // Log the error message
    OutputDebugStringF("ERROR: %s\n", buffer);

    return false;
}


void PrintException(std::exception* pex, const char* pszThread)
{
    char pszModule[260];
    pszModule[0] = '\0';

#ifdef _WIN32
    // Get the module name
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));

    // Convert the module name to lowercase (portable method)
    std::transform(pszModule, pszModule + strlen(pszModule), pszModule, [](unsigned char c){ return std::tolower(c); });
#endif

    char pszMessage[1000];

    if (pex)
    {
        // Format the exception message with type and details
        snprintf(pszMessage, sizeof(pszMessage),
                 "EXCEPTION: %s       \n%s       \n%s in %s       \n",
                 typeid(*pex).name(), pex->what(), pszModule, pszThread);
    }
    else
    {
        // Handle unknown exceptions
        snprintf(pszMessage, sizeof(pszMessage),
                 "UNKNOWN EXCEPTION       \n%s in %s       \n",
                 pszModule, pszThread);
    }

    // Print the exception message to the console
    OutputDebugStringF("\n\n************************\n%s", pszMessage);

#ifdef wxTheApp
    // If using wxWidgets, display the error message in a message box
    wxMessageBox(pszMessage, "Error", wxOK | wxICON_ERROR);
#endif

    // Rethrow the current exception
    throw;
    // Optionally, you could use DebugBreak here if needed for debugging
    // DebugBreak();
}



void ParseString(const std::string& str, char delimiter, std::vector<std::string>& result)
{
    size_t start = 0;
    size_t end;

    do
    {
        // Find the position of the delimiter starting from `start`
        end = str.find(delimiter, start);

        // Extract the substring from `start` to `end`, and push it to the result vector
        result.push_back(str.substr(start, end - start));

        // Move `start` to the next character after the delimiter
        start = end + 1;

    } while (end != std::string::npos);  // Use std::string::npos to check if the delimiter was not found
}


std::string FormatMoney(int64_t n, bool fPlus)
{
    n /= CENT;  // Assuming CENT is a predefined constant

    // Format the money string with portable format specifiers
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%" PRId64 ".%02" PRId64, (n > 0 ? n : -n) / 100, (n > 0 ? n : -n) % 100);

    std::string str(buffer);

    // Insert commas for thousand separators
    for (size_t i = 6; i < str.size(); i += 4)
    {
        if (isdigit(str[str.size() - i - 1]))
            str.insert(str.size() - i, 1, ',');
    }

    // Handle negative or positive signs
    if (n < 0)
        str.insert(0, 1, '-');
    else if (fPlus && n > 0)
        str.insert(0, 1, '+');

    return str;
}



bool ParseMoney(const char* pszIn, int64_t& nRet)
{
    std::string strWhole;
    int64_t nCents = 0;
    const char* p = pszIn;

    // Skip leading spaces
    while (isspace(*p))
        p++;

    // Parse the whole part and look for commas
    for (; *p; p++)
    {
        if (*p == ',' && p > pszIn && isdigit(p[-1]) && isdigit(p[1]) && isdigit(p[2]) && isdigit(p[3]) && !isdigit(p[4]))
            continue;

        // Handle the decimal point and cents
        if (*p == '.')
        {
            p++;
            if (isdigit(*p))
            {
                nCents = 10 * (*p++ - '0');
                if (isdigit(*p))
                    nCents += (*p++ - '0');
            }
            break;
        }

        // Break if we encounter space or invalid characters
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;

        // Build the whole part of the number
        strWhole.push_back(*p);
    }

    // Skip trailing spaces and check if the input is valid
    for (; *p; p++)
        if (!isspace(*p))
            return false;

    // Check if the whole part is too large
    if (strWhole.size() > 14)
        return false;

    // Check if cents are valid
    if (nCents < 0 || nCents > 99)
        return false;

    // Convert the whole part of the string to a 64-bit integer
    int64_t nWhole;
    try {
        nWhole = std::stoll(strWhole);
    } catch (const std::invalid_argument& e) {
        return false;
    } catch (const std::out_of_range& e) {
        return false;
    }

    // Calculate the final value, including cents
    int64_t nPreValue = nWhole * 100 + nCents;
    int64_t nValue = nPreValue * CENT;

    // Check for overflow
    if (nValue / CENT != nPreValue)
        return false;
    if (nValue / COIN != nWhole)
        return false;

    // Set the result and return success
    nRet = nValue;
    return true;
}

bool FileExists(const char* psz)
{
    return std::filesystem::exists(psz);
}


std::int64_t GetFilesize(const std::string& filename)
{
    std::error_code ec;
    std::int64_t filesize = std::filesystem::file_size(filename, ec);
    if (ec) {
        return -1;  // Return -1 on error
    }
    return filesize;
}

// VERY WILD you need to improve these methods too modernize them.
// Undefine the 'max' macro after including Windows headers to prevent conflicts
#ifdef max
#undef max
#endif


uint64_t GetRand(uint64_t nMax)
{
    if (nMax == 0)
        return 0;

    // The range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
    uint64_t nRand = 0;

    // Generate random numbers until we find one within the valid range
    do
    {
        RAND_bytes(reinterpret_cast<unsigned char*>(&nRand), sizeof(nRand));
    }
    while (nRand >= nRange);

    return (nRand % nMax);
}


//
// "Never go to sea with two chronometers; take one or three."
// Our three chronometers are:
//  - System clock
//  - Median of other server's clocks
//  - NTP servers
//
// note: NTP isn't implemented yet, so until then we just use the median
//  of other nodes clocks to correct ours.
//
// System time function
int64_t GetTime()
{
    return std::time(nullptr);
}

static int64_t nTimeOffset = 0;

// Adjusted time function
int64_t GetAdjustedTime()
{
    return GetTime() + nTimeOffset;
}

// Add time data function
void AddTimeData(uint32_t ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - GetTime();

    // Ignore duplicates
    static std::set<uint32_t> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    static std::vector<int64_t> vTimeOffsets;
    if (vTimeOffsets.empty())
        vTimeOffsets.push_back(0);
    vTimeOffsets.push_back(nOffsetSample);

    OutputDebugStringF("Added time data, samples %zu, ip %08x, offset %+ " PRId64 " (%+" PRId64 " minutes)\n",
                vTimeOffsets.size(), ip, vTimeOffsets.back(), vTimeOffsets.back() / 60);

    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1)
    {
        std::sort(vTimeOffsets.begin(), vTimeOffsets.end());
        int64_t nMedian = vTimeOffsets[vTimeOffsets.size() / 2];
        nTimeOffset = nMedian;

        if (std::abs(nMedian) > 5 * 60)
        {
            // Only let other nodes change our clock so far before we
            // go to the NTP servers
            /// todo: Get time from NTP servers, then set a flag
            ///    to make sure it doesn't get changed again
        }

        // Range-based for loop to print offsets
        for (int64_t n : vTimeOffsets)
        {
            OutputDebugStringF("%+" PRId64 "  ", n);
        }

        OutputDebugStringF("|  nTimeOffset = %+ " PRId64 "  (%+ " PRId64 " minutes)\n", nTimeOffset, nTimeOffset / 60);
    }
}
