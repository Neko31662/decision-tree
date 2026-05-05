inline void MSSshare_reshare_to_random_mask(const int party_id, NetIOMP &netio, const MSSshare *src,
                                            MSSshare *dst) {
#ifdef DEBUG_MODE
    if (!src->has_shared) {
        error("MSSshare_reshare_to_random_mask: src must be shared");
    }
    if (!dst->has_preprocess) {
        error("MSSshare_reshare_to_random_mask: dst must be preprocessed");
    }
    if (src->BITLEN != dst->BITLEN) {
        error("MSSshare_reshare_to_random_mask: bitlen mismatch");
    }
    dst->has_shared = true;
#endif

    if (party_id == 0) {
        return;
    }

    ShareValue delta_r_local = (src->v2 - dst->v2) & dst->MASK;
    ShareValue delta_r_peer = 0;

    netio.send_data(3 - party_id, &delta_r_local, dst->BYTELEN);
    netio.recv_data(3 - party_id, &delta_r_peer, dst->BYTELEN);

    dst->v1 = (src->v1 + delta_r_local + delta_r_peer) & dst->MASK;
}

void PI_cotp_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_cotp_intermediate &intermediate) {
    std::vector<PI_cotp_intermediate *> intermediate_vec = {&intermediate};
    PI_cotp_preprocess_vec(party_id, PRGs, netio, intermediate_vec);
}

void PI_cotp_preprocess_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                            std::vector<PI_cotp_intermediate *> &intermediate_vec) {
    int batch_size = (int)intermediate_vec.size();

    std::vector<PI_sign_mult_intermediate *> sign_mult_inters;
    std::vector<MSSshare *> random_cmp_inputs;
    std::vector<std::vector<MSSshare *>> random_mul_input_ptrs_vec;
    std::vector<MSSshare *> sign_outs;
    std::vector<std::vector<MSSshare *>> mult_out_ptrs_vec;
    sign_mult_inters.reserve(batch_size);
    random_cmp_inputs.reserve(batch_size);
    random_mul_input_ptrs_vec.reserve(batch_size);
    sign_outs.reserve(batch_size);
    mult_out_ptrs_vec.reserve(batch_size);

    for (int b = 0; b < batch_size; b++) {
        PI_cotp_intermediate &intermediate = *intermediate_vec[b];
        int vec_size = intermediate.vec_size;
#ifdef DEBUG_MODE
        if (intermediate.has_preprocess) {
            error("PI_cotp_preprocess_vec has already been called on this object");
        }
        if (vec_size <= 0) {
            error("PI_cotp_preprocess_vec: vec_size must be positive");
        }
        intermediate.has_preprocess = true;
#endif

        MSSshare &random_cmp_input = intermediate.random_cmp_input;
        std::vector<MSSshare> &random_mul_input_vec = intermediate.random_mul_input_vec;

        // Random placeholders for online re-sharing target masks.
        MSSshare_preprocess(0, party_id, PRGs, netio, &random_cmp_input);
        for (int i = 0; i < vec_size; i++) {
            MSSshare_preprocess(0, party_id, PRGs, netio, &random_mul_input_vec[i]);
        }

        random_mul_input_ptrs_vec.push_back(make_ptr_vec(random_mul_input_vec));
        mult_out_ptrs_vec.push_back(make_ptr_vec(intermediate.mult_out_vec));
        sign_mult_inters.push_back(&intermediate.sign_mult_intermediate);
        random_cmp_inputs.push_back(&random_cmp_input);
        sign_outs.push_back(&intermediate.sign_out);
    }

    // 批量预处理 sign-mult，减少P1/P2通信轮数。
    PI_sign_mult_preprocess_vec(party_id, PRGs, netio, sign_mult_inters, random_cmp_inputs,
                                random_mul_input_ptrs_vec, sign_outs, mult_out_ptrs_vec);
}

std::vector<ShareValue> PI_cotp(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                PI_cotp_intermediate &intermediate, MSSshare *input_x,
                                MSSshare *input_y, std::vector<MSSshare *> &input_L_vec,
                                std::vector<MSSshare *> &input_R_vec) {
    int ell = intermediate.ell;
    int vec_size = intermediate.vec_size;
#ifdef DEBUG_MODE
    if (!intermediate.has_preprocess) {
        error("PI_cotp: intermediate must be preprocessed before calling");
    }
    if (!input_x->has_shared || !input_y->has_shared) {
        error("PI_cotp: input_x and input_y must be shared before calling");
    }
    if (input_x->BITLEN != ell || input_y->BITLEN != ell) {
        error("PI_cotp: input_x/input_y bitlen mismatch");
    }
    if ((int)input_L_vec.size() != vec_size || (int)input_R_vec.size() != vec_size) {
        error("PI_cotp: input vector size mismatch");
    }
    for (int i = 0; i < vec_size; i++) {
        if (!input_L_vec[i]->has_shared || !input_R_vec[i]->has_shared) {
            error("PI_cotp: input L/R vectors must be shared before calling");
        }
        if (input_L_vec[i]->BITLEN != ell || input_R_vec[i]->BITLEN != ell) {
            error("PI_cotp: input L/R bitlen mismatch");
        }
    }
#endif

    MSSshare &random_cmp_input = intermediate.random_cmp_input;
    std::vector<MSSshare> &random_mul_input_vec = intermediate.random_mul_input_vec;
    MSSshare &sign_out = intermediate.sign_out;
    std::vector<MSSshare> &mult_out_vec = intermediate.mult_out_vec;
    PI_sign_mult_intermediate &sign_mult_intermediate = intermediate.sign_mult_intermediate;

    // x - y
    MSSshare diff_share = *input_x - *input_y;

    // R - L
    std::vector<MSSshare> rl_vec(vec_size, MSSshare(ell));
    for (int i = 0; i < vec_size; i++) {
        rl_vec[i] = *input_R_vec[i] - *input_L_vec[i];
#ifdef DEBUG_MODE
        rl_vec[i].has_shared = true;
#endif
    }

    // Re-share (x-y) and (R-L) onto random masks prepared in preprocess.
    MSSshare_reshare_to_random_mask(party_id, netio, &diff_share, &random_cmp_input);
    for (int i = 0; i < vec_size; i++) {
        MSSshare_reshare_to_random_mask(party_id, netio, &rl_vec[i], &random_mul_input_vec[i]);
    }

    // sign(x-y) * (R-L)
    auto random_mul_input_ptrs = make_ptr_vec(random_mul_input_vec);
    auto mult_out_ptrs = make_ptr_vec(mult_out_vec);
    PI_sign_mult(party_id, PRGs, netio, sign_mult_intermediate, &random_cmp_input,
                 random_mul_input_ptrs, &sign_out, mult_out_ptrs);

    // sign(x-y) * (R-L) + L
    std::vector<MSSshare> out_vec(vec_size, MSSshare(ell));
    for (int i = 0; i < vec_size; i++) {
        out_vec[i] = mult_out_vec[i] + *input_L_vec[i];
#ifdef DEBUG_MODE
        out_vec[i].has_shared = true;
#endif
    }

    // Reveal to all parties.
    std::vector<ShareValue> opened(vec_size, 0);
    for (int i = 0; i < vec_size; i++) {
        opened[i] = MSSshare_recon(party_id, netio, &out_vec[i]);
    }
    return opened;
}
