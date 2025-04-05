#ifndef SINGLETHREAD_TREE_H_
#define SINGLETHREAD_TREE_H_

#include <cstddef>

// TODO: Add inclusive, out-of-place versions.

void PrefixSumTree(const size_t data_len, float* data);

void PrefixSumTreePartitioned(const size_t data_len, const size_t partition_len,
                              float* data);

void PrefixSumTreeSIMD(const size_t data_len, float* data);

#endif  // SINGLETHREAD_TREE_H_