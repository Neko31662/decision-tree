void PI_strawman_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                            PI_strawman_intermediate &intermediate, const plain_tree *input_tree,
                            const std::vector<ShareValue> *input_x) {
    const int s_id = 0;
    const int c_id = 1;
    const int ell = intermediate.ell;
    const int feature_num = intermediate.feature_num;
    const int d = intermediate.d;

#ifdef DEBUG_MODE
    if (party_id < 0 || party_id > 2) {
        error("PI_strawman_preprocess: party_id must be in {0,1,2}");
    }
    if (ell <= 0 || ell > ShareValue_BitLength) {
        error("PI_strawman_preprocess: ell out of range");
    }
    if (feature_num <= 0 || d <= 0) {
        error("PI_strawman_preprocess: feature_num and d must be positive");
    }
    if (party_id == s_id && input_tree == nullptr) {
        error("PI_strawman_preprocess: P0 must provide input_tree");
    }
    if (party_id == c_id && input_x == nullptr) {
        error("PI_strawman_preprocess: P1 must provide input_x");
    }
    if (intermediate.has_preprocess) {
        error("PI_strawman_preprocess has already been called on this object");
    }
    intermediate.has_preprocess = true;
#endif

    ShareValue mask = (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
    int dummy_cnt = (d >= 2) ? (d - 2) : 0;
    int feat_total = feature_num + dummy_cnt;

    // S/C在预处理阶段同步公开长度信息（直接发送，不走store）
    int feature_num_from_s = feature_num;
    int d_from_s = d;
    if (party_id == s_id) {
        netio.send_data(1, &feature_num_from_s, sizeof(int));
        netio.send_data(2, &feature_num_from_s, sizeof(int));
        netio.send_data(1, &d_from_s, sizeof(int));
        netio.send_data(2, &d_from_s, sizeof(int));
    } else {
        netio.recv_data(s_id, &feature_num_from_s, sizeof(int));
        netio.recv_data(s_id, &d_from_s, sizeof(int));
    }

    int x_len_from_c = feature_num;
    if (party_id == c_id) {
        x_len_from_c = (int)input_x->size();
        netio.send_data(0, &x_len_from_c, sizeof(int));
        netio.send_data(2, &x_len_from_c, sizeof(int));
    } else {
        netio.recv_data(c_id, &x_len_from_c, sizeof(int));
    }

#ifdef DEBUG_MODE
    if (feature_num_from_s != feature_num || d_from_s != d) {
        error("PI_strawman_preprocess: public feature_num/d mismatch");
    }
    if (x_len_from_c != feature_num) {
        error("PI_strawman_preprocess: C feature vector length mismatch");
    }
#endif

    ShareValue I0 = 0, I1 = 0;
    int node_total = 0;
    std::vector<ShareValue> plain_t, plain_L, plain_R, plain_Lp, plain_Rp;
    std::vector<int> pi_feat;

    // P0执行模型侧本地预处理：生成pi/pi'、改写索引、重排节点
    if (party_id == s_id) {
        const plain_tree &work = *input_tree;
        node_total = work.size();
#ifdef DEBUG_MODE
        if (!work.prepared) {
            error("PI_strawman_preprocess: input_tree must be prepared before calling");
        }
#endif
        plain_t = work.t;
        plain_L = work.L;
        plain_R = work.R;
        plain_Lp = work.Lp;
        plain_Rp = work.Rp;

        block private_seed = gen_seed();
        PRGSync private_prg(&private_seed);
        std::vector<int> pi_node = gen_random_permutation(node_total, private_prg);

        // 固定角色下，P0与P1通过PRGs[0]生成相同pi'
        pi_feat = gen_random_permutation(feat_total, PRGs[0]);
        std::vector<int> inv_node = inv_permutation(pi_node);
        std::vector<int> inv_feat = inv_permutation(pi_feat);

        for (int i = 0; i < node_total; i++) {
            ShareValue oldL = plain_L[i], oldR = plain_R[i], oldLp = plain_Lp[i], oldRp = plain_Rp[i];
#ifdef DEBUG_MODE
            if (oldL > (ShareValue)node_total || oldR > (ShareValue)node_total) {
                error("PI_strawman_preprocess: child node index out of range before permutation");
            }
            if (oldLp > (ShareValue)feat_total || oldRp > (ShareValue)feat_total) {
                error("PI_strawman_preprocess: feature index out of range before permutation");
            }
#endif
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

        netio.send_data(1, &node_total, sizeof(int));
        netio.send_data(2, &node_total, sizeof(int));
        netio.send_data(1, &I0, sizeof(ShareValue));
        netio.send_data(2, &I0, sizeof(ShareValue));
        netio.send_data(1, &I1, sizeof(ShareValue));
        netio.send_data(2, &I1, sizeof(ShareValue));
    } else {
        netio.recv_data(s_id, &node_total, sizeof(int));
        netio.recv_data(s_id, &I0, sizeof(ShareValue));
        netio.recv_data(s_id, &I1, sizeof(ShareValue));
    }

#ifdef DEBUG_MODE
    if (node_total <= 0 || feat_total <= 0) {
        error("PI_strawman_preprocess: invalid public sizes");
    }
    if (I0 >= (ShareValue)node_total || I1 >= (ShareValue)feat_total) {
        error("PI_strawman_preprocess: initial index out of range");
    }
#endif

    // P1执行客户端本地预处理：同pi'打乱特征并补零
    std::vector<ShareValue> x_perm_plain;
    if (party_id == c_id) {
        pi_feat = gen_random_permutation(feat_total, PRGs[0]);
        std::vector<ShareValue> x_ext(feat_total, 0);
        for (int i = 0; i < feature_num; i++) {
            x_ext[i] = (*input_x)[i] & mask;
        }
        x_perm_plain = apply_permutation(pi_feat, x_ext);
    }

    intermediate.node_total = node_total;
    intermediate.feat_total = feat_total;
    intermediate.I0 = I0;
    intermediate.I1 = I1;

    intermediate.sh_t = std::make_unique<MSSshare_v>(node_total, ell);
    intermediate.sh_L = std::make_unique<MSSshare_v>(node_total, ell);
    intermediate.sh_R = std::make_unique<MSSshare_v>(node_total, ell);
    intermediate.sh_Lp = std::make_unique<MSSshare_v>(node_total, ell);
    intermediate.sh_Rp = std::make_unique<MSSshare_v>(node_total, ell);
    intermediate.sh_x = std::make_unique<MSSshare_v>(feat_total, ell);

    MSSshare_v &sh_t = *intermediate.sh_t;
    MSSshare_v &sh_L = *intermediate.sh_L;
    MSSshare_v &sh_R = *intermediate.sh_R;
    MSSshare_v &sh_Lp = *intermediate.sh_Lp;
    MSSshare_v &sh_Rp = *intermediate.sh_Rp;
    MSSshare_v &sh_x = *intermediate.sh_x;

    // 预处理并共享节点
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

    // 预处理并共享特征
    for (int i = 0; i < feat_total; i++) {
        MSSshare_preprocess(c_id, party_id, PRGs, netio, &sh_x[i]);
        ShareValue xv = (party_id == c_id) ? x_perm_plain[i] : 0;
        MSSshare_share_from(c_id, party_id, netio, &sh_x[i], xv);
    }

    // 预处理所有轮次的COTP中间体
    int rounds = d - 1;
    if (rounds < 0) {
        rounds = 0;
    }
    intermediate.cotp_inters.clear();
    intermediate.cotp_inters.reserve(rounds);
    for (int iter = 0; iter < rounds; iter++) {
        intermediate.cotp_inters.emplace_back(ell, 2);
        PI_cotp_preprocess(party_id, PRGs, netio, intermediate.cotp_inters.back());
    }

    // 本协议预处理阶段可能写入了store缓冲，统一冲刷
    netio.send_stored_data_all();
}

void PI_strawman(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 PI_strawman_intermediate &intermediate, MSSshare &output_r) {
    int ell = intermediate.ell;
    int d = intermediate.d;
    int node_total = intermediate.node_total;
    int feat_total = intermediate.feat_total;
    ShareValue I0 = intermediate.I0;
    ShareValue I1 = intermediate.I1;

#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_strawman: intermediate must be preprocessed before calling");
    }
    if (output_r.BITLEN != ell) {
        error("PI_strawman: output_r bitlen mismatch");
    }
    if (!intermediate.sh_t || !intermediate.sh_L || !intermediate.sh_R || !intermediate.sh_Lp ||
        !intermediate.sh_Rp || !intermediate.sh_x) {
        error("PI_strawman: shared inputs are not initialized");
    }
#endif

    MSSshare_v &sh_t = *intermediate.sh_t;
    MSSshare_v &sh_L = *intermediate.sh_L;
    MSSshare_v &sh_R = *intermediate.sh_R;
    MSSshare_v &sh_Lp = *intermediate.sh_Lp;
    MSSshare_v &sh_Rp = *intermediate.sh_Rp;
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
        error("PI_strawman: COTP intermediate round count mismatch");
    }
#endif

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
        std::vector<ShareValue> opened =
            PI_cotp(party_id, PRGs, netio, intermediate.cotp_inters[iter], &sh_x[feat_idx],
                    &sh_t[node_idx], left_vec, right_vec);

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
