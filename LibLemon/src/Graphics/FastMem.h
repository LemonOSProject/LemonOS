#pragma once

#include <smmintrin.h>
#include <stddef.h>
#include <stdint.h>

extern "C" void memcpy_sse2(void* dest, void* src, size_t count);
extern "C" void memcpy_sse2_unaligned(void* dest, void* src, size_t count);
extern "C" void memset32_sse2(void* dest, uint32_t c, uint64_t count);
extern "C" void memset64_sse2(void* dest, uint64_t c, uint64_t count);

inline void memset32_optimized(void* _dest, uint32_t c, size_t count) {
    uint32_t* dest = reinterpret_cast<uint32_t*>(_dest);
    while (count--) {
        *(dest++) = c;
    }
    return;
}

inline void memset64_optimized(void* _dest, uint64_t c, size_t count) {
    uint64_t* dest = reinterpret_cast<uint64_t*>(_dest);
    if (((size_t)dest & 0x7)) {
        while (count--) {
            *(dest++) = c;
        }
        return;
    }

    size_t overflow = (count & 0x7);          // Amount of overflow bytes
    size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    memset64_sse2(dest, c, size_aligned >> 3);

    while (overflow--) {
        *(dest++) = c;
    }
}

// Alpha blending:
// a0 = aa + ab(255 - aa)
// c0 = (ca * aa + cb * ab(255 - aa)) / a0

inline void alphablend_optimized(uint32_t* dest, uint32_t* src, size_t count) {
    // Each entry corresponds to the byte index to fill in the destination
    // Use this for the alpha values
    /*static const __m128i alphaShuffle = _mm_set_epi8(15, 15, 15, 15, 11, 11, 11, 11, 7, 7, 7, 7, 3, 3, 3, 3);
    static const __m128i vector00ff = _mm_set1_epi16(0x00ff);
    static const __m128i vectorffff = _mm_set1_epi32(0xffffffff);
    //static const __m128i srcDiscardAlpha = _mm_set_epi32(0x0000ffff, 0xffffffff, 0x0000ffff, 0xffffffff);

    __m128i alpha;
    __m128i alphaHigh;
    __m128i alphaLow;

    __m128i _dest;
    __m128i destLow;
    __m128i destHigh;

    __m128i _src;
    __m128i srcLow;
    __m128i srcHigh;

    // TODO: Handle aligned memory access
    // TODO: Overflow issues

    // Steps for alpha blending:
    // For each pixel, separate the color channels into the folowing
    // 0x00AA00BB00GG00RR
    // This allows us to discard any overflow
    size_t count128 = count >> 2;
    count &= 0x3;
    for (; count128; count128--, dest += 4, src += 4) {
        // TODO: We shouldnt need to retain the data in the cache so use a non-temporal store and load

        // Shift each 32-bit integer right by 24 and pad with 0s to get alpha values
        _src = _mm_loadu_si128(reinterpret_cast<__m128i_u*>(src));

        // Get alpha in format 0xAAAAAAAA for each pixel
        alpha = _mm_shuffle_epi8(_src, alphaShuffle);
        if (_mm_test_all_zeros(alpha, vectorffff)) {
            // No alpha blending to be done, alpha channels are all 0, dest doesen't change
            continue;
        } else if(_mm_test_all_ones(alpha)) {
            // No alpha blending to be done, alpha channels are full,
            // src does not change
            _mm_storeu_si128(reinterpret_cast<__m128i_u*>(dest), _src);
            continue;
        }

        _dest = _mm_loadu_si128(reinterpret_cast<__m128i_u*>(dest));

        // Expand to 0x00AA00AA00AA00AA for each pixel...
        alphaLow = _mm_unpacklo_epi8(alpha, _mm_setzero_si128());
        alphaHigh = _mm_unpackhi_epi8(alpha, _mm_setzero_si128());

        // Expand to 0x00AA00BB00GG00RR for each pixel...
        destLow = _mm_unpacklo_epi8(_dest, _mm_setzero_si128());
        destHigh = _mm_unpackhi_epi8(_dest, _mm_setzero_si128());

        srcLow = _mm_unpacklo_epi8(_src, _mm_setzero_si128());
        srcHigh = _mm_unpackhi_epi8(_src, _mm_setzero_si128());

        // XORing the alpha will have the equivalent of (256 - alpha) for each byte
        srcLow = _mm_srli_epi16(_mm_mullo_epi16(srcLow, alphaLow), 8);
        srcHigh = _mm_srli_epi16(_mm_mullo_epi16(srcHigh, alphaHigh), 8);
        destLow = _mm_srli_epi16(_mm_mullo_epi16(destLow, _mm_xor_si128(alphaLow, vector00ff)), 8);
        destHigh = _mm_srli_epi16(_mm_mullo_epi16(destHigh, _mm_xor_si128(alphaHigh, vector00ff)), 8);

        _dest =  _mm_packus_epi16(_mm_add_epi16(destLow, srcLow), _mm_add_epi16(destHigh, srcHigh));
        
        // Repack dest and store it
        _mm_storeu_si128(reinterpret_cast<__m128i_u*>(dest), _dest);
    }*/

    // Compiler must be performing black magic because SSE version
    // isn't actually that much faster
    for(; count; count--, dest++, src++){
        *dest = Lemon::Graphics::AlphaBlendInt(*dest, *src);
    }
}

extern "C" void memcpy_optimized(void* dest, void* src, size_t count);