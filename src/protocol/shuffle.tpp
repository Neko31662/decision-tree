namespace pi_shuffle_detail {

/* 根据参与方与对端id，返回PRGs中对应的索引
 * @param party_id: 当前参与方id，0/1/2
 * @param peer_id: 对端参与方id
 */
inline int prg_idx_with_peer(int party_id, int peer_id) {
    if (party_id == 0) {
        if (peer_id == 1)
            return 0;
        if (peer_id == 2)
            return 1;
    } else if (party_id == 1) {
        if (peer_id == 0)
            return 0;
        if (peer_id == 2)
            return 1;
    } else if (party_id == 2) {
        if (peer_id == 0)
            return 0;
        if (peer_id == 1)
            return 1;
    }
    error("PI_shuffle: invalid party_id/peer_id combination");
    return -1;
}

/* 执行安全混洗协议中的“一轮置换+加法转MSS”
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param in_vec: 本轮输入共享向量
 * @param out_vec: 本轮输出共享向量
 * @param party_a: 本轮第一个主执行方（持有加法分量a）
 * @param party_b: 本轮第二个主执行方（持有加法分量b）
 * @param party_c: 本轮辅助方（不知晓本轮置换）
 * @param pi_hint: 可选置换；若非空则使用该置换，否则本轮内部随机生成
 */
inline void shuffle_round(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                          MSSshare_v &in_vec, MSSshare_v &out_vec, int party_a, int party_b,
                          int party_c, const std::vector<int> *pi_hint = nullptr) {
    int len = in_vec.size();
    ShareValue mask = in_vec.MASK;
    int bytem = in_vec.BYTELEN;

#ifdef DEBUG_MODE
    if (in_vec.size() != out_vec.size()) {
        error("PI_shuffle: shuffle_round input/output size mismatch");
    }
    if (in_vec.BITLEN != out_vec.BITLEN) {
        error("PI_shuffle: shuffle_round input/output bitlen mismatch");
    }
#endif

    std::vector<ShareValue> add_share(len, 0);
    std::vector<ShareValue> comp_a(len, 0);
    std::vector<ShareValue> comp_b(len, 0);
    std::vector<ShareValue> comp_c(len, 0);
    std::vector<ShareValue> delta_local(len, 0);

    // A、B先把本轮输入看作2方加法分享，然后对加法分享做同一置换
    if (party_id == party_a || party_id == party_b) {
        const std::vector<int> *use_pi = pi_hint;
        std::vector<int> local_pi;
        if (use_pi == nullptr) {
            int peer = (party_id == party_a) ? party_b : party_a;
            int pi_prg_idx = prg_idx_with_peer(party_id, peer);
            local_pi = gen_random_permutation(len, PRGs[pi_prg_idx]);
            use_pi = &local_pi;
        }
#ifdef DEBUG_MODE
        if ((int)use_pi->size() != len) {
            error("PI_shuffle: permutation length mismatch in shuffle_round");
        }
#endif

        if (party_id == party_a) {
            for (int i = 0; i < len; i++) {
                add_share[i] = (in_vec[i].x_prev() + in_vec[i].x_nxt()) & mask;
            }
        } else {
            for (int i = 0; i < len; i++) {
                add_share[i] = in_vec[i].x_prev() & mask;
            }
        }
        add_share = apply_permutation(*use_pi, add_share);
    }

    // 采样转回MSS时需要的随机分量
    if (party_id == party_a || party_id == party_c) {
        int peer = (party_id == party_a) ? party_c : party_a;
        int prg_idx = prg_idx_with_peer(party_id, peer);
        for (int i = 0; i < len; i++) {
            PRGs[prg_idx].gen_random_data(&comp_b[i], bytem);
            comp_b[i] &= mask;
        }
    }

    if (party_id == party_b || party_id == party_c) {
        int peer = (party_id == party_b) ? party_c : party_b;
        int prg_idx = prg_idx_with_peer(party_id, peer);
        for (int i = 0; i < len; i++) {
            PRGs[prg_idx].gen_random_data(&comp_a[i], bytem);
            comp_a[i] &= mask;
        }
    }

    // A与B互发差值，恢复第三个分量
    if (party_id == party_a) {
        for (int i = 0; i < len; i++) {
            delta_local[i] = (add_share[i] - comp_b[i]) & mask;
            netio.store_data(party_b, &delta_local[i], bytem);
        }
        netio.send_stored_data(party_b);

        for (int i = 0; i < len; i++) {
            ShareValue delta_peer = 0;
            netio.recv_data(party_b, &delta_peer, bytem);
            comp_c[i] = (delta_local[i] + delta_peer) & mask;
        }
    } else if (party_id == party_b) {
        for (int i = 0; i < len; i++) {
            delta_local[i] = (add_share[i] - comp_a[i]) & mask;
            netio.store_data(party_a, &delta_local[i], bytem);
        }
        netio.send_stored_data(party_a);

        for (int i = 0; i < len; i++) {
            ShareValue delta_peer = 0;
            netio.recv_data(party_a, &delta_peer, bytem);
            comp_c[i] = (delta_local[i] + delta_peer) & mask;
        }
    }

    // 写回本轮输出的MSS分量（通过x_prev()/x_nxt()屏蔽了party视角差异）
    for (int i = 0; i < len; i++) {
        if (party_id == party_a) {
            // party_a输出 (t_a, t_c)
            out_vec[i].x_prev() = comp_c[i];
            out_vec[i].x_nxt() = comp_b[i];
        } else if (party_id == party_b) {
            // party_b输出 (t_b, t_a)
            out_vec[i].x_prev() = comp_a[i];
            out_vec[i].x_nxt() = comp_c[i];
        } else if (party_id == party_c) {
            // party_c输出 (t_c, t_b)
            out_vec[i].x_prev() = comp_b[i];
            out_vec[i].x_nxt() = comp_a[i];
        } else {
            error("PI_shuffle: invalid party_id in shuffle_round");
        }
#ifdef DEBUG_MODE
        out_vec[i].has_shared = true;
        out_vec[i].has_preprocess = true;
#endif
    }
}

/* 按轮次角色，生成一轮需要同步复用的置换
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param len: 向量长度
 * @param party_a: 本轮第一个主执行方
 * @param party_b: 本轮第二个主执行方
 * @param pi: 输出置换；仅party_a/party_b会填充
 */
inline void gen_shared_round_permutation(const int party_id, std::vector<PRGSync> &PRGs, int len,
                                         int party_a, int party_b, std::vector<int> &pi) {
    if (party_id == party_a || party_id == party_b) {
        int peer = (party_id == party_a) ? party_b : party_a;
        int pi_prg_idx = prg_idx_with_peer(party_id, peer);
        pi = gen_random_permutation(len, PRGs[pi_prg_idx]);
    } else {
        pi.clear();
    }
}

} // namespace pi_shuffle_detail

void PI_shuffle(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio, MSSshare_v &x_vec,
                MSSshare_v &y_vec) {
    int len = x_vec.size();
#ifdef DEBUG_MODE
    if (len != y_vec.size()) {
        error("PI_shuffle: input/output vector size mismatch");
    }
    if (x_vec.BITLEN != y_vec.BITLEN) {
        error("PI_shuffle: input/output bitlen mismatch");
    }
    for (int i = 0; i < len; i++) {
        if (!x_vec[i].has_shared) {
            error("PI_shuffle: input must be shared before calling");
        }
    }
#endif

    int ell = x_vec.BITLEN;
    MSSshare_v stage1(len, ell);
    MSSshare_v stage2(len, ell);

    pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, x_vec, stage1, 0, 1, 2);
    pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, stage1, stage2, 1, 2, 0);
    pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, stage2, y_vec, 2, 0, 1);
}

void PI_shuffle(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio, MSSshare_v &x_vec) {
    MSSshare_v out_vec(x_vec.size(), x_vec.BITLEN);
    PI_shuffle(party_id, PRGs, netio, x_vec, out_vec);
    x_vec = out_vec;
}

void PI_shuffle_sync(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     std::vector<MSSshare_v *> &x_vec_list, std::vector<MSSshare_v *> &y_vec_list) {
    int obj_num = (int)x_vec_list.size();
#ifdef DEBUG_MODE
    if (obj_num != (int)y_vec_list.size()) {
        error("PI_shuffle_sync: input/output object count mismatch");
    }
#endif
    if (obj_num == 0) {
        return;
    }

#ifdef DEBUG_MODE
    for (int k = 0; k < obj_num; k++) {
        if (x_vec_list[k] == nullptr || y_vec_list[k] == nullptr) {
            error("PI_shuffle_sync: null pointer in input/output list");
        }
    }
#endif

    int len = x_vec_list[0]->size();

#ifdef DEBUG_MODE
    for (int k = 0; k < obj_num; k++) {
        if (x_vec_list[k]->size() != len || y_vec_list[k]->size() != len) {
            error("PI_shuffle_sync: all objects must have same vector length");
        }
        if (x_vec_list[k]->BITLEN != y_vec_list[k]->BITLEN) {
            error("PI_shuffle_sync: input/output bitlen mismatch at object " + std::to_string(k));
        }
        for (int i = 0; i < len; i++) {
            if (!(*x_vec_list[k])[i].has_shared) {
                error("PI_shuffle_sync: all inputs must be shared before calling");
            }
        }
    }
#endif

    // 每一轮只生成一次置换，然后复用于所有对象
    std::vector<int> pi_round0, pi_round1, pi_round2;
    pi_shuffle_detail::gen_shared_round_permutation(party_id, PRGs, len, 0, 1, pi_round0);
    pi_shuffle_detail::gen_shared_round_permutation(party_id, PRGs, len, 1, 2, pi_round1);
    pi_shuffle_detail::gen_shared_round_permutation(party_id, PRGs, len, 2, 0, pi_round2);

    for (int k = 0; k < obj_num; k++) {
        MSSshare_v &in_vec = *x_vec_list[k];
        MSSshare_v &out_vec = *y_vec_list[k];
        int ell = in_vec.BITLEN;

        MSSshare_v stage1(len, ell);
        MSSshare_v stage2(len, ell);
        pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, in_vec, stage1, 0, 1, 2, &pi_round0);
        pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, stage1, stage2, 1, 2, 0, &pi_round1);
        pi_shuffle_detail::shuffle_round(party_id, PRGs, netio, stage2, out_vec, 2, 0, 1, &pi_round2);
    }
}

void PI_shuffle_sync(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     std::vector<MSSshare_v *> &x_vec_list) {
    int obj_num = (int)x_vec_list.size();
    if (obj_num == 0) {
        return;
    }

    std::vector<MSSshare_v> out_storage;
    out_storage.reserve(obj_num);
    std::vector<MSSshare_v *> out_ptrs;
    out_ptrs.reserve(obj_num);

    for (int k = 0; k < obj_num; k++) {
#ifdef DEBUG_MODE
        if (x_vec_list[k] == nullptr) {
            error("PI_shuffle_sync: null pointer in input list");
        }
#endif
        out_storage.emplace_back(x_vec_list[k]->size(), x_vec_list[k]->BITLEN);
        out_ptrs.push_back(&out_storage.back());
    }

    PI_shuffle_sync(party_id, PRGs, netio, x_vec_list, out_ptrs);
    for (int k = 0; k < obj_num; k++) {
        *x_vec_list[k] = out_storage[k];
    }
}

