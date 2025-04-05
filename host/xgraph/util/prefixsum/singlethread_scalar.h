#ifndef SINGLETHREAD_SCALAR_H_
#define SINGLETHREAD_SCALAR_H_

#include <cstddef>

template <typename Type>
void PrefixSum(const size_t input_len, const Type* input, Type* output,
               const Type input_offset = 0) {
  Type sum{input_offset};
  for (size_t i{0}; i < input_len; ++i) {
    sum += input[i];
    output[i] = sum;
  }
}

template <typename Type>
void PrefixSum(const size_t data_len, Type* data) {
  PrefixSum(data_len, data, data);
}

#endif  // SINGLETHREAD_SCALAR_H_
