#include "singlethread_tree.h"

#include <x86intrin.h>

#include <cassert>

// https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
void PrefixSumTree(const size_t len, float* x) {
  assert((len & (len - 1)) == 0);  // len is power of two

  // Pass 1: Up-sweep.
  for (size_t d = 2; d <= len; d <<= 1) {
    for (size_t i = 0; i < len; i += d) {
      x[i + d - 1] += x[i + (d >> 1) - 1];
    }
  }

  // Pass 2: Down-sweep.
  x[len - 1] = 0.0;
  for (size_t d = len; d >= 2; d >>= 1) {
    for (size_t i = 0; i < len; i += d) {
      float t = x[i + (d >> 1) - 1];
      x[i + (d >> 1) - 1] = x[i + d - 1];
      x[i + d - 1] = x[i + d - 1] + t;
    }
  }
}

void PrefixSumTreePartitioned(const size_t len, const size_t chunk_len,
                              float* x) {
  // TODO: Check if chunk_len should be L1 size.
  assert(len % chunk_len == 0);
  float last_element_of_previous_chunk{};
  for (size_t base{0}; base < len; base += chunk_len) {
    if (base == 0) {
	    // Make a copy of the last element so it won't be overwrite.
      last_element_of_previous_chunk = x[base + chunk_len - 1];
      PrefixSumTree(chunk_len, x);
      continue;
    }
    const float tmp1 = x[base - 1];
    const float tmp2 = x[base + chunk_len - 2];
    x[base - 1] += last_element_of_previous_chunk;
    PrefixSumTree(chunk_len, x + base - 1);
    x[base - 1] = tmp1;
    last_element_of_previous_chunk = x[base + chunk_len - 1];
    x[base + chunk_len - 1] = x[base + chunk_len - 2] + tmp2;
  }
}

void PrefixSumTreeSIMD(const size_t len, float* x) {
  for (int d = 2; d <= len; d <<= 1) {
    int num = len / d;
    if (num < 16) {
      for (int i = 0; i < len; i += d) {
        x[i + d - 1] += x[i + (d >> 1) - 1];
      }
    } else {
      // TODO: This assumes idx is 32-bit int.
      assert(num % 16 == 0);
      __m512i lIdx = _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
                                      3, 2, 1, 0);
      lIdx = _mm512_mullo_epi32(lIdx, _mm512_set1_epi32(d));
      __m512i rIdx = lIdx;
      lIdx = _mm512_add_epi32(lIdx, _mm512_set1_epi32(d - 1));
      rIdx = _mm512_add_epi32(rIdx, _mm512_set1_epi32((d >> 1) - 1));

      for (int offset = 0; offset < num; offset += 16) {
        __m512 e1 = _mm512_i32gather_ps(lIdx, x, 4);
        __m512 e2 = _mm512_i32gather_ps(rIdx, x, 4);
        __m512 sum = _mm512_add_ps(e1, e2);
        _mm512_i32scatter_ps(x, lIdx, sum, 4);

        lIdx = _mm512_add_epi32(lIdx, _mm512_set1_epi32(16 * d));
        rIdx = _mm512_add_epi32(rIdx, _mm512_set1_epi32(16 * d));
      }
    }
  }

  x[len - 1] = 0.0;
  for (int d = len; d >= 2; d >>= 1) {
    int num = len / d;
    if (num < 16) {
      for (int i = 0; i < len; i += d) {
        float t = x[i + (d >> 1) - 1];
        x[i + (d >> 1) - 1] = x[i + d - 1];
        x[i + d - 1] = x[i + d - 1] + t;
      }
    } else {
      assert(num % 16 == 0);
      __m512i lIdx = _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
                                      3, 2, 1, 0);
      lIdx = _mm512_mullo_epi32(lIdx, _mm512_set1_epi32(d));
      __m512i rIdx = lIdx;
      lIdx = _mm512_add_epi32(lIdx, _mm512_set1_epi32(d - 1));
      rIdx = _mm512_add_epi32(rIdx, _mm512_set1_epi32((d >> 1) - 1));

      for (int offset = 0; offset < num; offset += 16) {
        __m512 e1 = _mm512_i32gather_ps(lIdx, x, 4);
        __m512 e2 = _mm512_i32gather_ps(rIdx, x, 4);
        _mm512_i32scatter_ps(x, rIdx, e1, 4);
        __m512 sum = _mm512_add_ps(e1, e2);
        _mm512_i32scatter_ps(x, lIdx, sum, 4);

        lIdx = _mm512_add_epi32(lIdx, _mm512_set1_epi32(16 * d));
        rIdx = _mm512_add_epi32(rIdx, _mm512_set1_epi32(16 * d));
      }
    }
  }
}
