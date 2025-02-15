#ifndef BASE58_H
#define BASE58_H
// Modernized Base58 Encoding/Decoding


static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// EncodeBase58
inline std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend)
{
    BN_CTX* pctx = BN_CTX_new();
    BIGNUM* bn58 = BN_new();
    BIGNUM* bn0 = BN_new();
    BIGNUM* bn = BN_new();
    BIGNUM* dv = BN_new();
    BIGNUM* rem = BN_new();

    BN_set_word(bn58, 58);
    BN_set_word(bn0, 0);

    // Convert big endian data to little endian
    std::vector<unsigned char> vchTmp(pend - pbegin + 1, 0);
    std::reverse_copy(pbegin, pend, vchTmp.begin());

    // Convert little endian data to bignum
    BN_bin2bn(vchTmp.data(), vchTmp.size(), bn);

    // Convert bignum to string
    std::string str;
    str.reserve((pend - pbegin) * 138 / 100 + 1);
    while (BN_cmp(bn, bn0) > 0)
    {
        if (!BN_div(dv, rem, bn, bn58, pctx))
            throw std::runtime_error("EncodeBase58 : BN_div failed");
        BN_copy(bn, dv);
        unsigned int c = BN_get_word(rem);
        str += pszBase58[c];
    }

    // Leading zeroes encoded as base58 zeros
    for (const unsigned char* p = pbegin; p < pend && *p == 0; p++)
        str += pszBase58[0];

    // Convert little endian string to big endian
    std::reverse(str.begin(), str.end());

    // Clean up
    BN_free(bn);
    BN_free(bn58);
    BN_free(bn0);
    BN_free(dv);
    BN_free(rem);
    BN_CTX_free(pctx);

    return str;
}

inline std::string EncodeBase58(const std::vector<unsigned char>& vch)
{
    return EncodeBase58(vch.data(), vch.data() + vch.size());
}

// DecodeBase58
inline bool DecodeBase58(const char* psz, std::vector<unsigned char>& vchRet)
{
    BN_CTX* pctx = BN_CTX_new();
    BIGNUM* bn58 = BN_new();
    BIGNUM* bn = BN_new();
    BIGNUM* bnChar = BN_new();

    BN_set_word(bn58, 58);
    BN_set_word(bn, 0);

    while (isspace(*psz))
        psz++;

    // Convert big endian string to bignum
    for (const char* p = psz; *p; p++)
    {
        const char* p1 = strchr(pszBase58, *p);
        if (p1 == NULL)
        {
            while (isspace(*p))
                p++;
            if (*p != '\0')
                return false;
            break;
        }
        BN_set_word(bnChar, p1 - pszBase58);
        if (!BN_mul(bn, bn, bn58, pctx))
            throw std::runtime_error("DecodeBase58 : BN_mul failed");
        BN_add(bn, bn, bnChar);
    }

    // Get bignum as little endian data
    std::vector<unsigned char> vchTmp(BN_num_bytes(bn));
    BN_bn2bin(bn, vchTmp.data());

    // Trim off sign byte if present
    if (vchTmp.size() >= 2 && vchTmp.back() == 0 && vchTmp[vchTmp.size() - 2] >= 0x80)
        vchTmp.pop_back();

    // Restore leading zeros
    int nLeadingZeros = 0;
    for (const char* p = psz; *p == pszBase58[0]; p++)
        nLeadingZeros++;
    vchRet.assign(nLeadingZeros + vchTmp.size(), 0);

    // Convert little endian data to big endian
    std::reverse_copy(vchTmp.begin(), vchTmp.end(), vchRet.end() - vchTmp.size());

    // Clean up
    BN_free(bn);
    BN_free(bn58);
    BN_free(bnChar);
    BN_CTX_free(pctx);

    return true;
}

inline bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58(str.c_str(), vchRet);
}


// EncodeBase58Check
inline std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn)
{
    // Add 4-byte hash check to the end
    std::vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
    return EncodeBase58(vch);
}

// DecodeBase58Check
inline bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet)
{
    if (!DecodeBase58(psz, vchRet))
        return false;
    if (vchRet.size() < 4)
    {
        vchRet.clear();
        return false;
    }
    uint256 hash = Hash(vchRet.begin(), vchRet.end() - 4);
    if (memcmp(&hash, &vchRet.end()[-4], 4) != 0)
    {
        vchRet.clear();
        return false;
    }
    vchRet.resize(vchRet.size() - 4);
    return true;
}

inline bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58Check(str.c_str(), vchRet);
}

// Address Encoding/Decoding
static const unsigned char ADDRESSVERSION = 0;

inline std::string Hash160ToAddress(uint160 hash160)
{
    std::vector<unsigned char> vch(1, ADDRESSVERSION);
    vch.insert(vch.end(), UBEGIN(hash160), UEND(hash160));
    return EncodeBase58Check(vch);
}

inline bool AddressToHash160(const char* psz, uint160& hash160Ret)
{
    std::vector<unsigned char> vch;
    if (!DecodeBase58Check(psz, vch))
        return false;
    if (vch.empty() || vch.size() != sizeof(hash160Ret) + 1)
        return false;
    memcpy(&hash160Ret, &vch[1], sizeof(hash160Ret));
    return vch[0] == ADDRESSVERSION;
}

inline bool AddressToHash160(const std::string& str, uint160& hash160Ret)
{
    return AddressToHash160(str.c_str(), hash160Ret);
}

inline bool IsValidCoinAddress(const char* psz)
{
    uint160 hash160;
    return AddressToHash160(psz, hash160);
}

inline bool IsValidCoinAddress(const std::string& str)
{
    return IsValidCoinAddress(str.c_str());
}

inline std::string PubKeyToAddress(const std::vector<unsigned char>& vchPubKey)
{
    return Hash160ToAddress(Hash160(vchPubKey));
}

#endif // BASE58_H
