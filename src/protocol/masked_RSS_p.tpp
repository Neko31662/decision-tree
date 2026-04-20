
ShareValue MSSshare_p_recon(const int party_id, NetIOMP &netio, MSSshare_p *s) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_p_recon requires the input to have been shared");
    }
#endif
    ShareValue total(0);
    if (party_id == 0) {
        netio.send_data(1, &s->v2, s->BYTELEN);
        netio.send_data(2, &s->v1, s->BYTELEN);

        netio.recv_data(1, &total, s->BYTELEN);
    } else if (party_id == 1) {
        netio.send_data(0, &s->v1, s->BYTELEN);
        netio.recv_data(0, &total, s->BYTELEN);

    } else if (party_id == 2) {
        netio.recv_data(0, &total, s->BYTELEN);
    }
    total = (total + s->v1 + s->v2) % s->p;
    return total;
}

inline std::vector<ShareValue> MSSshare_p_recon_vec(const int party_id, NetIOMP &netio,
                                                    MSSshare_p_v &s) {
    int len = s.size();
#ifdef DEBUG_MODE
    for (int i = 0; i < len; i++) {
        if (!s[i].has_shared) {
            error("MSSshare_p_recon_vec requires the input to have been shared");
        }
    }
#endif
    std::vector<ShareValue> result(len);
    for (int i = 0; i < len; i++) {
        if (party_id == 0) {
            netio.store_data(1, &s[i].v2, s[i].BYTELEN);
            netio.store_data(2, &s[i].v1, s[i].BYTELEN);
        } else if (party_id == 1) {
            netio.store_data(0, &s[i].v1, s[i].BYTELEN);
        }
    }
    netio.send_stored_data_all();
    for (int i = 0; i < len; i++) {
        ShareValue total(0);
        if (party_id == 0) {
            netio.recv_data(1, &total, s[i].BYTELEN);
        } else if (party_id == 1) {
            netio.recv_data(0, &total, s[i].BYTELEN);
        } else if (party_id == 2) {
            netio.recv_data(0, &total, s[i].BYTELEN);
        }
        result[i] = (total + s[i].v1 + s[i].v2) % s[i].p;
    }
    return result;
}

void MSSshare_p_preprocess(const int secret_holder_id, const int party_id,
                           std::vector<PRGSync> &PRGs, NetIOMP &netio, MSSshare_p *s) {
#ifdef DEBUG_MODE
    if (s->has_preprocess) {
        error("MSSshare_p_preprocess has already been called on this object");
    }
    s->has_preprocess = true;
#endif

    if (secret_holder_id != 2) {
        if (party_id == 0) {
            PRGs[0].gen_random_data(&s->v1, s->BYTELEN);
            s->v1 %= s->p;
        } else if (party_id == 1) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 %= s->p;
        }
    }

    if (secret_holder_id != 1) {
        if (party_id == 0) {
            PRGs[1].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 %= s->p;
        } else if (party_id == 2) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 %= s->p;
        }
    }
}

void MSSshare_p_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                           MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_p_share_from must be invoked after MSSshare_p_preprocess");
    }
    s->has_shared = true;
#endif

    // Party 0 holds secret x
    if (secret_holder_id == 0) {
        if (party_id == 0) {
            ShareValue m = x;
            m = (m - s->v1 + s->p) % s->p;
            m = (m - s->v2 + s->p) % s->p;
            // send m = x - r1 - r2
            netio.send_data(1, &m, s->BYTELEN);
            netio.send_data(2, &m, s->BYTELEN);
        } else {
            s->v1 = 0;
            netio.recv_data(0, &s->v1, s->BYTELEN);
        }
    }
    // Party 1,2 holds secret x
    else if (secret_holder_id == 1 || secret_holder_id == 2) {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id) {
            s->v1 = x;
            s->v1 = (s->v1 - s->v2 + s->p) % s->p;
            // send m = x - r1(or r2)
            netio.send_data(other_party, &s->v1, s->BYTELEN);
        } else if (party_id == other_party) {
            s->v1 = 0;
            netio.recv_data(secret_holder_id, &s->v1, s->BYTELEN);
        }
    }
}

void MSSshare_p_share_from_store(const int party_id, NetIOMP &netio, MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_p_share_from must be invoked after MSSshare_p_preprocess");
    }
    s->has_shared = true;
#endif

    if (party_id == 0) {
        ShareValue m = x;
        m = (m - s->v1 + s->p) % s->p;
        m = (m - s->v2 + s->p) % s->p;
        // send m = x - r1 - r2
        netio.store_data(1, &m, s->BYTELEN);
        netio.store_data(2, &m, s->BYTELEN);
    } else {
        s->v1 = 0;
        netio.recv_data(0, &s->v1, s->BYTELEN);
    }
}

void MSSshare_p_add_plain(const int party_id, MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_p_add_plain must be invoked after shared");
    }
#endif
    if (party_id != 0) {
        s->v1 = (s->v1 + x) % s->p;
    }
}

void MSSshare_p_minus_plain(const int party_id, MSSshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_p_add_plain must be invoked after shared");
    }
#endif
    if (party_id != 0) {
        x %= s->p;
        s->v1 = (s->v1 - x + s->p) % s->p;
    }
}

void MSSshare_p_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                   MSSshare_p_mul_res *res, const MSSshare_p *s1,
                                   const MSSshare_p *s2) {

#ifdef DEBUG_MODE
    if (!s1->has_preprocess || !s2->has_preprocess) {
        error("MSSshare_p_mul_res_preprocess requires s1 and s2 to have been preprocessed");
    }
    if (res->p != s1->p || res->p != s2->p) {
        error("MSSshare_p_mul_res_preprocess: modulus p mismatch");
    }
    if (res->has_preprocess) {
        error("MSSshare_p_mul_res_preprocess has already been called on this object");
    }
#endif

    MSSshare_p_preprocess(0, party_id, PRGs, netio, res);

    if (party_id == 0 || party_id == 1) {
        PRGs[0].gen_random_data(&res->v3, res->BYTELEN);
        res->v3 %= res->p;
    }
    if (party_id == 0) {
        ShareValue temp = (s1->v1 + s1->v2) % s1->p;
        temp = (temp * ((s2->v1 + s2->v2) % s2->p)) % res->p;
        temp = (temp - res->v3 + res->p) % res->p;
        netio.store_data(2, &temp, res->BYTELEN);
        // need to send later
    } else if (party_id == 2) {
        res->v3 = 0;
        netio.recv_data(0, &res->v3, res->BYTELEN);
    }
}

void MSSshare_p_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_p_mul_res *res,
                                 const MSSshare_p *s1, const MSSshare_p *s2) {
#ifdef DEBUG_MODE
    if (!res->has_preprocess) {
        error("MSSshare_p_mul_res_calc_mul must be invoked after "
              "MSSshare_p_mul_res_preprocess");
    }
    if (!s1->has_shared || !s2->has_shared) {
        error("MSSshare_p_mul_res_calc_mul requires s1 and s2 to have been shared");
    }
    if (res->p != s1->p || res->p != s2->p) {
        error("MSSshare_p_mul_res_calc_mul: modulus p mismatch");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    ShareValue temp = 0;
    ShareValue recv_temp = 0;
    temp += (s1->v1 * s2->v2) % res->p;
    temp += (s1->v2 * s2->v1) % res->p;
    temp += res->v3;
    temp += (res->p - res->v2);
    if (party_id == 1) {
        temp += (s1->v1 * s2->v1) % res->p;
    }
    temp %= res->p;

    netio.send_data(3 - party_id, &temp, res->BYTELEN);
    netio.recv_data(3 - party_id, &recv_temp, res->BYTELEN);

    res->v1 = (recv_temp + temp) % res->p;
}

inline void MSSshare_p_mul_res_calc_mul_vec(const int party_id, NetIOMP &netio,
                                            MSSshare_p_mul_res_v &res_vec, MSSshare_p_v &s1_vec,
                                            MSSshare_p_v &s2_vec) {
    int vec_len = res_vec.size();
#ifdef DEBUG_MODE
    if (vec_len != s1_vec.size() || vec_len != s2_vec.size()) {
        error("MSSshare_p_mul_res_calc_mul_vec: input vector size mismatch");
    }
    if (res_vec.p != s1_vec.p || res_vec.p != s2_vec.p) {
        error("MSSshare_p_mul_res_calc_mul_vec: modulus mismatch between vectors");
    }
    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        auto &s1 = s1_vec[i];
        auto &s2 = s2_vec[i];
        if (!res.has_preprocess) {
            error("MSSshare_p_mul_res_calc_mul_vec must be invoked after "
                  "MSSshare_p_mul_res_preprocess");
        }
        if (!s1.has_shared || !s2.has_shared) {
            error("MSSshare_p_mul_res_calc_mul_vec requires s1 and s2 to have been shared");
        }
        if (res.p != s1.p || res.p != s2.p) {
            error("MSSshare_p_mul_res_calc_mul_vec: modulus p mismatch");
        }
        res.has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        auto &s1 = s1_vec[i];
        auto &s2 = s2_vec[i];

        ShareValue temp = 0;
        temp += (s1.v1 * s2.v2) % res.p;
        temp += (s1.v2 * s2.v1) % res.p;
        temp += res.v3;
        temp += (res.p - res.v2);
        if (party_id == 1) {
            temp += (s1.v1 * s2.v1) % res.p;
        }
        temp %= res.p;

        netio.store_data(3 - party_id, &temp, res.BYTELEN);
        res.v1 = temp % res.p;
    }
    netio.send_stored_data(3 - party_id);

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        ShareValue recv_temp = 0;
        netio.recv_data(3 - party_id, &recv_temp, res.BYTELEN);
        res.v1 = (recv_temp + res.v1) % res.p;
    }
}
