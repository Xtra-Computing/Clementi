#ifndef SINGLETHREAD_AVX512_HORIZONTAL_H_
#define SINGLETHREAD_AVX512_HORIZONTAL_H_

#include <x86intrin.h>

#include <cstddef>

void PrefixSumAVX512Horizontal(const size_t input_len, const float* input,
                               float* output, const float input_offset = 0);

// TODO: Generic template doesn't work.
template <typename Type>
void PrefixSumAVX512Horizontal(const size_t data_len, Type* data) {
  // PrefixSum(data_len, data);  // Use scalar code instead.
  assert(false);
}

// Specialization for floats.
template <>
inline void PrefixSumAVX512Horizontal<float>(const size_t data_len,
                                             float* data) {
  PrefixSumAVX512Horizontal(data_len, data, data);
}

#endif  // SINGLETHREAD_AVX512_HORIZONTAL_H_
