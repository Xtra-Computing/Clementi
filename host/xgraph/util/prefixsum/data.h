#ifndef DATA_H_
#define DATA_H_

#include <algorithm>
#include <cstddef>
#include <random>

// TODO: Generate random data for all types, without overflow.

// For float, 99% 0.0 and 1% random numbers from (0,1).
void GenerateFloatData(const size_t data_len, float* data) {
  std::mt19937 rng;
  std::uniform_real_distribution<float> dis(0, 1);
  for (size_t i{0}; i < static_cast<size_t>(0.01 * data_len); ++i) {
    data[i] = dis(rng);
  }
  // TODO: No need to shuffle entire data.
  std::random_shuffle(data, data + data_len);
}

#endif  // DATA_H_