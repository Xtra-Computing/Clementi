#ifndef SINGLETHREAD_SSE_H_
#define SINGLETHREAD_SSE_H_

// https://stackoverflow.com/a/19519287

#include <x86intrin.h>

#include <cassert>
#include <cstddef>

// #include "singlethread_scalar.h"

inline __m128 ScanSSE(__m128 x) {
  x = _mm_add_ps(x, _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(x), 4)));
  x = _mm_add_ps(x, _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(x), 8)));
  return x;
}

// TODO: Only works for (input_len % 4 == 0).
void PrefixSumSSE(const size_t input_len, const float* input, float* output) {
  assert(input_len % 4 == 0);
  __m128 offset = _mm_setzero_ps();
  for (size_t i = 0; i < input_len; i += 4) {
    __m128 x = _mm_load_ps(&input[i]);
    __m128 out = ScanSSE(x);
    out = _mm_add_ps(out, offset);
    _mm_store_ps(&output[i], out);
    offset = _mm_shuffle_ps(out, out, _MM_SHUFFLE(3, 3, 3, 3));
  }
}

// TODO: Generic template doesn't work.
template <typename Type>
void PrefixSumSSE(const size_t data_len, Type* data) {
  // PrefixSum(data_len, data);  // Use scalar code instead.
  assert(false);
}

// Specialization for floats.
template <>
void PrefixSumSSE<float>(const size_t data_len, float* data) {
  PrefixSumSSE(data_len, data, data);
}

#endif  // SINGLETHREAD_SSE_H_