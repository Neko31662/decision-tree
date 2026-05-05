namespace pi_sign_mult_detail {
inline ShareValue mod_add_p(ShareValue a, ShareValue b, ShareValue p) {
    return (a % p + b % p) % p;
}

inline ShareValue mod_sub_p(ShareValue a, ShareValue b, ShareValue p) {
    return (a % p + p - b % p) % p;
}
} // namespace pi_sign_mult_detail

void PI_sign_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                             PI_sign_mult_intermediate &intermediate, MSSshare *input_x,
                             std::vector<MSSshare *> &input_y_vec, MSSshare *output_z,
                             std::vector<MSSshare *> &output_w_vec) {
    std::vector<PI_sign_mult_intermediate *> intermediate_vec = {&intermediate};
    std::vector<MSSshare *> input_x_vec = {input_x};
    std::vector<std::vector<MSSshare *>> input_y_vec_vec = {input_y_vec};
    std::vector<MSSshare *> output_z_vec = {output_z};
    std::vector<std::vector<MSSshare *>> output_w_vec_vec = {output_w_vec};
    PI_sign_mult_preprocess_vec(party_id, PRGs, netio, intermediate_vec, input_x_vec,
                                input_y_vec_vec, output_z_vec, output_w_vec_vec);
}

void PI_sign_mult_preprocess_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 std::vector<PI_sign_mult_intermediate *> &intermediate_vec,
                                 std::vector<MSSshare *> &input_x_vec,
                                 std::vector<std::vector<MSSshare *>> &input_y_vec_vec,
                                 std::vector<MSSshare *> &output_z_vec,
                                 std::vector<std::vector<MSSshare *>> &output_w_vec_vec) {
    int batch_size = (int)intermediate_vec.size();
#ifdef DEBUG_MODE
    if ((int)input_x_vec.size() != batch_size || (int)input_y_vec_vec.size() != batch_size ||
        (int)output_z_vec.size() != batch_size || (int)output_w_vec_vec.size() != batch_size) {
        error("PI_sign_mult_preprocess_vec: batch input size mismatch");
    }
#endif

    std::vector<ShareValue> gamma_local_list;
    std::vector<int> gamma_bytelen_list;
    gamma_local_list.reserve(batch_size);
    gamma_bytelen_list.reserve(batch_size);

    for (int b = 0; b < batch_size; b++) {
        PI_sign_mult_intermediate &intermediate = *intermediate_vec[b];
        MSSshare &x = *input_x_vec[b];
        MSSshare &z = *output_z_vec[b];
        std::vector<MSSshare *> &input_y_vec = input_y_vec_vec[b];
        std::vector<MSSshare *> &output_w_vec = output_w_vec_vec[b];

        int ell = intermediate.ell;
        int y_size = input_y_vec.size();
#ifdef DEBUG_MODE
        if (!input_x_vec[b]->has_preprocess) {
            error("PI_sign_mult_preprocess_vec: input_x must be preprocessed before calling");
        }
        if (intermediate.has_preprocess) {
            error("PI_sign_mult_preprocess_vec has already been called on this object");
        }
        if (output_z_vec[b]->has_preprocess) {
            error("PI_sign_mult_preprocess_vec: output_z has already been preprocessed");
        }
        if (input_x_vec[b]->BITLEN != ell || output_z_vec[b]->BITLEN != ell) {
            error("PI_sign_mult_preprocess_vec: input_x/output_z bitlen mismatch");
        }
        if ((int)output_w_vec.size() != y_size) {
            error("PI_sign_mult_preprocess_vec: output_w_vec size mismatch");
        }
        if ((int)intermediate.r_double_prime_list.size() != y_size ||
            (int)intermediate.ry_rz_share_list.size() != y_size) {
            error("PI_sign_mult_preprocess_vec: intermediate y-size mismatch");
        }
        for (int j = 0; j < y_size; j++) {
            if (!input_y_vec[j]->has_preprocess) {
                error("PI_sign_mult_preprocess_vec: input_y must be preprocessed before calling");
            }
            if (input_y_vec[j]->BITLEN != ell || output_w_vec[j]->BITLEN != ell) {
                error("PI_sign_mult_preprocess_vec: input_y/output_w bitlen mismatch");
            }
            if (output_w_vec[j]->has_preprocess) {
                error("PI_sign_mult_preprocess_vec: output_w has already been preprocessed");
            }
        }
        intermediate.has_preprocess = true;
#endif

        MSSshare &r_prime = intermediate.r_prime;
        bool &Delta = intermediate.Delta;
        ShareValue &Gamma = intermediate.Gamma;
        std::vector<ShareValue> &rx_list = intermediate.rx_list;
        std::vector<MSSshare> &r_double_prime_list = intermediate.r_double_prime_list;
        std::vector<MSSshare> &ry_rz_share_list = intermediate.ry_rz_share_list;
        ShareValue p = intermediate.p;
        ShareValue mask = intermediate.mask;
        int bytep = byte_of(p);
        int bytem = x.BYTELEN;

        // Step 1: preprocess [r']^M, [r_z]^M, [r''_j]^M, [r_{w,j}]^M, [r_y,j*r_z]^M
        MSSshare_preprocess(0, party_id, PRGs, netio, &r_prime);
        MSSshare_preprocess(0, party_id, PRGs, netio, &z);
        for (int j = 0; j < y_size; j++) {
            MSSshare_preprocess(0, party_id, PRGs, netio, &r_double_prime_list[j]);
            MSSshare_preprocess(0, party_id, PRGs, netio, output_w_vec[j]);
            MSSshare_preprocess(0, party_id, PRGs, netio, &ry_rz_share_list[j]);
        }

        // Step 2: P0/P1 prepare r_x bits (same as PI_sign)
        if (party_id == 0 || party_id == 1) {
            for (int i = 0; i < ell - 1; i++) {
                ShareValue r_i = 0;
                PRGs[0].gen_random_data(&r_i, sizeof(ShareValue));
                rx_list[i] = r_i % p;
            }
        }
        if (party_id == 0) {
            ShareValue rx_hat = (x.v1 + x.v2) & (x.MASK >> 1);
            for (int i = 0; i < ell - 1; i++) {
                ShareValue bit = (rx_hat >> (ell - 2 - i)) & 1;
                bit ^= 1;
                bit = pi_sign_mult_detail::mod_sub_p(bit, rx_list[i], p);
                netio.store_data(2, &bit, bytep);
            }
        } else if (party_id == 2) {
            for (int i = 0; i < ell - 1; i++) {
                ShareValue r_i = 0;
                netio.recv_data(0, &r_i, bytep);
                rx_list[i] = r_i;
            }
        }

        // Step 3(local): P1/P2 sample Delta and compute local Gamma share term.
        if (party_id == 1 || party_id == 2) {
            ShareValue tmp = 0;
            PRGs[1].gen_random_data(&tmp, sizeof(ShareValue));
            Delta = (tmp & 1);
            Gamma = Delta ? ((-r_prime.v2) & mask) : (r_prime.v2 & mask);
            Gamma = (Gamma - z.v2) & mask;
            if (party_id == 1) {
                Gamma = (Gamma + (Delta ? 1 : 0)) & mask;
            }
            gamma_local_list.push_back(Gamma);
            gamma_bytelen_list.push_back(bytem);
        }

        // Step 4: P0 shares r_{y,j} * r_z to P1/P2
        ShareValue r_z = 0;
        if (party_id == 0) {
            r_z = (z.v1 + z.v2) & mask;
        }
        for (int j = 0; j < y_size; j++) {
            ShareValue ry_mul_rz = 0;
            if (party_id == 0) {
                ShareValue r_y = (input_y_vec[j]->v1 + input_y_vec[j]->v2) & mask;
                ry_mul_rz = (r_y * r_z) & mask;
            }
            MSSshare_share_from_store(party_id, netio, &ry_rz_share_list[j], ry_mul_rz);
        }
    }

    // Step 3(comm): P1/P2 直接双向发送并在预处理阶段还原 Gamma（不使用store）。
    if (party_id == 1 || party_id == 2) {
        for (int i = 0; i < (int)gamma_local_list.size(); i++) {
            netio.send_data(3 - party_id, &gamma_local_list[i], gamma_bytelen_list[i]);
        }
        for (int i = 0; i < batch_size; i++) {
            ShareValue peer = 0;
            int bytem = intermediate_vec[i]->r_prime.BYTELEN;
            netio.recv_data(3 - party_id, &peer, bytem);
            intermediate_vec[i]->Gamma = (intermediate_vec[i]->Gamma + peer) & intermediate_vec[i]->mask;
        }
    }
}

void PI_sign_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  PI_sign_mult_intermediate &intermediate, MSSshare *input_x,
                  std::vector<MSSshare *> &input_y_vec, MSSshare *output_z,
                  std::vector<MSSshare *> &output_w_vec) {
    int ell = intermediate.ell;
    int y_size = input_y_vec.size();
#ifdef DEBUG_MODE
    if (!input_x->has_shared) {
        error("PI_sign_mult: input_x must be shared before calling");
    }
    if (!intermediate.has_preprocess) {
        error("PI_sign_mult: intermediate must be preprocessed before calling");
    }
    if (!output_z->has_preprocess) {
        error("PI_sign_mult: output_z must be preprocessed before calling");
    }
    if (input_x->BITLEN != ell || output_z->BITLEN != ell) {
        error("PI_sign_mult: input_x/output_z bitlen mismatch");
    }
    if (output_w_vec.size() != (size_t)y_size) {
        error("PI_sign_mult: output_w_vec size mismatch");
    }
    if (intermediate.r_double_prime_list.size() != (size_t)y_size ||
        intermediate.ry_rz_share_list.size() != (size_t)y_size) {
        error("PI_sign_mult: intermediate y-size mismatch");
    }
    for (int j = 0; j < y_size; j++) {
        if (!input_y_vec[j]->has_shared) {
            error("PI_sign_mult: input_y must be shared before calling");
        }
        if (input_y_vec[j]->BITLEN != ell || output_w_vec[j]->BITLEN != ell) {
            error("PI_sign_mult: input_y/output_w bitlen mismatch");
        }
        if (!output_w_vec[j]->has_preprocess) {
            error("PI_sign_mult: output_w must be preprocessed before calling");
        }
    }
    output_z->has_shared = true;
    for (int j = 0; j < y_size; j++) {
        output_w_vec[j]->has_shared = true;
    }
#endif

    MSSshare &x = *input_x;
    MSSshare &z = *output_z;
    MSSshare &r_prime = intermediate.r_prime;
    bool &Delta = intermediate.Delta;
    ShareValue &Gamma = intermediate.Gamma;
    std::vector<ShareValue> &rx_list = intermediate.rx_list;
    std::vector<MSSshare> &r_double_prime_list = intermediate.r_double_prime_list;
    std::vector<MSSshare> &ry_rz_share_list = intermediate.ry_rz_share_list;
    ShareValue p = intermediate.p;
    ShareValue mask = intermediate.mask;
    int bytep = byte_of(p);
    int bytem = x.BYTELEN;

    std::vector<ShareValue> Gamma2_open(y_size, 0);

    if (party_id == 1 || party_id == 2) {
        // Step 1~7: same sign-comparison core as PI_sign
        std::vector<bool> mx_list(ell);
        ShareValue mx_hat = (x.v1) & (x.MASK >> 1);
        for (int i = 0; i < ell - 1; i++) {
            mx_list[i] = (mx_hat >> (ell - 2 - i)) & 1;
        }
        mx_list[ell - 1] = 0;
        rx_list[ell - 1] = party_id - 1;

        std::vector<ShareValue> xor_list(ell, 0);
        for (int i = 0; i < ell; i++) {
            xor_list[i] = mx_list[i] ? pi_sign_mult_detail::mod_sub_p(0, rx_list[i], p)
                                     : (rx_list[i] % p);
            if (party_id == 1 && mx_list[i]) {
                xor_list[i] = pi_sign_mult_detail::mod_add_p(xor_list[i], 1, p);
            }
        }

        std::vector<ShareValue> pre_list(ell, 0);
        ShareValue sum = 0;
        for (int i = 0; i < ell; i++) {
            sum = pi_sign_mult_detail::mod_add_p(sum, xor_list[i], p);
            pre_list[i] = pi_sign_mult_detail::mod_sub_p(sum, xor_list[i], p);
            pre_list[i] = pi_sign_mult_detail::mod_sub_p(pre_list[i], xor_list[i], p);
            if (party_id == 1) {
                pre_list[i] = pi_sign_mult_detail::mod_add_p(pre_list[i], 1, p);
            }
        }

        bool mx_sign = ((x.v1 & x.MASK) >> (ell - 1));
        std::vector<ShareValue> encode_list(ell, 0);
        std::vector<ShareValue> w_list(ell, 0);
        for (int i = 0; i < ell; i++) {
            PRGs[1].gen_random_data(&w_list[i], sizeof(ShareValue));
            w_list[i] = (w_list[i] % (p - 1)) + 1;
        }
        for (int i = 0; i < ell; i++) {
            ShareValue tmp = (mx_sign ^ Delta ^ mx_list[i]) ? pre_list[i] : (party_id - 1);
            encode_list[i] = (w_list[i] * tmp) % p;
        }

        auto pi = gen_random_permutation(ell, PRGs[1]);
        apply_permutation(pi, encode_list.data());

        for (int i = 0; i < ell; i++) {
            netio.store_data(0, &encode_list[i], bytep);
        }
        netio.send_stored_data(0);

        // Step 8: reveal Gamma''_j to each other
        std::vector<ShareValue> Gamma2_local(y_size, 0);
        for (int j = 0; j < y_size; j++) {
            ShareValue gamma_prime_m = ry_rz_share_list[j].v1 & mask;
            ShareValue gamma_prime_r = (Gamma * input_y_vec[j]->v2) & mask;
            if (Delta) {
                gamma_prime_r = (gamma_prime_r + r_double_prime_list[j].v2) & mask;
            } else {
                gamma_prime_r = (gamma_prime_r - r_double_prime_list[j].v2) & mask;
            }
            gamma_prime_r = (gamma_prime_r + ry_rz_share_list[j].v2) & mask;
            gamma_prime_r = (gamma_prime_r - output_w_vec[j]->v2) & mask;

            Gamma2_local[j] = (input_y_vec[j]->v1 * z.v2 + gamma_prime_r) & mask;

            netio.store_data(3 - party_id, &Gamma2_local[j], bytem);
            Gamma2_open[j] = (gamma_prime_m + Gamma2_local[j]) & mask;
        }
        netio.send_stored_data(3 - party_id);
        for (int j = 0; j < y_size; j++) {
            ShareValue peer = 0;
            netio.recv_data(3 - party_id, &peer, bytem);
            Gamma2_open[j] = (Gamma2_open[j] + peer) & mask;
        }
    }

    if (party_id == 0) {
        // P0 receives encoded list and recovers t
        std::vector<ShareValue> encode_list(ell, 0);
        for (int i = 0; i < ell; i++) {
            netio.recv_data(1, &encode_list[i], bytep);
        }
        for (int i = 0; i < ell; i++) {
            ShareValue tmp = 0;
            netio.recv_data(2, &tmp, bytep);
            encode_list[i] = pi_sign_mult_detail::mod_add_p(encode_list[i], tmp, p);
        }

        int cnt = 0;
        for (int i = 0; i < ell; i++) {
            if (encode_list[i] == 0) {
                cnt++;
            }
        }
        if (cnt > 1) {
            error("PI_sign_mult: P0 found more than one zero in encode_list");
        }

        ShareValue r = (x.v1 + x.v2) & x.MASK;
        bool t = (r >> (ell - 1));
        if (cnt) {
            t ^= 1;
        }

        ShareValue m_prime = ((t ? 1 : 0) - r_prime.v1 - r_prime.v2) & mask;

        for (int dst = 1; dst <= 2; dst++) {
            netio.store_data(dst, &m_prime, bytem);
        }
        for (int j = 0; j < y_size; j++) {
            ShareValue r_y = (input_y_vec[j]->v1 + input_y_vec[j]->v2) & mask;
            ShareValue r_double_prime_open = (r_double_prime_list[j].v1 + r_double_prime_list[j].v2) & mask;
            ShareValue m_double_prime = (m_prime * r_y + r_double_prime_open) & mask;
            for (int dst = 1; dst <= 2; dst++) {
                netio.store_data(dst, &m_double_prime, bytem);
            }
        }
        netio.send_stored_data(1);
        netio.send_stored_data(2);
    }

    if (party_id == 1 || party_id == 2) {
        // Final: recover m_z and m_w,j
        ShareValue m_prime = 0;
        netio.recv_data(0, &m_prime, bytem);
        ShareValue m_z = Delta ? ((-m_prime) & mask) : (m_prime & mask);
        m_z = (m_z + Gamma) & mask;
        z.v1 = m_z;

        for (int j = 0; j < y_size; j++) {
            ShareValue m_double_prime = 0;
            netio.recv_data(0, &m_double_prime, bytem);
            ShareValue m_w = (input_y_vec[j]->v1 * m_z + Gamma2_open[j]) & mask;
            if (Delta) {
                m_w = (m_w - m_double_prime) & mask;
            } else {
                m_w = (m_w + m_double_prime) & mask;
            }
            output_w_vec[j]->v1 = m_w;
        }
    }
}
