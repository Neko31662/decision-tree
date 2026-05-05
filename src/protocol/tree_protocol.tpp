namespace pi_tree_protocol_detail {

inline ShareValue mask_of_ell(int ell) {
    return (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
}

inline bool is_perm_0_to_n_1(const std::vector<int> &p, int n) {
    if ((int)p.size() != n) {
        return false;
    }
    std::vector<char> seen(n, 0);
    for (int v : p) {
        if (v < 0 || v >= n || seen[v]) {
            return false;
        }
        seen[v] = 1;
    }
    return true;
}

/* 基于复制次数构造OVE风格排列 */
inline void build_ove_like_perm_from_counts(const std::vector<int> &counts, int total,
                                            std::vector<int> &out_perm) {
    int base_n = (int)counts.size();
    out_perm.assign(total, 0);

    std::vector<int> pos(base_n, 0);
    for (int i = 1; i < base_n; i++) {
        pos[i] = pos[i - 1] + counts[i - 1];
    }

    std::vector<char> used(total, 0);
    for (int i = 0; i < base_n; i++) {
        out_perm[i] = pos[i];
        used[pos[i]] = 1;
    }

    int ptr = 0;
    for (int i = base_n; i < total; i++) {
        while (ptr < total && used[ptr]) {
            ptr++;
        }
        if (ptr >= total) {
            error("PI_tree_protocol: failed to build OVE-like permutation");
        }
        out_perm[i] = ptr;
        used[ptr] = 1;
    }
}

/* 从“按值分组”的重复序列映射到目标序列，构造sigma */
inline void build_sigma_from_counts_and_target(const std::vector<int> &counts,
                                               const std::vector<int> &target,
                                               bool drop_first_zero, std::vector<int> &out_sigma) {
    int base_n = (int)counts.size();
    std::vector<std::vector<int>> slots(base_n);
    int idx = 0;
    for (int v = 0; v < base_n; v++) {
        for (int k = 0; k < counts[v]; k++) {
            if (drop_first_zero && v == 0 && k == 0) {
                continue;
            }
            slots[v].push_back(idx++);
        }
    }

    out_sigma.assign((int)target.size(), 0);
    std::vector<int> ptr(base_n, 0);
    for (int i = 0; i < (int)target.size(); i++) {
        int v = target[i];
        if (v < 0 || v >= base_n || ptr[v] >= (int)slots[v].size()) {
            error("PI_tree_protocol: failed to build sigma from target");
        }
        out_sigma[i] = slots[v][ptr[v]++];
    }
}

/* 将MSS分享只恢复给指定参与方（不向其他方公开） */
inline void open_share_to_party(const int party_id, const int target_id, NetIOMP &netio, MSSshare *s,
                                ShareValue &out, bool &has_out) {
    has_out = false;
    out = 0;
    if (target_id == 0) {
        if (party_id == 1) {
            netio.send_data(0, &s->v1, s->BYTELEN);
        } else if (party_id == 0) {
            ShareValue m = 0;
            netio.recv_data(1, &m, s->BYTELEN);
            out = (m + s->v1 + s->v2) & s->MASK;
            has_out = true;
        }
    } else if (target_id == 1) {
        if (party_id == 0) {
            netio.send_data(1, &s->v2, s->BYTELEN);
        } else if (party_id == 1) {
            ShareValue r2 = 0;
            netio.recv_data(0, &r2, s->BYTELEN);
            out = (s->v1 + s->v2 + r2) & s->MASK;
            has_out = true;
        }
    } else if (target_id == 2) {
        if (party_id == 0) {
            netio.send_data(2, &s->v1, s->BYTELEN);
        } else if (party_id == 2) {
            ShareValue r1 = 0;
            netio.recv_data(0, &r1, s->BYTELEN);
            out = (s->v1 + s->v2 + r1) & s->MASK;
            has_out = true;
        }
    } else {
        error("PI_tree_protocol: invalid target_id in open_share_to_party");
    }
}
} // namespace pi_tree_protocol_detail

void PI_tree_protocol_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 PI_tree_protocol_intermediate &intermediate,
                                 const plain_tree *input_tree,
                                 const std::vector<ShareValue> *input_x) {
    const int s_id = 0;
    const int c_id = 1;
    int ell = intermediate.ell;
    int feature_num = intermediate.feature_num;
    int d = intermediate.d;

#ifdef DEBUG_MODE
    if (party_id < 0 || party_id > 2) {
        error("PI_tree_protocol_preprocess: party_id must be in {0,1,2}");
    }
    if (ell <= 0 || ell > ShareValue_BitLength) {
        error("PI_tree_protocol_preprocess: ell out of range");
    }
    if (feature_num <= 0 || d <= 0) {
        error("PI_tree_protocol_preprocess: feature_num and d must be positive");
    }
    if (party_id == s_id && input_tree == nullptr) {
        error("PI_tree_protocol_preprocess: P0 must provide input_tree");
    }
    if (party_id == c_id && input_x == nullptr) {
        error("PI_tree_protocol_preprocess: P1 must provide input_x");
    }
    if (intermediate.has_preprocess) {
        error("PI_tree_protocol_preprocess has already been called on this object");
    }
    intermediate.has_preprocess = true;
#endif

    ShareValue mask = pi_tree_protocol_detail::mask_of_ell(ell);
    int n = feature_num + ((d >= 2) ? (d - 2) : 0);
    int m = 0;

    std::vector<ShareValue> plain_t, plain_L, plain_R, plain_Lp, plain_Rp;
    std::vector<int> zeta_int, sigma_int, zeta_p_int, sigma_p_int;

    if (party_id == s_id) {
        const plain_tree &tree = *input_tree;
#ifdef DEBUG_MODE
        if (!tree.prepared) {
            error("PI_tree_protocol_preprocess: input_tree must be prepared");
        }
        tree.assert_shape_valid();
#endif
        m = tree.size();
        plain_t = tree.t;
        plain_L = tree.L;
        plain_R = tree.R;
        plain_Lp = tree.Lp;
        plain_Rp = tree.Rp;

        // 规范化prepare可能产生的尾部哨兵索引：m->m-1, n->n-1
        for (int i = 0; i < m; i++) {
#ifdef DEBUG_MODE
            if (plain_L[i] > (ShareValue)m || plain_R[i] > (ShareValue)m) {
                error("PI_tree_protocol_preprocess: node child index out of allowed range");
            }
            if (plain_Lp[i] > (ShareValue)n || plain_Rp[i] > (ShareValue)n) {
                error("PI_tree_protocol_preprocess: feature index out of allowed range");
            }
#endif
            if (plain_L[i] == (ShareValue)m) {
                plain_L[i] = (ShareValue)(m - 1);
            }
            if (plain_R[i] == (ShareValue)m) {
                plain_R[i] = (ShareValue)(m - 1);
            }
            if (plain_Lp[i] == (ShareValue)n) {
                plain_Lp[i] = (ShareValue)(n - 1);
            }
            if (plain_Rp[i] == (ShareValue)n) {
                plain_Rp[i] = (ShareValue)(n - 1);
            }
        }

        std::vector<int> child_nodes(2 * m, 0);
        std::vector<int> child_feats(2 * m, 0);
        for (int i = 0; i < m; i++) {
            child_nodes[2 * i] = (int)plain_L[i];
            child_nodes[2 * i + 1] = (int)plain_R[i];
            child_feats[2 * i] = (int)plain_Lp[i];
            child_feats[2 * i + 1] = (int)plain_Rp[i];
        }

        std::vector<int> count_nodes(m, 0);
        for (int v : child_nodes) {
            count_nodes[v]++;
        }
        count_nodes[0]++;

        std::vector<int> count_feats(n, 0);
        for (int v : child_feats) {
            count_feats[v]++;
        }
#ifdef DEBUG_MODE
        if (tree.alpha >= (ShareValue)n) {
            error("PI_tree_protocol_preprocess: alpha out of range");
        }
#endif
        count_feats[(int)tree.alpha]++;

#ifdef DEBUG_MODE
        // 要求每个特征至少被使用一次（L'/R'与alpha合并计数后）
        for (int i = 0; i < n; i++) {
            if (count_feats[i] <= 0) {
                error("PI_tree_protocol_preprocess: every feature must be used at least once");
            }
        }
#endif

        pi_tree_protocol_detail::build_ove_like_perm_from_counts(count_nodes, 2 * m + 1, zeta_int);
        pi_tree_protocol_detail::build_ove_like_perm_from_counts(count_feats, 2 * m + 1, zeta_p_int);

        std::vector<int> sigma_map, sigma_p_map;
        pi_tree_protocol_detail::build_sigma_from_counts_and_target(count_nodes, child_nodes, true,
                                                                    sigma_map);
        sigma_int = inv_permutation(sigma_map);

        std::vector<int> feat_target = child_feats;
        feat_target.push_back((int)tree.alpha);
        pi_tree_protocol_detail::build_sigma_from_counts_and_target(count_feats, feat_target, false,
                                                                    sigma_p_map);
        sigma_p_int = inv_permutation(sigma_p_map);

#ifdef DEBUG_MODE
        if (!pi_tree_protocol_detail::is_perm_0_to_n_1(zeta_int, 2 * m + 1)) {
            error("PI_tree_protocol_preprocess: zeta invalid");
        }
        if (!pi_tree_protocol_detail::is_perm_0_to_n_1(zeta_p_int, 2 * m + 1)) {
            error("PI_tree_protocol_preprocess: zeta' invalid");
        }
        if (!pi_tree_protocol_detail::is_perm_0_to_n_1(sigma_int, 2 * m)) {
            error("PI_tree_protocol_preprocess: sigma invalid");
        }
        if (!pi_tree_protocol_detail::is_perm_0_to_n_1(sigma_p_int, 2 * m + 1)) {
            error("PI_tree_protocol_preprocess: sigma' invalid");
        }
#endif

        for (int dst = 1; dst <= 2; dst++) {
            netio.send_data(dst, &m, sizeof(int));
        }
    } else {
        netio.recv_data(s_id, &m, sizeof(int));
    }

#ifdef DEBUG_MODE
    if (m <= 0 || n <= 0) {
        error("PI_tree_protocol_preprocess: invalid m/n");
    }
    if (ShareValue(std::max(m - 1, n - 1)) > mask) {
        error("PI_tree_protocol_preprocess: ell too small for indices");
    }
#endif

    // Set-up：S共享 t,zeta,sigma,zeta',sigma'
    int long_len = 2 * m + 1;
    int two_m = 2 * m;
    MSSshare_v sh_t(m, ell), sh_zeta(long_len, ell), sh_sigma(two_m, ell), sh_zeta_p(long_len, ell),
        sh_sigma_p(long_len, ell);

    for (int i = 0; i < m; i++) {
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_t[i]);
        ShareValue v = (party_id == s_id) ? (plain_t[i] & mask) : 0;
        MSSshare_share_from(s_id, party_id, netio, &sh_t[i], v);
    }
    for (int i = 0; i < long_len; i++) {
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_zeta[i]);
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_zeta_p[i]);
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_sigma_p[i]);
        ShareValue vz = (party_id == s_id) ? (ShareValue)zeta_int[i] : 0;
        ShareValue vzp = (party_id == s_id) ? (ShareValue)zeta_p_int[i] : 0;
        ShareValue vsp = (party_id == s_id) ? (ShareValue)sigma_p_int[i] : 0;
        MSSshare_share_from(s_id, party_id, netio, &sh_zeta[i], vz);
        MSSshare_share_from(s_id, party_id, netio, &sh_zeta_p[i], vzp);
        MSSshare_share_from(s_id, party_id, netio, &sh_sigma_p[i], vsp);
    }
    for (int i = 0; i < two_m; i++) {
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_sigma[i]);
        ShareValue vs = (party_id == s_id) ? (ShareValue)sigma_int[i] : 0;
        MSSshare_share_from(s_id, party_id, netio, &sh_sigma[i], vs);
    }

    // Model Shuffle 1: 生成pi,pi'
    MSSshare_permutation pi_perm(m, ell);
    MSSshare_permutation pi_p_perm(n, ell);
    PI_pergen(party_id, PRGs, netio, m, pi_perm, ell);
    PI_pergen(party_id, PRGs, netio, n, pi_p_perm, ell);

    // F'_pergen：仅向C公开pi'
    std::vector<int> opened_pi_p(n, 0);
    for (int i = 0; i < n; i++) {
        ShareValue out = 0;
        bool has_out = false;
        pi_tree_protocol_detail::open_share_to_party(party_id, c_id, netio, &pi_p_perm[i], out, has_out);
        if (has_out) {
            opened_pi_p[i] = (int)out;
        }
    }
#ifdef DEBUG_MODE
    if (party_id == c_id && !pi_tree_protocol_detail::is_perm_0_to_n_1(opened_pi_p, n)) {
        error("PI_tree_protocol_preprocess: opened pi' for client is not permutation");
    }
#endif

    // Model Shuffle 2~4: 差分 + 零填充 + 用zeta/zeta'做扩展并前缀和
    MSSshare_v delta(long_len, ell), delta_p(long_len, ell);
    delta[0] = pi_perm[0];
    for (int i = 1; i < m; i++) {
        delta[i] = pi_perm[i] - pi_perm[i - 1];
    }
    for (int i = m; i < long_len; i++) {
        delta[i].v1 = 0;
        delta[i].v2 = 0;
#ifdef DEBUG_MODE
        delta[i].has_shared = true;
        delta[i].has_preprocess = true;
#endif
    }

    delta_p[0] = pi_p_perm[0];
    for (int i = 1; i < n; i++) {
        delta_p[i] = pi_p_perm[i] - pi_p_perm[i - 1];
    }
    for (int i = n; i < long_len; i++) {
        delta_p[i].v1 = 0;
        delta_p[i].v2 = 0;
#ifdef DEBUG_MODE
        delta_p[i].has_shared = true;
        delta_p[i].has_preprocess = true;
#endif
    }

    MSSshare_permutation zeta_perm(long_len, ell), zeta_p_perm(long_len, ell);
    zeta_perm.vec() = sh_zeta;
    zeta_p_perm.vec() = sh_zeta_p;
    MSSshare_v delta_after(long_len, ell), delta_p_after(long_len, ell);
    PI_secure_permutation(party_id, PRGs, netio, zeta_perm, delta, delta_after);
    PI_secure_permutation(party_id, PRGs, netio, zeta_p_perm, delta_p, delta_p_after);
    for (int i = 1; i < long_len; i++) {
        delta_after[i] = delta_after[i - 1] + delta_after[i];
        delta_p_after[i] = delta_p_after[i - 1] + delta_p_after[i];
    }

    // Model Shuffle 5: 公开I0
    ShareValue I0 = MSSshare_recon(party_id, netio, &delta_after[0]);
    MSSshare_v delta_cut(two_m, ell);
    for (int i = 0; i < two_m; i++) {
        delta_cut[i] = delta_after[i + 1];
    }

    // Model Shuffle 6: 用sigma/sigma'放置
    MSSshare_permutation sigma_perm(two_m, ell), sigma_p_perm(long_len, ell);
    sigma_perm.vec() = sh_sigma;
    sigma_p_perm.vec() = sh_sigma_p;
    MSSshare_v hat_pi(two_m, ell), hat_pi_p(long_len, ell);
    PI_secure_permutation(party_id, PRGs, netio, sigma_perm, delta_cut, hat_pi);
    PI_secure_permutation(party_id, PRGs, netio, sigma_p_perm, delta_p_after, hat_pi_p);
    // Model Shuffle 7~8: 组装N*并用pi做整体打乱
    MSSshare_v node_t(m, ell), node_L(m, ell), node_R(m, ell), node_Lp(m, ell), node_Rp(m, ell);
    for (int i = 0; i < m; i++) {
        node_t[i] = sh_t[i];
        node_L[i] = hat_pi[2 * i];
        node_R[i] = hat_pi[2 * i + 1];
        node_Lp[i] = hat_pi_p[2 * i];
        node_Rp[i] = hat_pi_p[2 * i + 1];
    }

    MSSshare_v t_perm(m, ell), L_perm(m, ell), R_perm(m, ell), Lp_perm(m, ell), Rp_perm(m, ell);
    PI_secure_permutation(party_id, PRGs, netio, pi_perm, node_t, t_perm);
    PI_secure_permutation(party_id, PRGs, netio, pi_perm, node_L, L_perm);
    PI_secure_permutation(party_id, PRGs, netio, pi_perm, node_R, R_perm);
    PI_secure_permutation(party_id, PRGs, netio, pi_perm, node_Lp, Lp_perm);
    PI_secure_permutation(party_id, PRGs, netio, pi_perm, node_Rp, Rp_perm);

    // Model Shuffle 6(特征末位): 公开I1
    ShareValue I1 = MSSshare_recon(party_id, netio, &hat_pi_p[two_m]);

#ifdef DEBUG_MODE
    if (I0 >= (ShareValue)m || I1 >= (ShareValue)n) {
        error("PI_tree_protocol_preprocess: initial indices out of range");
    }
#endif

    // C本地按pi'置换并共享（作为预处理的一部分完成）
    MSSshare_v sh_x(n, ell);
    std::vector<ShareValue> x_perm_plain;
    if (party_id == c_id) {
#ifdef DEBUG_MODE
        if ((int)input_x->size() != feature_num) {
            error("PI_tree_protocol_preprocess: input_x size mismatch");
        }
#endif
        std::vector<ShareValue> x_ext(n, 0);
        for (int i = 0; i < feature_num; i++) {
            x_ext[i] = (*input_x)[i] & mask;
        }
        std::vector<int> inv_opened_pi_p = inv_permutation(opened_pi_p);
        x_perm_plain = apply_permutation(inv_opened_pi_p, x_ext);
    }
    for (int i = 0; i < n; i++) {
        MSSshare_preprocess(c_id, party_id, PRGs, netio, &sh_x[i]);
        ShareValue xv = (party_id == c_id) ? x_perm_plain[i] : 0;
        MSSshare_share_from(c_id, party_id, netio, &sh_x[i], xv);
    }

    intermediate.m = m;
    intermediate.n = n;
    intermediate.I0 = I0;
    intermediate.I1 = I1;
    intermediate.t_perm = std::make_unique<MSSshare_v>(std::move(t_perm));
    intermediate.L_perm = std::make_unique<MSSshare_v>(std::move(L_perm));
    intermediate.R_perm = std::make_unique<MSSshare_v>(std::move(R_perm));
    intermediate.Lp_perm = std::make_unique<MSSshare_v>(std::move(Lp_perm));
    intermediate.Rp_perm = std::make_unique<MSSshare_v>(std::move(Rp_perm));
    intermediate.sh_x = std::make_unique<MSSshare_v>(std::move(sh_x));

    int rounds = d - 1;
    if (rounds < 0) {
        rounds = 0;
    }
    intermediate.cotp_inters.clear();
    intermediate.cotp_inters.reserve(rounds);
#ifdef USE_DICF
    for (int iter = 0; iter < rounds; iter++) {
        intermediate.cotp_inters.emplace_back(ell);
        PI_cotp_DICF_preprocess(party_id, PRGs, netio, intermediate.cotp_inters.back());
    }
#else
    std::vector<PI_cotp_intermediate *> cotp_inter_ptrs;
    cotp_inter_ptrs.reserve(rounds);
    for (int iter = 0; iter < rounds; iter++) {
        intermediate.cotp_inters.emplace_back(ell, 2);
        cotp_inter_ptrs.push_back(&intermediate.cotp_inters.back());
    }
    if (!cotp_inter_ptrs.empty()) {
        PI_cotp_preprocess_vec(party_id, PRGs, netio, cotp_inter_ptrs);
    }
#endif

    netio.send_stored_data_all();
}

void PI_tree_protocol(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      PI_tree_protocol_intermediate &intermediate, MSSshare &output_r) {
    int ell = intermediate.ell;
    int d = intermediate.d;
    int m = intermediate.m;
    int n = intermediate.n;
    ShareValue I0 = intermediate.I0;
    ShareValue I1 = intermediate.I1;

#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_tree_protocol: intermediate must be preprocessed before calling");
    }
    if (output_r.BITLEN != ell) {
        error("PI_tree_protocol: output_r bitlen mismatch");
    }
    if (!intermediate.t_perm || !intermediate.L_perm || !intermediate.R_perm ||
        !intermediate.Lp_perm || !intermediate.Rp_perm || !intermediate.sh_x) {
        error("PI_tree_protocol: shared inputs are not initialized");
    }
#endif

    MSSshare_v &t_perm = *intermediate.t_perm;
    MSSshare_v &L_perm = *intermediate.L_perm;
    MSSshare_v &R_perm = *intermediate.R_perm;
    MSSshare_v &Lp_perm = *intermediate.Lp_perm;
    MSSshare_v &Rp_perm = *intermediate.Rp_perm;
    MSSshare_v &sh_x = *intermediate.sh_x;

    MSSshare res(ell);
    res.v1 = 0;
    res.v2 = 0;
#ifdef DEBUG_MODE
    res.has_shared = true;
    res.has_preprocess = true;
#endif

    ShareValue cur_node = I0;
    ShareValue cur_feat = I1;
    int rounds = d - 1;
    if (rounds < 0) {
        rounds = 0;
    }

#ifdef DEBUG_MODE
    if ((int)intermediate.cotp_inters.size() != rounds) {
        error("PI_tree_protocol: COTP intermediate round count mismatch");
    }
#endif

    for (int iter = 0; iter < rounds; iter++) {
#ifdef DEBUG_MODE
        if (cur_node >= (ShareValue)m || cur_feat >= (ShareValue)n) {
            error("PI_tree_protocol: running indices out of range");
        }
#endif
        int node_idx = (int)cur_node;
        int feat_idx = (int)cur_feat;

        res = res + t_perm[node_idx];
#ifdef DEBUG_MODE
        res.has_shared = true;
        res.has_preprocess = true;
#endif

        // COTP语义为(x>=t)?L:R；树路由左分支条件为x<t，因此左右输入对调
        std::vector<MSSshare *> left_vec = {&R_perm[node_idx], &Rp_perm[node_idx]};
        std::vector<MSSshare *> right_vec = {&L_perm[node_idx], &Lp_perm[node_idx]};
#ifdef USE_DICF
        std::vector<ShareValue> opened = PI_cotp_DICF_vec(
            party_id, PRGs, netio, intermediate.cotp_inters[iter], &sh_x[feat_idx],
            &t_perm[node_idx], left_vec, right_vec);
#else
        std::vector<ShareValue> opened =
            PI_cotp(party_id, PRGs, netio, intermediate.cotp_inters[iter], &sh_x[feat_idx],
                    &t_perm[node_idx], left_vec, right_vec);
#endif

#ifdef DEBUG_MODE
        if ((int)opened.size() != 2) {
            error("PI_tree_protocol: COTP output size mismatch");
        }
        if (opened[0] >= (ShareValue)m || opened[1] >= (ShareValue)n) {
            error("PI_tree_protocol: COTP output index out of range");
        }
#endif
        cur_node = opened[0];
        cur_feat = opened[1];
    }

#ifdef DEBUG_MODE
    if (cur_node >= (ShareValue)m) {
        error("PI_tree_protocol: final node index out of range");
    }
#endif
    res = res + t_perm[(int)cur_node];
#ifdef DEBUG_MODE
    res.has_shared = true;
    res.has_preprocess = true;
#endif
    output_r = res;
}
