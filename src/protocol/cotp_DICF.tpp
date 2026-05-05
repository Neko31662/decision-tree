namespace pi_cotp_dicf_detail {

inline ShareValue mod_mask(ShareValue x, ShareValue mask) { return x & mask; }

inline int4 random_int4_from_prg(PRGSync &prg) {
    int4 out{0, 0, 0, 0};
    prg.gen_random_data(&out, sizeof(int4));
    out.w &= ~1;
    return out;
}

inline ShareValue random_share_from_prg(PRGSync &prg, int bytelength, ShareValue mask) {
    ShareValue out = 0;
    prg.gen_random_data(&out, bytelength);
    return out & mask;
}

inline ShareValue int4_to_share(const int4 &v) { return static_cast<ShareValue>(static_cast<uint32_t>(v.x)); }

inline ShareValue local_part_for_dealer(int dealer_id, int party_id, const MSSshare &s_diff) {
    // s_diff 使用 RSS 表示：P0=(a,b), P1=(c,a), P2=(c,b), 且 s=a+b+c
    // 对每个 dealer j，评估方 (j+1,j+2) 使用分解 p0+p1=s，避免公开 s。
    if (dealer_id == 0) {
        if (party_id == 1) {
            return s_diff.v1 + s_diff.v2; // c+a
        }
        if (party_id == 2) {
            return s_diff.v2; // b
        }
    } else if (dealer_id == 1) {
        if (party_id == 2) {
            return s_diff.v1 + s_diff.v2; // c+b
        }
        if (party_id == 0) {
            return s_diff.v1; // a
        }
    } else { // dealer_id == 2
        if (party_id == 0) {
            return s_diff.v1 + s_diff.v2; // a+b
        }
        if (party_id == 1) {
            return s_diff.v1; // c
        }
    }
    return 0;
}

inline ShareValue local_additive_msg_component(int party_id, const MSSshare &msg) {
    // 将 RSS 映射为 3 方加法 share:
    // x^(0)=a, x^(1)=c, x^(2)=b
    if (party_id == 0) {
        return msg.v1;
    }
    if (party_id == 1) {
        return msg.v1;
    }
    return msg.v2;
}

} // namespace pi_cotp_dicf_detail

void PI_cotp_DICF_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                             PI_cotp_DICF_intermediate &intermediate) {
    (void)PRGs;
    const int ell = intermediate.ell;
    const ShareValue mask = intermediate.mask;
    const ShareValue tau = intermediate.tau;
    const int bytem = (ell + 7) / 8;

#ifdef DEBUG_MODE
    if (ell <= 1 || ell > ShareValue_BitLength) {
        error("PI_cotp_DICF_preprocess: invalid ell");
    }
    if (intermediate.has_preprocess) {
        error("PI_cotp_DICF_preprocess has already been called");
    }
#endif

    block local_seed = gen_seed();
    PRGSync local_prg(&local_seed);

    for (int dealer = 0; dealer < 3; dealer++) {
        int eval0 = (dealer + 1) % 3;
        int eval1 = (dealer + 2) % 3;

        if (party_id == dealer) {
            ShareValue rho = pi_cotp_dicf_detail::random_share_from_prg(local_prg, bytem, mask);
            ShareValue rho0 = pi_cotp_dicf_detail::random_share_from_prg(local_prg, bytem, mask);
            ShareValue rho1 = (rho - rho0) & mask;

            int4 seeds_a[2] = {pi_cotp_dicf_detail::random_int4_from_prg(local_prg),
                               pi_cotp_dicf_detail::random_int4_from_prg(local_prg)};
            int4 seeds_b[2] = {pi_cotp_dicf_detail::random_int4_from_prg(local_prg),
                               pi_cotp_dicf_detail::random_int4_from_prg(local_prg)};

            PI_cotp_DICF_intermediate::DicfType::Key keys[2];
            intermediate.dicf.GenWithRingBits(keys, seeds_a, seeds_b, 0, tau, rho, ell);

            netio.send_data(eval0, &keys[0], sizeof(keys[0]));
            netio.send_data(eval0, &rho0, bytem);
            netio.send_data(eval1, &keys[1], sizeof(keys[1]));
            netio.send_data(eval1, &rho1, bytem);
        } else if (party_id == eval0 || party_id == eval1) {
            auto &st = intermediate.dealer_eval[dealer];
            netio.recv_data(dealer, &st.key, sizeof(st.key));
            netio.recv_data(dealer, &st.rho_share, bytem);
            st.rho_share &= mask;
            st.has_eval = true;
            st.eval_party_is_one = (party_id == eval1);
        }
    }

#ifdef DEBUG_MODE
    intermediate.has_preprocess = true;
#endif
}

ShareValue PI_cotp_DICF(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_cotp_DICF_intermediate &intermediate, MSSshare *input_x,
                        MSSshare *input_y, MSSshare *input_L, MSSshare *input_R) {
    std::vector<MSSshare *> L_vec = {input_L};
    std::vector<MSSshare *> R_vec = {input_R};
    std::vector<ShareValue> out = PI_cotp_DICF_vec(party_id, PRGs, netio, intermediate, input_x, input_y,
                                                   L_vec, R_vec);
    return out[0];
}

std::vector<ShareValue> PI_cotp_DICF_vec(const int party_id, std::vector<PRGSync> &PRGs,
                                         NetIOMP &netio, PI_cotp_DICF_intermediate &intermediate,
                                         MSSshare *input_x, MSSshare *input_y,
                                         std::vector<MSSshare *> &input_L_vec,
                                         std::vector<MSSshare *> &input_R_vec) {
    (void)PRGs;
    const int ell = intermediate.ell;
    const ShareValue mask = intermediate.mask;
    const int bytem = (ell + 7) / 8;
    const int vec_size = static_cast<int>(input_L_vec.size());

#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_cotp_DICF_vec: intermediate must be preprocessed");
    }
    if (!input_x->has_shared || !input_y->has_shared) {
        error("PI_cotp_DICF_vec: input_x/input_y must be shared");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
        error("PI_cotp_DICF_vec: input_x/input_y bitlen mismatch");
    }
    if ((int)input_R_vec.size() != vec_size || vec_size <= 0) {
        error("PI_cotp_DICF_vec: input vector size mismatch");
    }
    for (int i = 0; i < vec_size; i++) {
        if (!input_L_vec[i]->has_shared || !input_R_vec[i]->has_shared) {
            error("PI_cotp_DICF_vec: input L/R vectors must be shared");
        }
        if (input_L_vec[i]->BITLEN != ell || input_R_vec[i]->BITLEN != ell) {
            error("PI_cotp_DICF_vec: input L/R bitlen mismatch");
        }
    }
#endif

    // m1-m0 = x-y，对应 b = 1[x >= y]（区间 [0, 2^{ell-1}-1]）
    MSSshare diff = *input_x - *input_y;
#ifdef DEBUG_MODE
    diff.has_shared = true;
#endif

    ShareValue beta_from_dealer[3] = {0, 0, 0};

    // --- 通信轮 1：评估方两两交换 masked partial sum，得到 delta_j=(x-y)+rho_j ---
    for (int dealer = 0; dealer < 3; dealer++) {
        auto &st = intermediate.dealer_eval[dealer];
        if (!st.has_eval) {
            continue;
        }
        int eval0 = (dealer + 1) % 3;
        int eval1 = (dealer + 2) % 3;
        int peer = (party_id == eval0) ? eval1 : eval0;

        ShareValue part = pi_cotp_dicf_detail::local_part_for_dealer(dealer, party_id, diff) & mask;
        ShareValue d_local = (part + st.rho_share) & mask;
        netio.store_data(peer, &d_local, bytem);
    }
    netio.send_stored_data_all();

    for (int dealer = 0; dealer < 3; dealer++) {
        auto &st = intermediate.dealer_eval[dealer];
        if (!st.has_eval) {
            continue;
        }
        int eval0 = (dealer + 1) % 3;
        int eval1 = (dealer + 2) % 3;
        int peer = (party_id == eval0) ? eval1 : eval0;

        ShareValue part = pi_cotp_dicf_detail::local_part_for_dealer(dealer, party_id, diff) & mask;
        ShareValue d_local = (part + st.rho_share) & mask;
        ShareValue d_peer = 0;
        netio.recv_data(peer, &d_peer, bytem);
        d_peer &= mask;
        ShareValue delta = (d_local + d_peer) & mask;

        int4 beta = intermediate.dicf.Eval(st.eval_party_is_one, st.key,
                                           static_cast<uint32_t>(delta));
        beta_from_dealer[dealer] = pi_cotp_dicf_detail::int4_to_share(beta) & mask;
    }

    // --- 按 CSOT 语义设置消息：x0=L, x1=R，最终输出 x_{1-b} ---
    std::vector<ShareValue> x0_self(vec_size, 0), x1_self(vec_size, 0);
    std::vector<ShareValue> x0_next(vec_size, 0), x1_next(vec_size, 0);
    for (int i = 0; i < vec_size; i++) {
        x0_self[i] = pi_cotp_dicf_detail::local_additive_msg_component(party_id, *input_L_vec[i]) & mask;
        x1_self[i] = pi_cotp_dicf_detail::local_additive_msg_component(party_id, *input_R_vec[i]) & mask;
    }

    // --- 通信轮 2：循环发送 transformed message share ---
    int dst = (party_id + 2) % 3; // 0->2,1->0,2->1
    int src = (party_id + 1) % 3; // 0<-1,1<-2,2<-0
    for (int i = 0; i < vec_size; i++) {
        netio.store_data(dst, &x0_self[i], bytem);
        netio.store_data(dst, &x1_self[i], bytem);
    }
    netio.send_stored_data(dst);
    for (int i = 0; i < vec_size; i++) {
        netio.recv_data(src, &x0_next[i], bytem);
        netio.recv_data(src, &x1_next[i], bytem);
        x0_next[i] &= mask;
        x1_next[i] &= mask;
    }

    // 每个 P_j 都持有 beta_{j+1}^{(1)} 与 beta_{j+2}^{(0)}
    ShareValue beta_a = beta_from_dealer[(party_id + 1) % 3] & mask; // beta_{j+1}^{(1)}
    ShareValue beta_b = beta_from_dealer[(party_id + 2) % 3] & mask; // beta_{j+2}^{(0)}

    // y^(j) = x1^(j) + Σ_{q=0}^1 Σ_{k=0}^1 (-1)^k * xk^(j+q) * beta_{j+1+q}^{(1-q)}
    std::vector<ShareValue> y_local(vec_size, 0);
    for (int i = 0; i < vec_size; i++) {
        y_local[i] = x1_self[i];
        y_local[i] = (y_local[i] + (x0_self[i] * beta_a) - (x1_self[i] * beta_a)) & mask;
        y_local[i] = (y_local[i] + (x0_next[i] * beta_b) - (x1_next[i] * beta_b)) & mask;
    }

    // 公开重构输出
    std::vector<ShareValue> out(vec_size, 0);
    if (party_id == 0) {
        std::vector<ShareValue> y1(vec_size, 0), y2(vec_size, 0);
        for (int i = 0; i < vec_size; i++) {
            netio.recv_data(1, &y1[i], bytem);
            netio.recv_data(2, &y2[i], bytem);
            out[i] = (y_local[i] + y1[i] + y2[i]) & mask;
        }
        for (int i = 0; i < vec_size; i++) {
            netio.store_data(1, &out[i], bytem);
            netio.store_data(2, &out[i], bytem);
        }
        netio.send_stored_data(1);
        netio.send_stored_data(2);
    } else {
        for (int i = 0; i < vec_size; i++) {
            netio.store_data(0, &y_local[i], bytem);
        }
        netio.send_stored_data(0);
        for (int i = 0; i < vec_size; i++) {
            netio.recv_data(0, &out[i], bytem);
        }
    }
    for (int i = 0; i < vec_size; i++) {
        out[i] &= mask;
    }
    return out;
}
