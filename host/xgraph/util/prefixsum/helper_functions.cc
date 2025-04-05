#include "helper_functions.h"

#include <x86intrin.h>

#include <cassert>
#include <cstddef>

__m512 PrefixSumAVX512VerticalHelper(const int chunk_len, const float* input,
                                     float* output,
                                     const __m512 input_offsets) {
  __m512 sums{input_offsets};
  __m512i idx{_mm512_mullo_epi32(
      _mm512_set1_epi32(chunk_len),
      _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))};
  const __m512i idx_inc{_mm512_set1_epi32(1)};
  for (int i{0}; i < chunk_len; ++i) {
    const __m512 in{_mm512_i32gather_ps(idx, input, sizeof(float))};
    sums = _mm512_add_ps(sums, in);
    _mm512_i32scatter_ps(output, idx, sums, sizeof(float));
    idx = _mm512_add_epi32(idx, idx_inc);
  }
  return sums;
}

float Accumulate(const size_t data_len, const float* data) {
  float sum{0};
  for (size_t i{0}; i < data_len; ++i) sum += data[i];
  return sum;
}

float AccumulateSIMD(const size_t data_len, const float* data) {
  assert(data_len % 16 == 0);
  __m512 sum{_mm512_setzero_ps()};
  for (size_t i{0}; i < data_len; i += 16) {
    sum = _mm512_add_ps(sum, _mm512_load_ps(&data[i]));
  }
  return _mm512_reduce_add_ps(sum);
}

void Increment(const float inc, const size_t data_len, float* data) {
  for (size_t i{0}; i < data_len; ++i) data[i] += inc;
}

void IncrementSIMD(const float inc, const size_t data_len, float* data) {
  assert(data_len % 16 == 0);
  __m512 offset{_mm512_set1_ps(inc)};
  for (size_t i{0}; i < data_len; i += 16) {
    const __m512 in{_mm512_load_ps(&data[i])};
    const __m512 out{_mm512_add_ps(in, offset)};
    _mm512_store_ps(&data[i], out);
  }
}