#include "protocol/dicf.h"
#include "protocol/share_party_init.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <fss/prg/chacha.cuh>

using namespace std;

namespace {

template <typename T> T int4_to_uint(int4 v) {
    using UT = T;
    if constexpr (sizeof(UT) <= 4) {
        return static_cast<UT>(static_cast<uint32_t>(v.x));
    } else if constexpr (sizeof(UT) == 8) {
        return static_cast<UT>(static_cast<uint32_t>(v.x)) |
               (static_cast<UT>(static_cast<uint32_t>(v.y)) << 32);
    } else {
        return static_cast<UT>(static_cast<uint32_t>(v.x)) |
               (static_cast<UT>(static_cast<uint32_t>(v.y)) << 32) |
               (static_cast<UT>(static_cast<uint32_t>(v.z)) << 64) |
               (static_cast<UT>(static_cast<uint32_t>(v.w) >> 1) << 96);
    }
}

template <typename In> In mod_pow2_bits(In v, int bits) {
    if (bits == static_cast<int>(sizeof(In) * 8)) {
        return v;
    }
    return v & ((In(1) << bits) - In(1));
}

template <typename In> long long to_signed_bits(In v, int bits) {
    In sign_bit = In(1) << (bits - 1);
    if ((v & sign_bit) == 0) {
        return static_cast<long long>(v);
    }
    In mod = In(1) << bits;
    return -static_cast<long long>(mod - v);
}

int4 sample_seed(std::mt19937_64 &rng) {
    uint32_t x = static_cast<uint32_t>(rng());
    uint32_t y = static_cast<uint32_t>(rng());
    uint32_t z = static_cast<uint32_t>(rng());
    uint32_t w = static_cast<uint32_t>(rng()) & ~uint32_t(1);
    return int4{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), static_cast<int>(w)};
}

} // namespace

int main(int argc, char **argv) {
    int party_id = atoi(argv[1]);
    init_share_party_refs(party_id);

    constexpr int in_bits = 8;
    using In = uint16_t;
    using Group = fss::group::Uint<uint32_t>;
    using Prg = fss::prg::ChaCha<4>;
    using DicfType = Dicf<in_bits, Group, Prg, In>;

    int nonce[2] = {static_cast<int>(0x12345678u), static_cast<int>(0x9abcdef0u)};
    Prg prg(nonce);
    DicfType dicf(prg);

    std::mt19937_64 rng(0x20260505ULL);
    constexpr In N = In(1) << in_bits;

    // Generic interval tests.
    for (int t = 0; t < 80; t++) {
        In p = static_cast<In>(rng() % N);
        In q = static_cast<In>(p + (rng() % (N - p)));
        if (q == N - 1) {
            q = N - 2; // Keep q < N-1 as required by decomposition.
        }
        In r = static_cast<In>(rng() % N);

        int4 seeds_a[2] = {sample_seed(rng), sample_seed(rng)};
        int4 seeds_b[2] = {sample_seed(rng), sample_seed(rng)};
        typename DicfType::Key k[2];
        dicf.Gen(k, seeds_a, seeds_b, p, q, r);

        for (In x = 0; x < N; x++) {
            In y = mod_pow2_bits<In>(static_cast<In>(x + r), in_bits);
            int4 z0 = dicf.Eval(false, k[0], y);
            int4 z1 = dicf.Eval(true, k[1], y);
            int4 rec = DicfType::Reconstruct(z0, z1);
            uint32_t out = int4_to_uint<uint32_t>(rec);
            uint32_t expect = (x >= p && x <= q) ? 1u : 0u;
            if (out != expect) {
                cout << "DICF generic mismatch: t=" << t << ", x=" << static_cast<int>(x)
                     << ", p=" << static_cast<int>(p) << ", q=" << static_cast<int>(q)
                     << ", r=" << static_cast<int>(r) << ", out=" << out
                     << ", expect=" << expect << endl;
                return 1;
            }
        }
    }

    // CSOT special case: [0, tau], tau=2^(in_bits-1)-1.
    {
        In tau = (In(1) << (in_bits - 1)) - 1;
        In p = 0;
        In q = tau;
        for (int t = 0; t < 1000; t++) {
            In rho = static_cast<In>(rng() % N);
            In m0 = static_cast<In>(rng() % N);
            In m1 = static_cast<In>(rng() % N);

            int4 seeds_a[2] = {sample_seed(rng), sample_seed(rng)};
            int4 seeds_b[2] = {sample_seed(rng), sample_seed(rng)};
            typename DicfType::Key k[2];
            dicf.Gen(k, seeds_a, seeds_b, p, q, rho);

            In delta = mod_pow2_bits<In>(static_cast<In>(m1 - m0 + rho), in_bits);
            int4 z0 = dicf.Eval(false, k[0], delta);
            int4 z1 = dicf.Eval(true, k[1], delta);
            int4 rec = DicfType::Reconstruct(z0, z1);
            uint32_t out = int4_to_uint<uint32_t>(rec);
            In diff = mod_pow2_bits<In>(static_cast<In>(m1 - m0), in_bits);
            uint32_t expect = (diff <= tau) ? 1u : 0u;
            if (out != expect) {
                cout << "DICF CSOT mismatch: t=" << t << ", m0=" << static_cast<int>(m0)
                     << ", m1=" << static_cast<int>(m1) << ", rho=" << static_cast<int>(rho)
                     << ", out=" << out << ", expect=" << expect << endl;
                return 1;
            }
        }
    }

    cout << "DICF test passed!" << endl;
    return 0;
}
