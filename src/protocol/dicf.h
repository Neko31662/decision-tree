#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include <fss/dcf.cuh>
#include <fss/group/uint.cuh>
#include <fss/prg/chacha.cuh>
#include <fss/util.cuh>

/*
 * DICF (Distributed Interval Containment Function)
 *
 * API style aligns with the imported FSS DCF style:
 * - Gen(): dealer-side key generation
 * - Eval(): party-side key evaluation
 *
 * This wrapper does not expose the source library namespace in its API.
 */
template <int in_bits, typename Group = fss::group::Uint<uint32_t>,
          typename Prg = fss::prg::ChaCha<4>, typename In = uint32_t>
    requires((std::is_unsigned_v<In> || std::is_same_v<In, __uint128_t>) &&
             in_bits <= static_cast<int>(sizeof(In) * 8) && Groupable<Group> &&
             Prgable<Prg, 4>)
class Dicf {
  public:
    using DcfType = fss::Dcf<in_bits, Group, Prg, In, fss::DcfPred::kLt>;
    using DcfCw = typename DcfType::Cw;

    struct Key {
        int4 seed_a;
        int4 seed_b;
        std::array<DcfCw, in_bits + 1> cws_a;
        std::array<DcfCw, in_bits + 1> cws_b;
        int4 c_share;
    };

    explicit Dicf(Prg prg) : prg_(prg), dcf_(prg_) {}

    /*
     * Generate 2-party DICF keys for interval [p, q] with input offset r.
     * Output semantics: z0 + z1 == 1[(y-r) in [p, q]] in Group.
     */
    void Gen(Key keys[2], const int4 seeds_a[2], const int4 seeds_b[2], In p, In q, In r);

    /*
     * Generate keys with runtime ring bit-length.
     * For ring Z_{2^ring_bits}, 1 <= ring_bits <= in_bits.
     */
    void GenWithRingBits(Key keys[2], const int4 seeds_a[2], const int4 seeds_b[2], In p, In q,
                         In r, int ring_bits);

    /*
     * Evaluate one DICF key share.
     * @param party party index: false for party 0, true for party 1.
     * @param key key share for this party.
     * @param y shifted input y = x + r mod 2^in_bits.
     */
    int4 Eval(bool party, const Key &key, In y);

    /*
     * Reconstruct helper: adds 2 shares in Group and returns int4 buffer.
     */
    static int4 Reconstruct(int4 z0, int4 z1) {
        return (Group::From(z0) + Group::From(z1)).Into();
    }

  private:
    static constexpr bool kFullBits = (in_bits == static_cast<int>(sizeof(In) * 8));

    static In ModN(In v) {
        if constexpr (kFullBits) {
            return v;
        } else {
            return v & ((In(1) << in_bits) - In(1));
        }
    }

    static In ModNBits(In v, int bits) {
        if (bits >= static_cast<int>(sizeof(In) * 8)) {
            return v;
        }
        return v & ((In(1) << bits) - In(1));
    }

    static int4 BoolToGroupBuf(bool b) {
        return Group::From(int4{b ? 1 : 0, 0, 0, 0}).Into();
    }

    Prg prg_;
    DcfType dcf_;
};

#include "protocol/dicf.tpp"
