#include <cstring>
#include <cstdint>

void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {
        0x67452301,
        0xEFCDAB89,
        0x98BADCFE,
        0x10325476,
        0xC3D2E1F0
    };

    size_t total_len = len + 1 + 8;
    size_t padded_len = ((total_len + 63) / 64) * 64;
    uint8_t* padded = new uint8_t[padded_len]();
    memcpy(padded, data, len);
    padded[len] = 0x80;

    uint64_t bit_len = len * 8;
    for (int i = 0; i < 8; ++i)
        padded[padded_len - 1 - i] = (uint8_t)(bit_len >> (8 * i));

    for (size_t offset = 0; offset < padded_len; offset += 64) {
        uint32_t w[80] = {};
        for (int i = 0; i < 16; ++i)
            w[i] = (padded[offset + i * 4 + 0] << 24) |
            (padded[offset + i * 4 + 1] << 16) |
            (padded[offset + i * 4 + 2] << 8) |
            (padded[offset + i * 4 + 3]);

        for (int i = 16; i < 80; ++i) {
            uint32_t temp = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (temp << 1) | (temp >> 31);
        }

        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            }
            else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    delete[] padded;

    for (int i = 0; i < 5; ++i) {
        out[i * 4 + 0] = (uint8_t)(h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(h[i]);
    }
}
