#ifndef SINGLETHREAD_AVX_H_
#define SINGLETHREAD_AVX_H_

// https://stackoverflow.com/a/19519287

#include <x86intrin.h>

#include <cassert>
#include <cstddef>

// #include "singlethread_scalar.h"

inline __m256 ScanAVX(__m256 x) {
  __m256 t0, t1;
  // shift1_AVX + add
  t0 = _mm256_permute_ps(x, _MM_SHUFFLE(2, 1, 0, 3));
  t1 = _mm256_permute2f128_ps(t0, t0, 41);
  x = _mm256_add_ps(x, _mm256_blend_ps(t0, t1, 0x11));
  // shift2_AVX + add
  t0 = _mm256_permute_ps(x, _MM_SHUFFLE(1, 0, 3, 2));
  t1 = _mm256_permute2f128_ps(t0, t0, 41);
  x = _mm256_add_ps(x, _mm256_blend_ps(t0, t1, 0x33));
  // shift3_AVX + add
  x = _mm256_add_ps(x, _mm256_permute2f128_ps(x, x, 41));
  return x;
}

// TODO: Only works for (input_len % 8 == 0);
void PrefixSumAVX(const size_t input_len, const float* input, float* output) {
  assert(input_len % 8 == 0);
  __m256 offset = _mm256_setzero_ps();
  for (size_t i = 0; i < input_len; i += 8) {
    __m256 x = _mm256_load_ps(&input[i]);
    __m256 out = ScanAVX(x);
    out = _mm256_add_ps(out, offset);
    _mm256_store_ps(&output[i], out);
    // broadcast last element
    __m256 t0 = _mm256_permute2f128_ps(out, out, 0x11);
    offset = _mm256_permute_ps(t0, 0xff);
  }
}

// TODO: Generic template doesn't work.
template <typename Type>
void PrefixSumAVX(const size_t data_len, Type* data) {
  // PrefixSum(data_len, data);  // Use scalar code instead.
  assert(false);
}

// Specialization for floats.
template <>
void PrefixSumAVX<float>(const size_t data_len, float* data) {
  PrefixSumAVX(data_len, data, data);
}

#endif  // SINGLETHREAD_AVX_H_
