//
// Created by Shixuan Sun on 2020/10/18.
//

#include <iostream>
#include <algorithm>
#include <vector>
#include "utility.h"

int main() {
    const int size = 6;
    int64_t a[size] = {0, 2, 5, 6, 8, 9};
    int64_t x = 5;
    std::cout << "Branch free search: " << Utility::branchfree_search(a, size, x) << std::endl;
    std::cout << "Traditional binary search: " << Utility::binary_search((int64_t*)a, 0, size, x) << std::endl;
    std::cout << "Lower bound: " << std::lower_bound(a, a + size, x) - a << std::endl;
    std::cout << "Upper bound: " << std::upper_bound(a, a + size, x) - a << std::endl;

    std::vector<int64_t> v(a, a + size);

    std::vector<int64_t>::iterator lower,upper;
    lower = std::lower_bound (v.begin(), v.end(), x);
    upper = std::upper_bound (v.begin(), v.end(), x);

    std::cout << "lower_bound for 6 at position " << (lower- v.begin()) << '\n';
    std::cout << "upper_bound for 6 at position " << (upper - v.begin()) << '\n';
    return 0;
}