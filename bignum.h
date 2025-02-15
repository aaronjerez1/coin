#ifndef BIGNUM_H
#define BIGNUM_H

// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

\
class bignum_error : public std::runtime_error
{
public:
    explicit bignum_error(const std::string& str) : std::runtime_error(str) {}
};

class CAutoBN_CTX
{
protected:
    BN_CTX* pctx;
    BN_CTX* operator=(BN_CTX* pnew) { return pctx = pnew; }

public:
    CAutoBN_CTX()
    {
        pctx = BN_CTX_new();
        if (pctx == NULL)
            throw bignum_error("CAutoBN_CTX : BN_CTX_new() returned NULL");
    }

    ~CAutoBN_CTX()
    {
        if (pctx != NULL)
            BN_CTX_free(pctx);
    }

    operator BN_CTX*() { return pctx; }
    BN_CTX& operator*() { return *pctx; }
    BN_CTX** operator&() { return &pctx; }
    bool operator!() { return (pctx == NULL); }
};

class CBigNum
{
private:
    BIGNUM* bn;

public:
    CBigNum()
    {
        bn = BN_new();
        if (!bn)
            throw bignum_error("CBigNum::CBigNum : BN_new failed");
    }

    CBigNum(const CBigNum& b)
    {
        bn = BN_new();
        if (!bn || !BN_copy(bn, b.bn))
        {
            BN_free(bn);
            throw bignum_error("CBigNum::CBigNum(const CBigNum&) : BN_copy failed");
        }
    }

    explicit CBigNum(const std::string& str)
    {
        bn = BN_new();
        if (!bn)
            throw bignum_error("CBigNum::CBigNum(string) : BN_new failed");
        SetHex(str);
    }

    ~CBigNum()
    {
        BN_free(bn);
    }

    CBigNum& operator=(const CBigNum& b)
    {
        if (!BN_copy(bn, b.bn))
            throw bignum_error("CBigNum::operator= : BN_copy failed");
        return *this;
    }

    CBigNum(char n)             { bn = BN_new(); if (n >= 0) setulong(n); else setint64(n); }
    CBigNum(short n)            { bn = BN_new(); if (n >= 0) setulong(n); else setint64(n); }
    CBigNum(int n)              { bn = BN_new(); if (n >= 0) setulong(n); else setint64(n); }
    CBigNum(long n)             { bn = BN_new(); if (n >= 0) setulong(n); else setint64(n); }
    CBigNum(int64_t n)          { bn = BN_new(); setint64(n); }
    CBigNum(unsigned char n)    { bn = BN_new(); setulong(n); }
    CBigNum(unsigned short n)   { bn = BN_new(); setulong(n); }
    CBigNum(unsigned int n)     { bn = BN_new(); setulong(n); }
    CBigNum(unsigned long n)    { bn = BN_new(); setulong(n); }
    CBigNum(uint64_t n)         { bn = BN_new(); setuint64(n); }
    explicit CBigNum(uint256 n) { bn = BN_new(); setuint256(n); }

    explicit CBigNum(const std::vector<unsigned char>& vch)
    {
        bn = BN_new();
        setvch(vch);
    }

    void setulong(unsigned long n)
    {
        if (!BN_set_word(bn, n))
            throw bignum_error("CBigNum conversion from unsigned long : BN_set_word failed");
    }

    unsigned long getulong() const
    {
        return BN_get_word(bn);
    }

    unsigned int getuint() const
    {
        return BN_get_word(bn);
    }

    BIGNUM* getBN() const {
        return bn;
    }

    int getint() const
    {
        unsigned long n = BN_get_word(bn);
        if (!BN_is_negative(bn))
            return (n > INT_MAX ? INT_MAX : n);
        else
            return (n > INT_MAX ? INT_MIN : -(int)n);
    }

    void setint64(int64_t n)
    {
        unsigned char pch[sizeof(n) + 6];
        unsigned char* p = pch + 4;
        bool fNegative = false;
        if (n < 0)
        {
            n = -n;
            fNegative = true;
        }
        bool fLeadingZeroes = true;
        for (int i = 0; i < 8; i++)
        {
            unsigned char c = (n >> 56) & 0xff;
            n <<= 8;
            if (fLeadingZeroes)
            {
                if (c == 0)
                    continue;
                if (c & 0x80)
                    *p++ = (fNegative ? 0x80 : 0);
                else if (fNegative)
                    c |= 0x80;
                fLeadingZeroes = false;
            }
            *p++ = c;
        }
        unsigned int nSize = p - (pch + 4);
        pch[0] = (nSize >> 24) & 0xff;
        pch[1] = (nSize >> 16) & 0xff;
        pch[2] = (nSize >> 8) & 0xff;
        pch[3] = (nSize) & 0xff;
        BN_mpi2bn(pch, p - pch, bn);
    }

    void setuint64(uint64_t n)
    {
        unsigned char pch[sizeof(n) + 6];
        unsigned char* p = pch + 4;
        bool fLeadingZeroes = true;
        for (int i = 0; i < 8; i++)
        {
            unsigned char c = (n >> 56) & 0xff;
            n <<= 8;
            if (fLeadingZeroes)
            {
                if (c == 0)
                    continue;
                if (c & 0x80)
                    *p++ = 0;
                fLeadingZeroes = false;
            }
            *p++ = c;
        }
        unsigned int nSize = p - (pch + 4);
        pch[0] = (nSize >> 24) & 0xff;
        pch[1] = (nSize >> 16) & 0xff;
        pch[2] = (nSize >> 8) & 0xff;
        pch[3] = (nSize) & 0xff;
        BN_mpi2bn(pch, p - pch, bn);
    }

    void setuint256(const uint256& n)
    {
        // Prepare a byte array (MPI representation) with extra space for the size header
        unsigned char pch[sizeof(n) + 6];
        unsigned char* p = pch + 4;  // Skip the first 4 bytes (reserved for size header)

        bool fLeadingZeroes = true;
        const unsigned char* pbegin = (const unsigned char*)&n;
        const unsigned char* psrc = pbegin + sizeof(n);

        // Loop through the bytes of n (in reverse order) to strip leading zeroes
        while (psrc != pbegin)
        {
            unsigned char c = *(--psrc);
            if (fLeadingZeroes)
            {
                if (c == 0)  // Skip leading zeroes
                    continue;
                if (c & 0x80)  // If the highest bit is set, add an extra byte
                    *p++ = 0;
                fLeadingZeroes = false;
            }
            *p++ = c;
        }

        // Compute the size of the number and store it in the first 4 bytes of pch
        unsigned int nSize = p - (pch + 4);
        pch[0] = (nSize >> 24) & 0xff;
        pch[1] = (nSize >> 16) & 0xff;
        pch[2] = (nSize >> 8) & 0xff;
        pch[3] = (nSize >> 0) & 0xff;

        // Convert the byte array (MPI format) to a BIGNUM and store it in 'bn'
        BN_mpi2bn(pch, p - pch, bn);
    }

    uint256 getuint256() const
    {
        // Get the size of the BIGNUM in MPI format
        unsigned int nSize = BN_bn2mpi(bn, NULL);
        if (nSize < 4)
            return 0;  // If the size is less than 4, return 0

        // Allocate a vector to hold the MPI representation
        std::vector<unsigned char> vch(nSize);

        // Convert the BIGNUM to MPI format and store it in the vector
        BN_bn2mpi(bn, &vch[0]);

        // Ensure the highest bit is cleared
        if (vch.size() > 4)
            vch[4] &= 0x7f;

        // Convert the vector into a uint256
        uint256 n = 0;
        for (int i = 0, j = vch.size() - 1; i < sizeof(n) && j >= 4; i++, j--)
            ((unsigned char*)&n)[i] = vch[j];

        return n;
    }


    void setvch(const std::vector<unsigned char>& vch)
    {
        std::vector<unsigned char> vch2(vch.size() + 4);
        unsigned int nSize = vch.size();
        vch2[0] = (nSize >> 24) & 0xff;
        vch2[1] = (nSize >> 16) & 0xff;
        vch2[2] = (nSize >> 8) & 0xff;
        vch2[3] = (nSize) & 0xff;
        std::reverse_copy(vch.begin(), vch.end(), vch2.begin() + 4);
        BN_mpi2bn(&vch2[0], vch2.size(), bn);
    }

    std::vector<unsigned char> getvch() const
    {
        unsigned int nSize = BN_bn2mpi(bn, NULL);
        if (nSize < 4)
            return std::vector<unsigned char>();
        std::vector<unsigned char> vch(nSize);
        BN_bn2mpi(bn, &vch[0]);
        vch.erase(vch.begin(), vch.begin() + 4);
        std::reverse(vch.begin(), vch.end());
        return vch;
    }

    CBigNum& SetCompact(unsigned int nCompact)
    {
        // Extract the size from the compact representation
        unsigned int nSize = nCompact >> 24;

        // Create a vector to store the binary representation
        std::vector<unsigned char> vch(4 + nSize);
        vch[0] = (nSize >> 24) & 0xff;
        vch[1] = (nSize >> 16) & 0xff;
        vch[2] = (nSize >> 8) & 0xff;
        vch[3] = (nSize) & 0xff;

        // Fill the rest of the vector with the nCompact data
        if (nSize >= 1) vch[4] = (nCompact >> 16) & 0xff;
        if (nSize >= 2) vch[5] = (nCompact >> 8) & 0xff;
        if (nSize >= 3) vch[6] = (nCompact >> 0) & 0xff;

        // Convert the byte vector to a BIGNUM (use bn instead of this)
        BN_mpi2bn(&vch[0], vch.size(), bn);  // 'bn' is your BIGNUM member variable, not 'this'

        return *this;
    }

    unsigned int GetCompact() const
    {
        unsigned int nSize = BN_bn2mpi(bn, NULL);
        std::vector<unsigned char> vch(nSize);
        nSize -= 4;
        BN_bn2mpi(bn, &vch[0]);
        unsigned int nCompact = nSize << 24;
        if (nSize >= 1) nCompact |= (vch[4] << 16);
        if (nSize >= 2) nCompact |= (vch[5] << 8);
        if (nSize >= 3) nCompact |= (vch[6] << 0);
        return nCompact;
    }


    void SetHex(const std::string& str)
    {
        // skip 0x
        const char* psz = str.c_str();
        while (isspace(*psz))
            psz++;
        bool fNegative = false;
        if (*psz == '-')
        {
            fNegative = true;
            psz++;
        }
        if (psz[0] == '0' && tolower(psz[1]) == 'x')
            psz += 2;
        while (isspace(*psz))
            psz++;

        // hex string to bignum
        static char phexdigit[256] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0, 0,0xa,0xb,0xc,0xd,0xe,0xf,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0xa,0xb,0xc,0xd,0xe,0xf,0,0,0,0,0,0,0,0,0 };
        *this = 0;
        while (isxdigit(*psz))
        {
            *this <<= 4;
            int n = phexdigit[*psz++];
            *this += n;
        }
        if (fNegative)
            *this = 0 - *this;
    }
    ///////
    /// \brief GetSerializeSize
    /// \param nType
    /// \param nVersion
    /// \return
    //// VERY IMPORTANT TO LOOK AT AND ITERATE
    unsigned int GetSerializeSize(int nType = 0, int nVersion = VERSION) const
    {
        return ::GetSerializeSize(getvch(), nType, nVersion);  // Use the vector returned by getvch()
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType = 0, int nVersion = VERSION) const
    {
        ::Serialize(s, getvch(), nType, nVersion);  // Serialize the vector returned by getvch()
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType = 0, int nVersion = VERSION)
    {
        std::vector<unsigned char> vch;
        ::Unserialize(s, vch, nType, nVersion);  // Deserialize into a vector
        setvch(vch);  // Set the vector back into the CBigNum
    }

////////////
    bool operator!() const
    {
        return BN_is_zero(bn);
    }

    CBigNum& operator+=(const CBigNum& b)
    {
        if (!BN_add(bn, bn, b.bn))
            throw bignum_error("CBigNum::operator+= : BN_add failed");
        return *this;
    }
    CBigNum& operator-=(const CBigNum& b)
    {
        *this = *this - b;  // Use the already defined subtraction operator
        return *this;
    }


    CBigNum& operator*=(const CBigNum& b)
    {
        CAutoBN_CTX pctx;
        if (!BN_mul(bn, bn, b.bn, pctx))
            throw bignum_error("CBigNum::operator*= : BN_mul failed");
        return *this;
    }

    CBigNum& operator/=(const CBigNum& b)
    {
        *this = *this / b;  // Use the already defined division operator
        return *this;
    }

    CBigNum& operator%=(const CBigNum& b)
    {
        *this = *this % b;  // Use the already defined modulo operator
        return *this;
    }

    CBigNum& operator<<=(unsigned int shift)
    {
        if (!BN_lshift(bn, bn, shift))  // Shift left the internal BIGNUM*
            throw bignum_error("CBigNum:operator<<= : BN_lshift failed");
        return *this;
    }
    CBigNum& operator>>=(unsigned int shift)
    {
        if (!BN_rshift(bn, bn, shift))  // Shift right the internal BIGNUM*
            throw bignum_error("CBigNum:operator>>= : BN_rshift failed");
        return *this;
    }
    CBigNum& operator++()
    {
        if (!BN_add(bn, bn, BN_value_one()))  // Increment the internal BIGNUM*
            throw bignum_error("CBigNum::operator++ : BN_add failed");
        return *this;
    }
    const CBigNum operator++(int)
    {
        const CBigNum ret = *this;  // Save the original value for return
        ++(*this);  // Call the prefix increment operator
        return ret;
    }
    CBigNum& operator--()
    {
        CBigNum r;
        if (!BN_sub(r.bn, bn, BN_value_one()))  // Subtract 1 from the internal BIGNUM*
            throw bignum_error("CBigNum::operator-- : BN_sub failed");
        *this = r;  // Assign the result back
        return *this;
    }
    const CBigNum operator--(int)
    {
        const CBigNum ret = *this;  // Save the original value for return
        --(*this);  // Call the prefix decrement operator
        return ret;
    }


    friend inline const CBigNum operator+(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator-(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator-(const CBigNum& a);
    friend inline const CBigNum operator/(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator%(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator*(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator<<(const CBigNum& a, unsigned int shift);
    friend inline const CBigNum operator>>(const CBigNum& a, unsigned int shift);
    friend inline bool operator==(const CBigNum& a, const CBigNum& b);
    friend inline bool operator!=(const CBigNum& a, const CBigNum& b);
    friend inline bool operator<=(const CBigNum& a, const CBigNum& b);
    friend inline bool operator>=(const CBigNum& a, const CBigNum& b);
    friend inline bool operator<(const CBigNum& a, const CBigNum& b);
    friend inline bool operator>(const CBigNum& a, const CBigNum& b);
};
inline const CBigNum operator+(const CBigNum& a, const CBigNum& b)
{
    CBigNum r;  // Create a result object
    if (!BN_add(r.bn, a.bn, b.bn))  // Use the internal BIGNUM* members
        throw bignum_error("CBigNum::operator+ : BN_add failed");
    return r;
}

inline const CBigNum operator-(const CBigNum& a, const CBigNum& b)
{
    CBigNum r;
    if (!BN_sub(r.bn, a.bn, b.bn))
        throw bignum_error("CBigNum::operator- : BN_sub failed");
    return r;
}
inline const CBigNum operator-(const CBigNum& a)
{
    CBigNum r(a);  // Copy the input
    BN_set_negative(r.bn, !BN_is_negative(r.bn));  // Set negation
    return r;
}

 inline const CBigNum operator/(const CBigNum& a, const CBigNum& b)
{
    CAutoBN_CTX pctx;
    CBigNum r;
    if (!BN_div(r.bn, NULL, a.bn, b.bn, pctx))
        throw bignum_error("CBigNum::operator/ : BN_div failed");
    return r;
}

 inline const CBigNum operator%(const CBigNum& a, const CBigNum& b)
{
    CAutoBN_CTX pctx;
    CBigNum r;
    if (!BN_mod(r.bn, a.bn, b.bn, pctx))
        throw bignum_error("CBigNum::operator% : BN_mod failed");
    return r;
}


inline const CBigNum operator*(const CBigNum& a, const CBigNum& b)
{
    CAutoBN_CTX pctx;
    CBigNum r;
    if (!BN_mul(r.bn, a.bn, b.bn, pctx))
        throw bignum_error("CBigNum::operator* : BN_mul failed");
    return r;
}

inline const CBigNum operator<<(const CBigNum& a, unsigned int shift)
{
    CBigNum r;
    if (!BN_lshift(r.bn, a.bn, shift))
        throw bignum_error("CBigNum:operator<< : BN_lshift failed");
    return r;
}

inline const CBigNum operator>>(const CBigNum& a, unsigned int shift)
{
    CBigNum r;
    if (!BN_rshift(r.bn, a.bn, shift))
        throw bignum_error("CBigNum:operator>> : BN_rshift failed");
    return r;
}

inline bool operator==(const CBigNum& a, const CBigNum& b)
{
    return (BN_cmp(a.bn, b.bn) == 0);
}
//inline bool operator==(const CBigNum& a, const CBigNum& b) { return (BN_cmp(a.bn, b.bn) == 0); }
inline bool operator!=(const CBigNum& a, const CBigNum& b) { return (BN_cmp(a.bn, b.bn) != 0); }
inline bool operator<=(const CBigNum& a, const CBigNum& b) { return (BN_cmp(a.bn, b.bn) <= 0); }
inline bool operator>=(const CBigNum& a, const CBigNum& b) { return (BN_cmp(a.bn, b.bn) >= 0); }
inline bool operator<(const CBigNum& a, const CBigNum& b)  { return (BN_cmp(a.bn, b.bn) < 0); }
inline bool operator>(const CBigNum& a, const CBigNum& b)  { return (BN_cmp(a.bn, b.bn) > 0); }

#endif // BIGNUM_H
