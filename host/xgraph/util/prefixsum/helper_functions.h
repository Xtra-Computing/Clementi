#ifndef HELPER_FUNCTIONS_H_
#define HELPER_FUNCTIONS_H_

#include <x86intrin.h>

#include <cstddef>

inline __m512i _mm512_slli_si512(const __m512i x, const int imm8) {
  return _mm512_alignr_epi32(x, _mm512_setzero_si512(), 16 - imm8);
}

inline __m512 Scan(__m512 x) {
  x = _mm512_add_ps(
      x, _mm512_castsi512_ps(_mm512_slli_si512(_mm512_castps_si512(x), 1)));
  x = _mm512_add_ps(
      x, _mm512_castsi512_ps(_mm512_slli_si512(_mm512_castps_si512(x), 2)));
  x = _mm512_add_ps(
      x, _mm512_castsi512_ps(_mm512_slli_si512(_mm512_castps_si512(x), 4)));
  x = _mm512_add_ps(
      x, _mm512_castsi512_ps(_mm512_slli_si512(_mm512_castps_si512(x), 8)));
  return x;
}

// Compute prefix sums in 16 chunks.
__m512 PrefixSumAVX512VerticalHelper(
    const int chunk_len, const float* input, float* output,
    const __m512 input_offsets = _mm512_setzero_ps());

float Accumulate(const size_t data_len, const float* data);
float AccumulateSIMD(const size_t data_len, const float* data);

void Increment(const float inc, const size_t data_len, float* data);
void IncrementSIMD(const float inc, const size_t data_len, float* data);

#endif  // HELPER_FUNCTIONS_H_