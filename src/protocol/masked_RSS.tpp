ShareValue MSSshare_recon(const int party_id, NetIOMP &netio, MSSshare *s) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_recon requires the input to have been shared");
    }
#endif
    ShareValue total;
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
    total = (total + s->v1 + s->v2) & s->MASK;
    return total;
}

inline std::vector<ShareValue> MSSshare_recon_vec(const int party_id, NetIOMP &netio,
                                                  MSSshare_v &s) {
    int len = s.size();
#ifdef DEBUG_MODE
    for (int i = 0; i < len; i++) {
        if (!s[i].has_shared) {
            error("MSSshare_recon_vec requires the input to have been shared");
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
        ShareValue total;
        if (party_id == 0) {
            netio.recv_data(1, &total, s[i].BYTELEN);
        } else if (party_id == 1) {
            netio.recv_data(0, &total, s[i].BYTELEN);
        } else if (party_id == 2) {
            netio.recv_data(0, &total, s[i].BYTELEN);
        }
        result[i] = (total + s[i].v1 + s[i].v2) & s[i].MASK;
    }
    return result;
}

void MSSshare_preprocess(const int secret_holder_id, const int party_id, std::vector<PRGSync> &PRGs,
                         NetIOMP &netio, MSSshare *s) {
#ifdef DEBUG_MODE
    s->has_preprocess = true;
#endif

    if (secret_holder_id != 2) {
        if (party_id == 0) {
            PRGs[0].gen_random_data(&s->v1, s->BYTELEN);
            s->v1 &= s->MASK;
        } else if (party_id == 1) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 &= s->MASK;
        }
    }

    if (secret_holder_id != 1) {
        if (party_id == 0) {
            PRGs[1].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 &= s->MASK;
        } else if (party_id == 2) {
            PRGs[0].gen_random_data(&s->v2, s->BYTELEN);
            s->v2 &= s->MASK;
        }
    }
}

void MSSshare_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                         MSSshare *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_share_from must be invoked after MSSshare_preprocess");
    }
    s->has_shared = true;
#endif

    // Party 0 holds secret x
    if (secret_holder_id == 0) {
        if (party_id == 0) {
            ShareValue m = x;
            m -= s->v1;
            m -= s->v2;
            m &= s->MASK;
            // send m = x - r1 - r2
            netio.send_data(1, &m, s->BYTELEN);
            netio.send_data(2, &m, s->BYTELEN);
        } else {
            netio.recv_data(0, &s->v1, s->BYTELEN);
            s->v1 &= s->MASK;
        }
    }
    // Party 1,2 holds secret x
    else if (secret_holder_id == 1 || secret_holder_id == 2) {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id) {
            s->v1 = x;
            s->v1 -= s->v2;
            s->v1 &= s->MASK;
            // send m = x - r1(or r2)
            netio.send_data(other_party, &s->v1, s->BYTELEN);
        } else if (party_id == other_party) {
            netio.recv_data(secret_holder_id, &s->v1, s->BYTELEN);
            s->v1 &= s->MASK;
        }
    }
}

void MSSshare_share_from_store(const int party_id, NetIOMP &netio, MSSshare *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_preprocess) {
        error("MSSshare_share_from must be invoked after MSSshare_preprocess");
    }
    s->has_shared = true;
#endif

    if (party_id == 0) {
        ShareValue m = x;
        m -= s->v1;
        m -= s->v2;
        m &= s->MASK;
        // send m = x - r1 - r2
        netio.store_data(1, &m, s->BYTELEN);
        netio.store_data(2, &m, s->BYTELEN);
    } else {
        netio.recv_data(0, &s->v1, s->BYTELEN);
        s->v1 &= s->MASK;
    }
}

void MSSshare_add_plain(const int party_id, MSSshare *s, ShareValue x) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("MSSshare_add_plain must be invoked after shared");
    }
#endif
    if (party_id != 0) {
        s->v1 = (s->v1 + x) & s->MASK;
    }
}

void MSSshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 MSSshare_mul_res *res, const MSSshare *s1, const MSSshare *s2) {

#ifdef DEBUG_MODE
    if (!s1->has_preprocess || !s2->has_preprocess) {
        error("MSSshare_mul_res_preprocess requires s1 and s2 to have been preprocessed");
    }
    if (res->BITLEN != s1->BITLEN || res->BITLEN != s2->BITLEN) {
        error("MSSshare_mul_res_preprocess: bit-length mismatch");
    }
    res->has_preprocess = true;
#endif

    MSSshare_preprocess(0, party_id, PRGs, netio, res);

    if (party_id == 0 || party_id == 1) {
        PRGs[0].gen_random_data(&res->v3, res->BYTELEN);
        res->v3 &= res->MASK;
    }
    if (party_id == 0) {
        ShareValue temp = (s1->v1 + s1->v2) * (s2->v1 + s2->v2);
        temp -= res->v3;
        temp &= res->MASK;
        netio.store_data(2, &temp, res->BYTELEN);
        // need to send later
    } else if (party_id == 2) {
        netio.recv_data(0, &res->v3, res->BYTELEN);
        res->v3 &= res->MASK;
    }
}

void MSSshare_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_mul_res *res,
                               const MSSshare *s1, const MSSshare *s2) {
#ifdef DEBUG_MODE
    if (!res->has_preprocess) {
        error("MSSshare_mul_res_calc_mul must be invoked after "
              "MSSshare_mul_res_preprocess");
    }
    if (!s1->has_shared || !s2->has_shared) {
        error("MSSshare_mul_res_calc_mul requires s1 and s2 to have been shared");
    }
    if (res->BITLEN != s1->BITLEN || res->BITLEN != s2->BITLEN) {
        error("MSSshare_mul_res_calc_mul: bit-length mismatch");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }
    ShareValue temp = 0;
    ShareValue recv_temp;
    temp += s1->v1 * s2->v2;
    temp += s1->v2 * s2->v1;
    temp += res->v3;
    temp -= res->v2;
    if (party_id == 1) {
        temp += s1->v1 * s2->v1;
    }

    netio.send_data(3 - party_id, &temp, res->BYTELEN);
    netio.recv_data(3 - party_id, &recv_temp, res->BYTELEN);

    res->v1 = (recv_temp + temp) & res->MASK;
}

inline void MSSshare_mul_res_calc_mul_vec(const int party_id, NetIOMP &netio,
                                          MSSshare_mul_res_v &res_vec, MSSshare_v &s1_vec,
                                          MSSshare_v &s2_vec) {
    int vec_len = res_vec.size();
#ifdef DEBUG_MODE
    if (vec_len != s1_vec.size() || vec_len != s2_vec.size()) {
        error("MSSshare_mul_res_calc_mul_vec: input vector size mismatch");
    }
    if (res_vec.BITLEN != s1_vec.BITLEN || res_vec.BITLEN != s2_vec.BITLEN) {
        error("MSSshare_mul_res_calc_mul_vec: bitlen mismatch");
    }
    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        auto &s1 = s1_vec[i];
        auto &s2 = s2_vec[i];
        if (!res.has_preprocess) {
            error("MSSshare_mul_res_calc_mul_vec must be invoked after "
                  "MSSshare_mul_res_preprocess");
        }
        if (!s1.has_shared || !s2.has_shared) {
            error("MSSshare_mul_res_calc_mul_vec requires s1 and s2 to have been shared");
        }
        if (res.BITLEN != s1.BITLEN || res.BITLEN != s2.BITLEN) {
            error("MSSshare_mul_res_calc_mul_vec: bit-length mismatch");
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
        temp += s1.v1 * s2.v2;
        temp += s1.v2 * s2.v1;
        temp += res.v3;
        temp -= res.v2;

        if (party_id == 1) {
            temp += s1.v1 * s2.v1;
        }
        res.v1 = temp & res.MASK;
        netio.store_data(3 - party_id, &temp, res.BYTELEN);
    }
    netio.send_stored_data(3 - party_id);

    for (int i = 0; i < vec_len; i++) {
        auto &res = res_vec[i];
        ShareValue recv_temp = 0;
        netio.recv_data(3 - party_id, &recv_temp, res.BYTELEN);
        res.v1 = (recv_temp + res.v1) & res.MASK;
    }
}
