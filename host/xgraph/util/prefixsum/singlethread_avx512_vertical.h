#ifndef SINGLETHREAD_AVX512_VERTICAL_H_
#define SINGLETHREAD_AVX512_VERTICAL_H_

#include <x86intrin.h>

#include <cstddef>

// #include "singlethread_scalar.h"

void PrefixSumAVX512Vertical(const size_t input_len, const size_t partition_len,
                             const int algo_id, const float* input,
                             float* output, const float input_offset = 0);

// TODO: Generic template doesn't work.
template <typename Type>
void PrefixSumAVX512Vertical(const size_t data_len, Type* data) {
  // PrefixSum(data_len, data);  // Use scalar code instead.
  assert(false);
}

// // Specialization for floats.
// template <>
// void PrefixSumAVX512Vertical<float>(const size_t data_len, float* data) {
//   PrefixSumAVX512Vertical(data_len, data, data);
// }

// TODO: Used for testing only.
void PrefixSumAVX512Vertical1(const size_t input_len, const float* input,
                              float* output);
void PrefixSumAVX512Vertical2(const size_t input_len, const float* input,
                              float* output);
void PrefixSumAVX512Vertical3(const size_t input_len, const float* input,
                              float* output);
void PrefixSumAVX512Vertical4(const size_t input_len, const float* input,
                              float* output);

#endif  // SINGLETHREAD_AVX512_VERTICAL_H_
