namespace pi_ove_detail {

/* 构造长度m的明文排列，满足D[i]会被放到位置pos_i
 * 其中pos_0=0, pos_i=sum_{t=0}^{i-1}a_t (i>=1)
 * @param a: 复制次数数组，长度n
 * @param m: 扩展后长度，m=sum(a_i)
 * @param out_perm: 输出排列（长度m，取值为0..m-1）
 */
inline void build_plain_permutation_for_ove(const std::vector<int> &a, int m,
                                            std::vector<ShareValue> &out_perm) {
    int n = (int)a.size();
    out_perm.assign(m, 0);

    std::vector<int> pos(n, 0);
    for (int i = 1; i < n; i++) {
        pos[i] = pos[i - 1] + a[i - 1];
    }

    std::vector<char> used(m, 0);
    for (int i = 0; i < n; i++) {
        out_perm[i] = pos[i];
        used[pos[i]] = 1;
    }

    int ptr = 0;
    for (int i = n; i < m; i++) {
        while (ptr < m && used[ptr]) {
            ptr++;
        }
        if (ptr >= m) {
            error("PI_OVE: failed to build permutation");
        }
        out_perm[i] = ptr;
        used[ptr] = 1;
    }
}

/* 根据三元组(r1,r2,m)取出某个参与方需要持有的(x_prev,x_nxt)
 * @param party: 目标参与方id，0/1/2
 * @param r1: 复制分享中的r1分量
 * @param r2: 复制分享中的r2分量
 * @param m: 复制分享中的m分量
 * @param prev: 输出x_prev
 * @param nxt: 输出x_nxt
 */
inline void get_party_pair_from_triplet(int party, ShareValue r1, ShareValue r2, ShareValue m,
                                        ShareValue &prev, ShareValue &nxt) {
    if (party == 0) {
        prev = r1;
        nxt = r2;
    } else if (party == 1) {
        prev = m;
        nxt = r1;
    } else if (party == 2) {
        prev = r2;
        nxt = m;
    } else {
        error("PI_OVE: invalid party id in get_party_pair_from_triplet");
    }
}

/* 将单个MSSshare对象设置为已分享状态
 * @param s: 目标分享对象
 * @param prev: x_prev要设置的值
 * @param nxt: x_nxt要设置的值
 */
inline void set_shared_pair(MSSshare &s, ShareValue prev, ShareValue nxt) {
    s.x_prev() = prev;
    s.x_nxt() = nxt;
#ifdef DEBUG_MODE
    s.has_shared = true;
    s.has_preprocess = true;
#endif
}

} // namespace pi_ove_detail

void PI_OVE(const int party_id, const int s_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
            MSSshare_v &input_x, MSSshare_v &output_x, const std::vector<int> *input_a) {
    int n = input_x.size();
    int ell = input_x.BITLEN;
    ShareValue mask = input_x.MASK;

#ifdef DEBUG_MODE
    if (s_id < 0 || s_id > 2) {
        error("PI_OVE: s_id must be in {0,1,2}");
    }
    for (int i = 0; i < n; i++) {
        if (!input_x[i].has_shared) {
            error("PI_OVE: input_x must be shared before calling");
        }
    }
    if (party_id == s_id) {
        if (input_a == nullptr) {
            error("PI_OVE: S party must provide input_a");
        }
        if ((int)input_a->size() != n) {
            error("PI_OVE: input_a size must equal input_x size");
        }
        for (int i = 0; i < n; i++) {
            if ((*input_a)[i] < 1) {
                error("PI_OVE: each a_i must be >= 1");
            }
        }
    }
#endif

    int m = 0;
    std::vector<ShareValue> perm_plain;
    if (party_id == s_id) {
        for (int v : *input_a) {
            m += v;
        }
#ifdef DEBUG_MODE
        if (m < n) {
            error("PI_OVE: output length m must be >= input length n");
        }
        if (m > 0 && ShareValue(m - 1) > mask) {
            error("PI_OVE: permutation value exceeds input ell range");
        }
#endif
        pi_ove_detail::build_plain_permutation_for_ove(*input_a, m, perm_plain);
    }

    // 1) 与置换分享阶段合并：S在同一通信批次内发送m和置换分享
    //    置换分享采用“由S本地直接生成三份”方式，不调用preprocess/share_from。
    //    注意：这里perm_share的ell与input_x保持一致。
    block private_seed = gen_seed();
    PRGSync private_prg(&private_seed);
    MSSshare_permutation perm_share((party_id == s_id) ? m : 1, ell);
    if (party_id == s_id) {
        for (int dst = 0; dst < 3; dst++) {
            if (dst != s_id) {
                netio.store_data(dst, &m, sizeof(int));
            }
        }

        int bytem = perm_share[0].BYTELEN;
        for (int i = 0; i < m; i++) {
            ShareValue r1 = 0, r2 = 0;
            private_prg.gen_random_data(&r1, bytem);
            private_prg.gen_random_data(&r2, bytem);
            r1 &= mask;
            r2 &= mask;
            ShareValue m_comp = (perm_plain[i] - r1 - r2) & mask;

            ShareValue local_prev = 0, local_nxt = 0;
            pi_ove_detail::get_party_pair_from_triplet(s_id, r1, r2, m_comp, local_prev, local_nxt);
            pi_ove_detail::set_shared_pair(perm_share[i], local_prev, local_nxt);

            for (int dst = 0; dst < 3; dst++) {
                if (dst == s_id) {
                    continue;
                }
                ShareValue dst_prev = 0, dst_nxt = 0;
                pi_ove_detail::get_party_pair_from_triplet(dst, r1, r2, m_comp, dst_prev, dst_nxt);
                netio.store_data(dst, &dst_prev, bytem);
                netio.store_data(dst, &dst_nxt, bytem);
            }
        }
        for (int dst = 0; dst < 3; dst++) {
            if (dst != s_id) {
                netio.send_stored_data(dst);
            }
        }
    } else {
        netio.recv_data(s_id, &m, sizeof(int));
#ifdef DEBUG_MODE
        if (m < n) {
            error("PI_OVE: received m must be >= input length n");
        }
        if (m > 0 && ShareValue(m - 1) > mask) {
            error("PI_OVE: received permutation value exceeds input ell range");
        }
#endif
        perm_share = MSSshare_permutation(m, ell);
        int bytem = perm_share[0].BYTELEN;
        for (int i = 0; i < m; i++) {
            ShareValue prev = 0, nxt = 0;
            netio.recv_data(s_id, &prev, bytem);
            netio.recv_data(s_id, &nxt, bytem);
            pi_ove_detail::set_shared_pair(perm_share[i], prev, nxt);
        }
    }

    // 2) 计算差分向量D，并扩展到长度m（后缀补0）
    MSSshare_v d_ext(m, ell);
    if (n > 0) {
        d_ext[0] = input_x[0];
        for (int i = 1; i < n; i++) {
            d_ext[i] = input_x[i] - input_x[i - 1];
        }
    }
    for (int i = n; i < m; i++) {
        d_ext[i].v1 = 0;
        d_ext[i].v2 = 0;
#ifdef DEBUG_MODE
        d_ext[i].has_shared = true;
        d_ext[i].has_preprocess = true;
#endif
    }

    // 3) 安全置换D得到D'，并做前缀和恢复扩展向量
    MSSshare_v d_perm(m, ell);
    PI_secure_permutation(party_id, PRGs, netio, perm_share, d_ext, d_perm);

    output_x = MSSshare_v(m, ell);
    if (m > 0) {
        output_x[0] = d_perm[0];
        for (int i = 1; i < m; i++) {
            output_x[i] = output_x[i - 1] + d_perm[i];
        }
    }
}

