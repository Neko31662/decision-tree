#pragma once
#include "config/config.h"
#include "utils/logs.h"
#include "utils/multi_party_net_io.h"
#include <cstdio>
#include <iostream>
#include <string>

// 计算最高位 1 的位置（最低位为1）
constexpr static inline int highest_bit_pos(uint64_t x) {
    assert(x != 0);
    int pos = 0;
    while (x) {
        ++pos;
        x >>= 1;
    }
    return pos;
}

constexpr static inline int nxt_prime(uint x) {
    assert(x <= ShareValue_BitLength);
    auto is_prime = [](int n) {
        if (n <= 1)
            return false;
        for (int i = 2; i * i <= n; i++) {
            if (n % i == 0)
                return false;
        }
        return true;
    };
    x++;
    while (!is_prime(x)) {
        x++;
    }
    return x;
}

inline int byte_of(ShareValue p) {
    int bytep = 1;
    ShareValue one(1);
    while ((one << (bytep * 8)) < p) {
        bytep++;
        if (bytep >= sizeof(ShareValue)) {
            return sizeof(ShareValue);
        }
    }
    return bytep;
}

template <typename T> std::vector<T *> make_ptr_vec(std::vector<T> &vec) {
    std::vector<T *> ptrs;
    ptrs.reserve(vec.size());
    for (auto &item : vec) {
        ptrs.push_back(&item);
    }
    return ptrs;
}

template <typename T, typename... Ts>
std::vector<T *> make_ptr_vec(T &first, T &second, Ts &...rest) {
    using PtrT = std::add_pointer_t<T>;
    return std::vector<PtrT>{&first, &second, &rest...};
}

template <typename Base, typename... Ts> std::vector<Base *> make_ptr_vec(Ts &...objs) {
    static_assert(((std::is_base_of_v<Base, Ts> || std::is_same_v<Base, Ts>) && ...),
                  "All types in Ts... must be Base or derive from Base");
    return std::vector<Base *>{static_cast<Base *>(&objs)...};
}