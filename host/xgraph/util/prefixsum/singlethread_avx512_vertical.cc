#include "singlethread_avx512_vertical.h"

#include <x86intrin.h>

#include <cassert>
#include <limits>

#include "helper_functions.h"

void PrefixSumAVX512Vertical1(const size_t input_len, const float* input,
                              float* output) {
  assert(16 < input_len && input_len < std::numeric_limits<int>::max());
  assert(input_len % 16 == 0);
  const int chunk_len{
      static_cast<int>(input_len / 16)};  // TODO: Only works for 32-bit.

  // 1st Pass: Compute prefix sums in 16 chunks.
  __m512 sums{PrefixSumAVX512VerticalHelper(chunk_len, input, output)};

  // 2nd Pass: Increment results in 16 chunks.
  sums = _mm512_castsi512_ps(_mm512_slli_si512(_mm512_castps_si512(sums), 1));
  sums = Scan(sums);
  __m512i idx{_mm512_mullo_epi32(
      _mm512_set1_epi32(chunk_len),
      _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))};
  const __m512i idx_inc{_mm512_set1_epi32(1)};
  for (int i{0}; i < chunk_len; ++i) {
    const __m512 in{_mm512_i32gather_ps(idx, output, sizeof(float))};
    const __m512 out{_mm512_add_ps(sums, in)};
    _mm512_i32scatter_ps(output, idx, out, sizeof(float));
    idx = _mm512_add_epi32(idx, idx_inc);
  }
}

void PrefixSumAVX512Vertical2(const size_t input_len, const float* input,
                              float* output) {
  assert(16 < input_len && input_len < std::numeric_limits<int>::max());
  assert(input_len % 16 == 0);
  const int chunk_len{
      static_cast<int>(input_len / 16)};  // TODO: Only works for 32-bit.

  // 1st Pass: Compute prefix sums in 16 chunks.
  PrefixSumAVX512VerticalHelper(chunk_len, input, output);

  // 2nd Pass: Increment results horizontally without using gather/scatter.
  assert(chunk_len % 16 == 0);  // TODO: Support arbitrary sizes.
  for (int j{1}; j < 16; ++j) {
    const int chunk_begin{j * chunk_len};
    const int chunk_end{chunk_begin + chunk_len};
    const __m512 previous_sum{_mm512_set1_ps(output[chunk_begin - 1])};
    for (int i{chunk_begin}; i < chunk_end; i += 16) {
      // TODO: Use aligned load/store if possible.
      __m512 tmp{_mm512_load_ps(&output[i])};
      tmp = _mm512_add_ps(tmp, previous_sum);
      _mm512_store_ps(&output[i], tmp);
    }
  }
}

void PrefixSumAVX512Vertical3(const size_t input_len, const float* input,
                              float* output) {
  assert(16 < input_len && input_len < std::numeric_limits<int>::max());
  assert(input_len % 16 == 0);
  const int chunk_len{
      static_cast<int>(input_len / 16)};  // TODO: Only works for 32-bit.

  // 1st Pass: Accumulate partial sums in 16 chunks.
  __m512i idx{_mm512_mullo_epi32(
      _mm512_set1_epi32(chunk_len),
      _mm512_set_epi32(14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0))};
  const __m512i idx_inc{_mm512_set1_epi32(1)};
  const __mmask16 mask{0xfffe};
  __m512 sum{_mm512_setzero_ps()};
  for (int i{0}; i < chunk_len; ++i) {
    const __m512 in{_mm512_mask_i32gather_ps(_mm512_setzero_ps(), mask, idx,
                                             input, sizeof(float))};
    sum = _mm512_add_ps(sum, in);
    idx = _mm512_maskz_add_epi32(mask, idx, idx_inc);
  }
  sum = Scan(sum);

  // 2nd Pass: Compute prefix sums in 16 chunks.
  PrefixSumAVX512VerticalHelper(chunk_len, input, output, sum);
}

void PrefixSumAVX512Vertical4(const size_t input_len, const float* input,
                              float* output) {
  assert(16 < input_len && input_len < std::numeric_limits<int>::max());
  assert(input_len % 16 == 0);
  const int chunk_len{
      static_cast<int>(input_len / 16)};  // TODO: Only works for 32-bit.

  // 1st Pass: Accumulate partial sums horizontally.
  float sums[16] = {0};
  for (int i{0}; i < 15; ++i) {
    sums[i + 1] = sums[i] + AccumulateSIMD(chunk_len, input + i * chunk_len);
  }
  const __m512 sum{_mm512_loadu_ps(sums)};

  // 2nd Pass: Compute prefix sums in 16 chunks.
  PrefixSumAVX512VerticalHelper(chunk_len, input, output, sum);
}

void PrefixSumAVX512Vertical(const size_t input_len, const size_t partition_len,
                             const int algo_id, const float* input,
                             float* output, const float input_offset) {
  assert(input_len % partition_len == 0);
  void (*func)(const size_t input_len, const float* input, float* output);
  switch (algo_id) {
    case 1:
      func = PrefixSumAVX512Vertical1;
      break;
    case 2:
      func = PrefixSumAVX512Vertical2;
      break;
    case 3:
      func = PrefixSumAVX512Vertical3;
      break;
    case 4:
      func = PrefixSumAVX512Vertical4;
      break;
    default:
      assert(false);
  }

  // TODO: Make this work for out-of-place.
  assert(input == output);
  float last_element_of_previous_chunk{input_offset};
  for (size_t base{0}; base < input_len; base += partition_len) {
    output[base] += last_element_of_previous_chunk;
    func(partition_len, input + base, output + base);
    last_element_of_previous_chunk = output[base + partition_len - 1];
  }
}