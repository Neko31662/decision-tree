#pragma once
#include "config/config.h"
#include "utils/misc.h"
#include <emp-tool/emp-tool.h>
using block = emp::block;
using PRG = emp::PRG;

inline block gen_seed() {
    block v;
#ifndef ENABLE_RDSEED
    uint32_t *data = (uint32_t *)(&v);
    std::random_device rand_div("/dev/urandom");
    for (size_t i = 0; i < sizeof(block) / sizeof(uint32_t); ++i)
        data[i] = rand_div();
#else
    unsigned long long r0, r1;
    int i = 0;
    for (; i < 10; ++i)
        if ((_rdseed64_step(&r0) == 1) && (r0 != ULLONG_MAX) && (r0 != 0))
            break;
    if (i == 10)
        emp::error("RDSEED FAILURE");

    for (i = 0; i < 10; ++i)
        if ((_rdseed64_step(&r1) == 1) && (r1 != ULLONG_MAX) && (r1 != 0))
            break;
    if (i == 10)
        emp::error("RDSEED FAILURE");

    v = emp::makeBlock(r0, r1);
#endif
    return v;
}

class PRGSync {
    PRG prg;

  public:
    static const uint BATCH_SIZE = 1 << 5;
    static const uint POOL_SIZE = BATCH_SIZE << 5;
    static const uint INDEX_MASK = POOL_SIZE - 1;

    int cnt = 0;
    int cnt_prefetch = 0;
    std::vector<unsigned char> pool;

    PRGSync(const block *seed, int id = 0) : prg(seed, id) { pool.resize(POOL_SIZE); }

    // Generate nbytes of random data into data, return the starting index
    int gen_random_data(void *data, int nbytes) {
        int cur = 0;
        int start = cnt;
        if (cnt < cnt_prefetch) {
            int len = std::min(nbytes, cnt_prefetch - cnt);
            memcpy((unsigned char *)data + cur, pool.data() + (cnt & INDEX_MASK), len);
            cur += len;
            cnt += len;
        }
        while (nbytes - cur >= BATCH_SIZE) {
            prg.random_data((void *)(pool.data() + (cnt_prefetch & INDEX_MASK)), BATCH_SIZE);
            memcpy((unsigned char *)data + cur, pool.data() + (cnt_prefetch & INDEX_MASK),
                   BATCH_SIZE);
            cur += BATCH_SIZE;
            cnt_prefetch += BATCH_SIZE;
            cnt = cnt_prefetch;
        }
        if (nbytes - cur > 0) {
            prg.random_data((void *)(pool.data() + (cnt_prefetch & INDEX_MASK)), BATCH_SIZE);
            memcpy((unsigned char *)data + cur, pool.data() + (cnt_prefetch & INDEX_MASK),
                   nbytes - cur);
            cnt_prefetch += BATCH_SIZE;
            cnt += (nbytes - cur);
        }
        return start;
    }
};