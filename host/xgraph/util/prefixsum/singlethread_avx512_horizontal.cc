#include <x86intrin.h>

#include <cassert>
#include <cstddef>

#include "helper_functions.h"

void PrefixSumAVX512Horizontal(const size_t input_size, const float* input,
                               float* output, const float input_offset) {
  assert(input_size % 16 == 0);

  __m512 offset = _mm512_set1_ps(input_offset);
  for (size_t i = 0; i < input_size; i += 16) {
    const __m512 in = _mm512_load_ps(&input[i]);
    const __m512 out = _mm512_add_ps(Scan(in), offset);
    _mm512_store_ps(&output[i], out);

    // Broadcast last element.
    offset = _mm512_permutexvar_ps(_mm512_set1_epi32(15), out);
  }
}
