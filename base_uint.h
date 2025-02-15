// #ifndef BASE_UINT_H
// #define BASE_UINT_H

// #include <vector>
// #include <string>
// #include <cstring>
// #include <stdexcept>
// #include <sstream>
// #include <iostream>
// #include <cstdint>
// #include "serialize.h"

// template <unsigned int BITS>
// class base_uint {
// protected:
//     static constexpr int WIDTH = BITS / 32;  // Number of 32-bit parts
//     unsigned int pn[WIDTH];

// public:
//     // Default constructor - initializes to zero
//     base_uint() {
//         memset(pn, 0, sizeof(pn));
//     }

//     // Copy constructor from uint64
//     base_uint(uint64_t b) {
//         pn[0] = static_cast<unsigned int>(b);
//         pn[1] = static_cast<unsigned int>(b >> 32);
//         for (int i = 2; i < WIDTH; i++) {
//             pn[i] = 0;
//         }
//     }

//     // Assignment from uint64
//     base_uint& operator=(uint64_t b) {
//         pn[0] = static_cast<unsigned int>(b);
//         pn[1] = static_cast<unsigned int>(b >> 32);
//         for (int i = 2; i < WIDTH; i++) {
//             pn[i] = 0;
//         }
//         return *this;
//     }

//     // Bitwise NOT
//     const base_uint operator~() const {
//         base_uint ret;
//         for (int i = 0; i < WIDTH; i++) {
//             ret.pn[i] = ~pn[i];
//         }
//         return ret;
//     }

//     // Bitwise XOR with another base_uint
//     base_uint& operator^=(const base_uint& b) {
//         for (int i = 0; i < WIDTH; i++) {
//             pn[i] ^= b.pn[i];
//         }
//         return *this;
//     }

//     // Bitwise AND with another base_uint
//     base_uint& operator&=(const base_uint& b) {
//         for (int i = 0; i < WIDTH; i++) {
//             pn[i] &= b.pn[i];
//         }
//         return *this;
//     }

//     // Bitwise OR with another base_uint
//     base_uint& operator|=(const base_uint& b) {
//         for (int i = 0; i < WIDTH; i++) {
//             pn[i] |= b.pn[i];
//         }
//         return *this;
//     }

//     // Left shift operator
//     base_uint& operator<<=(unsigned int shift) {
//         base_uint a(*this);
//         memset(pn, 0, sizeof(pn));
//         int k = shift / 32;
//         shift = shift % 32;
//         for (int i = 0; i < WIDTH; i++) {
//             if (i + k + 1 < WIDTH && shift != 0) {
//                 pn[i + k + 1] |= (a.pn[i] >> (32 - shift));
//             }
//             if (i + k < WIDTH) {
//                 pn[i + k] |= (a.pn[i] << shift);
//             }
//         }
//         return *this;
//     }

//     // Right shift operator
//     base_uint& operator>>=(unsigned int shift) {
//         base_uint a(*this);
//         memset(pn, 0, sizeof(pn));
//         int k = shift / 32;
//         shift = shift % 32;
//         for (int i = 0; i < WIDTH; i++) {
//             if (i - k - 1 >= 0 && shift != 0) {
//                 pn[i - k - 1] |= (a.pn[i] << (32 - shift));
//             }
//             if (i - k >= 0) {
//                 pn[i - k] |= (a.pn[i] >> shift);
//             }
//         }
//         return *this;
//     }

//     // Addition with another base_uint
//     base_uint& operator+=(const base_uint& b) {
//         uint64_t carry = 0;
//         for (int i = 0; i < WIDTH; i++) {
//             uint64_t n = carry + pn[i] + b.pn[i];
//             pn[i] = static_cast<unsigned int>(n & 0xffffffff);
//             carry = n >> 32;
//         }
//         return *this;
//     }

//     // Prefix increment
//     base_uint& operator++() {
//         int i = 0;
//         while (++pn[i] == 0 && i < WIDTH - 1) {
//             i++;
//         }
//         return *this;
//     }

//     // Postfix increment
//     const base_uint operator++(int) {
//         base_uint ret = *this;
//         ++(*this);
//         return ret;
//     }

//     // Hexadecimal representation of the value
//     std::string GetHex() const {
//         std::stringstream ss;
//         ss << std::hex;
//         for (int i = WIDTH - 1; i >= 0; i--) {
//             ss << pn[i];
//         }
//         return ss.str();
//     }

//     // Set value from hex string
//     void SetHex(const std::string& str) {
//         memset(pn, 0, sizeof(pn));
//         std::string hex_str = str;
//         if (str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X") {
//             hex_str = str.substr(2);
//         }
//         int length = hex_str.length();
//         int byte_pos = 0;
//         for (int i = length - 1; i >= 0 && byte_pos < sizeof(pn); i -= 2, byte_pos++) {
//             std::string byte_str = hex_str.substr(std::max(0, i - 1), (i - 1 >= 0) ? 2 : 1);
//             unsigned int byte_value = std::stoul(byte_str, nullptr, 16);
//             reinterpret_cast<unsigned char*>(pn)[sizeof(pn) - byte_pos - 1] = static_cast<unsigned char>(byte_value);
//         }
//     }

//     // To string
//     std::string ToString() const {
//         return GetHex();
//     }

//     // Size in bytes
//     unsigned int size() const {
//         return sizeof(pn);
//     }

//     template<typename T>
//     unsigned int GetSerializeSize(const std::vector<T>& vch, int nType = 0, int nVersion = VERSION)
//     {
//         return vch.size() * sizeof(T);  // Return the size in bytes
//     }



//     // Serialization and deserialization (as an example)
//     template<typename Stream, typename T>
//     void Serialize(Stream& s, const std::vector<T>& vch, int nType = 0, int nVersion = VERSION)
//     {
//         // Serialize the size of the vector first
//         uint64_t vectorSize = vch.size();
//         s.write(reinterpret_cast<const char*>(&vectorSize), sizeof(vectorSize));

//         // Serialize the vector data
//         s.write(reinterpret_cast<const char*>(vch.data()), vectorSize * sizeof(T));
//     }


//     template<typename Stream, typename T>
//     void Unserialize(Stream& s, std::vector<T>& vch, int nType = 0, int nVersion = VERSION)
//     {
//         // Deserialize the size of the vector first
//         uint64_t vectorSize;
//         s.read(reinterpret_cast<char*>(&vectorSize), sizeof(vectorSize));

//         // Resize the vector based on the deserialized size
//         vch.resize(vectorSize);

//         // Deserialize the vector data
//         s.read(reinterpret_cast<char*>(vch.data()), vectorSize * sizeof(T));
//     }

// };

// #endif // BASE_UINT_H
