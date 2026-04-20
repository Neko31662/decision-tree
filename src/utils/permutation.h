#pragma once
#include "config/config.h"
#include "utils/prg_sync.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

// 生成长度为n的随机排列，使用seed作为随机种子
// seed长度为8字节
inline std::vector<int> gen_random_permutation(int n, PRGSync &prg) {
    if (n <= 0) {
        return std::vector<int>();
    }
    char seed[8];
    prg.gen_random_data((void *)seed, 8);

    // 初始化排列 [0, 1, 2, ..., n-1]
    std::vector<int> permutation(n);
    for (int i = 0; i < n; i++) {
        permutation[i] = i;
    }

    // 使用seed初始化随机数生成器
    // 将8字节seed转换为64位整数
    uint64_t seed_value = 0;
    for (int i = 0; i < 8; i++) {
        seed_value = (seed_value << 8) | static_cast<uint8_t>(seed[i]);
    }

    // 简单的线性同余随机数生成器
    // 使用与许多标准库兼容的参数
    uint64_t state = seed_value;
    const uint64_t a = 1103515245ULL;
    const uint64_t c = 12345ULL;
    const uint64_t m = (1ULL << 32);

    // Fisher-Yates 洗牌算法
    for (int i = n - 1; i > 0; i--) {
        // 生成下一个随机数
        state = (a * state + c) % m;

        // 生成范围 [0, i] 内的随机索引
        int j = static_cast<int>(state % (i + 1));

        // 交换 permutation[i] 和 permutation[j]
        std::swap(permutation[i], permutation[j]);
    }

    return permutation;
}

inline std::vector<int> inv_permutation(const std::vector<int> &perm) {
    int n = perm.size();
    std::vector<int> rev_perm(n);
    for (int i = 0; i < n; i++) {
        rev_perm[perm[i]] = i;
    }
    return rev_perm;
}

template <typename T>
inline std::vector<T> apply_permutation(const std::vector<int> &pi, std::vector<T> &data) {
    if (pi.size() != data.size()) {
        throw std::invalid_argument("Permutation vector and data vector must have the same size");
    }
    std::vector<T> temp;
    for (size_t i = 0; i < pi.size(); i++) {
        temp.push_back(data[pi[i]]);
    }
    return temp;
}

template <typename T> inline void apply_permutation(const std::vector<int> &pi, T *data) {
    int size = pi.size();
    std::vector<T> temp(size);
    for (size_t i = 0; i < pi.size(); i++) {
        temp[i] = data[pi[i]];
    }
    for (size_t i = 0; i < size; i++) {
        data[i] = temp[i];
    }
}