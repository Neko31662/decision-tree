template <int in_bits, typename Group, typename Prg, typename In>
    requires((std::is_unsigned_v<In> || std::is_same_v<In, __uint128_t>) &&
             in_bits <= static_cast<int>(sizeof(In) * 8) && Groupable<Group> &&
             Prgable<Prg, 4>)
void Dicf<in_bits, Group, Prg, In>::Gen(Key keys[2], const int4 seeds_a[2],
                                        const int4 seeds_b[2], In p, In q, In r) {
    GenWithRingBits(keys, seeds_a, seeds_b, p, q, r, in_bits);
}

template <int in_bits, typename Group, typename Prg, typename In>
    requires((std::is_unsigned_v<In> || std::is_same_v<In, __uint128_t>) &&
             in_bits <= static_cast<int>(sizeof(In) * 8) && Groupable<Group> &&
             Prgable<Prg, 4>)
void Dicf<in_bits, Group, Prg, In>::GenWithRingBits(Key keys[2], const int4 seeds_a[2],
                                                    const int4 seeds_b[2], In p, In q, In r,
                                                    int ring_bits) {
    if (ring_bits <= 0 || ring_bits > in_bits) {
        throw std::invalid_argument("Dicf::GenWithRingBits: invalid ring_bits");
    }

    In a = ModNBits(static_cast<In>(p + r), ring_bits);
    In b = ModNBits(static_cast<In>(q + In(1) + r), ring_bits);
    bool c = (a > b);

    int4 beta_one = BoolToGroupBuf(true);

    std::array<DcfCw, in_bits + 1> cws_a_tmp;
    std::array<DcfCw, in_bits + 1> cws_b_tmp;
    dcf_.Gen(cws_a_tmp.data(), seeds_a, a, beta_one);
    dcf_.Gen(cws_b_tmp.data(), seeds_b, b, beta_one);

    keys[0].seed_a = fss::util::SetLsb(seeds_a[0], false);
    keys[1].seed_a = fss::util::SetLsb(seeds_a[1], false);
    keys[0].seed_b = fss::util::SetLsb(seeds_b[0], false);
    keys[1].seed_b = fss::util::SetLsb(seeds_b[1], false);

    keys[0].cws_a = cws_a_tmp;
    keys[1].cws_a = cws_a_tmp;
    keys[0].cws_b = cws_b_tmp;
    keys[1].cws_b = cws_b_tmp;

    // Share wrap-correction bit c in Group.
    auto rnd_buf = prg_.Gen(keys[0].seed_a)[0];
    rnd_buf = fss::util::SetLsb(rnd_buf, false);
    auto c0 = Group::From(rnd_buf);
    auto c_all = Group::From(BoolToGroupBuf(c));
    auto c1 = c_all + (-c0);

    keys[0].c_share = c0.Into();
    keys[1].c_share = c1.Into();
}

template <int in_bits, typename Group, typename Prg, typename In>
    requires((std::is_unsigned_v<In> || std::is_same_v<In, __uint128_t>) &&
             in_bits <= static_cast<int>(sizeof(In) * 8) && Groupable<Group> &&
             Prgable<Prg, 4>)
int4 Dicf<in_bits, Group, Prg, In>::Eval(bool party, const Key &key, In y) {
    int4 ua = dcf_.Eval(party, key.seed_a, key.cws_a.data(), y);
    int4 ub = dcf_.Eval(party, key.seed_b, key.cws_b.data(), y);
    auto z = Group::From(ub) + (-Group::From(ua)) + Group::From(key.c_share);
    return z.Into();
}
