void PI_strawman(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 const plain_tree *input_tree, const std::vector<ShareValue> *input_x,
                 int feature_num, int d, MSSshare &output_r, int ell) {
    const int s_id = 0;
    const int c_id = 1;

#ifdef DEBUG_MODE
    if (party_id < 0 || party_id > 2) {
        error("PI_strawman: party_id must be in {0,1,2}");
    }
    if (ell <= 0 || ell > ShareValue_BitLength) {
        error("PI_strawman: ell out of range");
    }
    if (output_r.BITLEN != ell) {
        error("PI_strawman: output_r bitlen mismatch");
    }
    if (feature_num <= 0 || d <= 0) {
        error("PI_strawman: feature_num and d must be positive");
    }
    if (party_id == s_id && input_tree == nullptr) {
        error("PI_strawman: P0 must provide input_tree");
    }
    if (party_id == c_id && input_x == nullptr) {
        error("PI_strawman: P1 must provide input_x");
    }
#endif

    ShareValue mask = (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);

    int dummy_cnt = (d >= 2) ? (d - 2) : 0;
    int feat_total = feature_num + dummy_cnt;
    ShareValue I0 = 0, I1 = 0;

    std::vector<ShareValue> plain_t, plain_L, plain_R, plain_Lp, plain_Rp;
    int node_total = 0;
    std::vector<int> pi_node;
    std::vector<int> pi_feat;

    // P0准备模型：prepare + 生成pi/pi' + 重写索引 + 打乱节点数组 + 公开(I0,I1)
    if (party_id == s_id) {
        plain_tree work = *input_tree;
        work.prepare(feature_num);

        node_total = work.size();
#ifdef DEBUG_MODE
        int expected_node_total = ((int)input_tree->size()) + dummy_cnt;
        if (node_total != expected_node_total) {
            error("PI_strawman: node_total mismatch with provided d");
        }
#endif
        plain_t = work.t;
        plain_L = work.L;
        plain_R = work.R;
        plain_Lp = work.Lp;
        plain_Rp = work.Rp;

        block private_seed = gen_seed();
        PRGSync private_prg(&private_seed);
        pi_node = gen_random_permutation(node_total, private_prg);

        // 固定角色下，P0与P1共享PRGs[0]，用于生成同一个pi'
        pi_feat = gen_random_permutation(feat_total, PRGs[0]);
        std::vector<int> inv_node = inv_permutation(pi_node);
        std::vector<int> inv_feat = inv_permutation(pi_feat);

        for (int i = 0; i < node_total; i++) {
            ShareValue oldL = plain_L[i], oldR = plain_R[i], oldLp = plain_Lp[i], oldRp = plain_Rp[i];
#ifdef DEBUG_MODE
            if (oldL > (ShareValue)node_total || oldR > (ShareValue)node_total) {
                error("PI_strawman: child node index out of allowed range before permutation");
            }
            if (oldLp > (ShareValue)feat_total || oldRp > (ShareValue)feat_total) {
                error("PI_strawman: feature index out of allowed range before permutation");
            }
#endif
            // 允许prepare的“最后一个dummy哨兵索引”等于node_total/feat_total；
            // 该索引不会在协议执行路径上被真正解引用，这里映射到有效自环位以便完成置换重写。
            plain_L[i] = (oldL < (ShareValue)node_total) ? (ShareValue)inv_node[(int)oldL]
                                                          : (ShareValue)inv_node[0];
            plain_R[i] = (oldR < (ShareValue)node_total) ? (ShareValue)inv_node[(int)oldR]
                                                          : (ShareValue)inv_node[0];
            plain_Lp[i] = (oldLp < (ShareValue)feat_total) ? (ShareValue)inv_feat[(int)oldLp]
                                                            : (ShareValue)inv_feat[0];
            plain_Rp[i] = (oldRp < (ShareValue)feat_total) ? (ShareValue)inv_feat[(int)oldRp]
                                                            : (ShareValue)inv_feat[0];
        }

        I0 = (ShareValue)inv_node[0];
        I1 = (ShareValue)inv_feat[(int)work.alpha];

        plain_t = apply_permutation(pi_node, plain_t);
        plain_L = apply_permutation(pi_node, plain_L);
        plain_R = apply_permutation(pi_node, plain_R);
        plain_Lp = apply_permutation(pi_node, plain_Lp);
        plain_Rp = apply_permutation(pi_node, plain_Rp);

        for (int dst = 1; dst <= 2; dst++) {
            netio.send_data(dst, &node_total, sizeof(int));
            netio.send_data(dst, &I0, sizeof(ShareValue));
            netio.send_data(dst, &I1, sizeof(ShareValue));
        }
    } else {
        netio.recv_data(s_id, &node_total, sizeof(int));
        netio.recv_data(s_id, &I0, sizeof(ShareValue));
        netio.recv_data(s_id, &I1, sizeof(ShareValue));
    }

#ifdef DEBUG_MODE
    if (node_total <= 0 || feat_total <= 0) {
        error("PI_strawman: invalid public sizes");
    }
    if (I0 >= (ShareValue)node_total || I1 >= (ShareValue)feat_total) {
        error("PI_strawman: initial index out of range");
    }
#endif

    // P1生成同一个pi'并本地打乱特征，随后共享给三方
    std::vector<ShareValue> x_perm_plain;
    if (party_id == c_id) {
        pi_feat = gen_random_permutation(feat_total, PRGs[0]);
#ifdef DEBUG_MODE
        if ((int)input_x->size() != feature_num) {
            error("PI_strawman: input_x size mismatch with feature_num");
        }
#endif
        std::vector<ShareValue> x_ext(feat_total, 0);
        for (int i = 0; i < feature_num; i++) {
            x_ext[i] = (*input_x)[i] & mask;
        }
        x_perm_plain = apply_permutation(pi_feat, x_ext);
    }

    // 共享节点数组
    MSSshare_v sh_t(node_total, ell), sh_L(node_total, ell), sh_R(node_total, ell), sh_Lp(node_total, ell),
        sh_Rp(node_total, ell);
    for (int i = 0; i < node_total; i++) {
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_t[i]);
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_L[i]);
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_R[i]);
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_Lp[i]);
        MSSshare_preprocess(s_id, party_id, PRGs, netio, &sh_Rp[i]);

        ShareValue v_t = (party_id == s_id) ? (plain_t[i] & mask) : 0;
        ShareValue v_L = (party_id == s_id) ? (plain_L[i] & mask) : 0;
        ShareValue v_R = (party_id == s_id) ? (plain_R[i] & mask) : 0;
        ShareValue v_Lp = (party_id == s_id) ? (plain_Lp[i] & mask) : 0;
        ShareValue v_Rp = (party_id == s_id) ? (plain_Rp[i] & mask) : 0;

        MSSshare_share_from(s_id, party_id, netio, &sh_t[i], v_t);
        MSSshare_share_from(s_id, party_id, netio, &sh_L[i], v_L);
        MSSshare_share_from(s_id, party_id, netio, &sh_R[i], v_R);
        MSSshare_share_from(s_id, party_id, netio, &sh_Lp[i], v_Lp);
        MSSshare_share_from(s_id, party_id, netio, &sh_Rp[i], v_Rp);
    }

    // 共享特征数组
    MSSshare_v sh_x(feat_total, ell);
    for (int i = 0; i < feat_total; i++) {
        MSSshare_preprocess(c_id, party_id, PRGs, netio, &sh_x[i]);
        ShareValue xv = (party_id == c_id) ? x_perm_plain[i] : 0;
        MSSshare_share_from(c_id, party_id, netio, &sh_x[i], xv);
    }

    // 逐层评估
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

    for (int iter = 0; iter < rounds; iter++) {
#ifdef DEBUG_MODE
        if (cur_node >= (ShareValue)node_total || cur_feat >= (ShareValue)feat_total) {
            error("PI_strawman: running index out of range");
        }
#endif
        int node_idx = (int)cur_node;
        int feat_idx = (int)cur_feat;

        res = res + sh_t[node_idx];
#ifdef DEBUG_MODE
        res.has_shared = true;
        res.has_preprocess = true;
#endif

        std::vector<MSSshare *> left_vec = {&sh_R[node_idx], &sh_Rp[node_idx]};
        std::vector<MSSshare *> right_vec = {&sh_L[node_idx], &sh_Lp[node_idx]};
        PI_cotp_intermediate cotp_inter(ell, 2);
        PI_cotp_preprocess(party_id, PRGs, netio, cotp_inter);
        netio.send_stored_data_all();
        std::vector<ShareValue> opened =
            PI_cotp(party_id, PRGs, netio, cotp_inter, &sh_x[feat_idx], &sh_t[node_idx], left_vec,
                    right_vec);

#ifdef DEBUG_MODE
        if ((int)opened.size() != 2) {
            error("PI_strawman: COTP output size mismatch");
        }
        if (opened[0] >= (ShareValue)node_total || opened[1] >= (ShareValue)feat_total) {
            error("PI_strawman: COTP output index out of range");
        }
#endif
        cur_node = opened[0];
        cur_feat = opened[1];
    }

#ifdef DEBUG_MODE
    if (cur_node >= (ShareValue)node_total) {
        error("PI_strawman: final node index out of range");
    }
#endif
    res = res + sh_t[(int)cur_node];
#ifdef DEBUG_MODE
    res.has_shared = true;
    res.has_preprocess = true;
#endif

    output_r = res;
}
