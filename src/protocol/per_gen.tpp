namespace pi_pergen_detail {

/* 计算可表示[0, n-1]所需的最小比特宽度
 * @param n: 排列长度
 */
inline int perm_bitlen_from_n(int n) {
    if (n <= 1) {
        return 1;
    }
    int ell = 0;
    int v = n - 1;
    while (v > 0) {
        ell++;
        v >>= 1;
    }
    return ell;
}

} // namespace pi_pergen_detail

void PI_pergen(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio, int n,
               MSSshare_permutation &output_perm, int ell) {
#ifdef DEBUG_MODE
    if (party_id < 0 || party_id > 2) {
        error("PI_pergen: party_id must be 0/1/2");
    }
    if (n < 0) {
        error("PI_pergen: n must be non-negative");
    }
#endif
#ifdef DEBUG_MODE
    if (ell > ShareValue_BitLength) {
        error("PI_pergen: ell exceeds ShareValue_BitLength");
    }
#endif

    int use_ell = ell;
    if (use_ell <= 0) {
        use_ell = pi_pergen_detail::perm_bitlen_from_n(n);
    }
#ifdef DEBUG_MODE
    int min_ell = pi_pergen_detail::perm_bitlen_from_n(n);
    if (use_ell < min_ell) {
        error("PI_pergen: provided ell is too small for n");
    }
#endif

    if (n == 0) {
        output_perm = MSSshare_permutation(0, use_ell);
        return;
    }

    MSSshare_v perm_seed_share(n, use_ell);
    std::vector<ShareValue> perm_seed_plain(n, 0);

    // 第1步：P0 生成私有随机排列 pi0，并以MSS形式分享给三方
    if (party_id == 0) {
        block local_seed = gen_seed();
        PRGSync local_prg(&local_seed);
        std::vector<int> pi0 = gen_random_permutation(n, local_prg);
        for (int i = 0; i < n; i++) {
            perm_seed_plain[i] = (ShareValue)pi0[i];
        }
    }
    for (int i = 0; i < n; i++) {
        MSSshare_preprocess(0, party_id, PRGs, netio, &perm_seed_share[i]);
        MSSshare_share_from(0, party_id, netio, &perm_seed_share[i], perm_seed_plain[i]);
    }

    // 第2步：P1/P2 执行一轮 shuffle 子协议（与安全混洗中的 round 逻辑一致）
    MSSshare_v shuffled_once(n, use_ell);
    pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, perm_seed_share, shuffled_once, 1, 2,
                                     0);

    // 第3步：将结果移动到排列分享容器中，避免大规模拷贝
    output_perm.shares = std::move(shuffled_once);
}
