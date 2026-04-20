
void ADDshare_p_share_from(const int secret_holder_id, const int party_id,
                           std::vector<PRGSync> &PRGs, NetIOMP &netio, ADDshare_p *s,
                           ShareValue x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (secret_holder_id == 0) {
        if (party_id == 0 || party_id == 1) {
            s->v = 0;
            PRGs[0].gen_random_data(&s->v, s->BYTELEN);
            s->v %= s->p;
        }
        if (party_id == 0) {
            ShareValue temp = (x - s->v + s->p) % s->p;
            netio.send_data(2, &temp, s->BYTELEN);
        } else if (party_id == 2) {
            s->v = 0;
            netio.recv_data(0, &s->v, s->BYTELEN);
            s->v %= s->p;
        }
    } else {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id || party_id == 0) {
            int prg_idx = (party_id == 0) ? (secret_holder_id - 1) : 0;
            s->v = 0;
            PRGs[prg_idx].gen_random_data(&s->v, s->BYTELEN);
            s->v %= s->p;
        }
        if (party_id == secret_holder_id) {
            ShareValue temp = (x - s->v + s->p) % s->p;
            netio.send_data(other_party, &temp, s->BYTELEN);
        } else if (party_id == other_party) {
            s->v = 0;
            netio.recv_data(secret_holder_id, &s->v, s->BYTELEN);
            s->v %= s->p;
        }
    }
}

void ADDshare_p_share_from_store(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 ADDshare_p *s, ShareValue x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (party_id == 0 || party_id == 1) {
        s->v = 0;
        PRGs[0].gen_random_data(&s->v, s->BYTELEN);
        s->v %= s->p;
    }
    if (party_id == 0) {
        ShareValue temp = (x - s->v + s->p) % s->p;
        netio.store_data(2, &temp, s->BYTELEN);
    } else if (party_id == 2) {
        s->v = 0;
        netio.recv_data(0, &s->v, s->BYTELEN);
        s->v %= s->p;
    }
}

void ADDshare_p_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                   ADDshare_p_mul_res *res) {
#ifdef DEBUG_MODE
    res->has_preprocess = true;
#endif
    ShareValue tmpx, tmpy, tmpz;
    if (party_id == 0 || party_id == 1) {
        // P0和P1同步生成x,y,z的P1持有的份额
        res->x = res->y = res->z = 0;
        PRGs[0].gen_random_data(&res->x, res->BYTELEN);
        PRGs[0].gen_random_data(&res->y, res->BYTELEN);
        PRGs[0].gen_random_data(&res->z, res->BYTELEN);
        res->x %= res->p;
        res->y %= res->p;
        res->z %= res->p;
        if (party_id == 0) {
            tmpx = res->x;
            tmpy = res->y;
            tmpz = res->z;
        }
    }

    if (party_id == 0 || party_id == 2) {
        // P0和P2同步生成x,y的P2持有的份额
        res->x = res->y = 0;
        PRGs[(party_id + 1) % 3].gen_random_data(&res->x, res->BYTELEN);
        PRGs[(party_id + 1) % 3].gen_random_data(&res->y, res->BYTELEN);
        res->x %= res->p;
        res->y %= res->p;
    }

    if (party_id == 0) {
        ShareValue temp = ((tmpx + res->x) % res->p * (tmpy + res->y) % res->p) % res->p;
        temp = (temp - res->z + res->p) % res->p;
        netio.store_data(2, &temp, res->BYTELEN);
        // 之后需要调用send_data将temp发送给P2
    } else if (party_id == 2) {
        res->z = 0;
        netio.recv_data(0, &res->z, res->BYTELEN);
        res->z %= res->p;
    }
}

void ADDshare_p_mul_res_cal_mult(const int party_id, NetIOMP &netio, ADDshare_p_mul_res *res,
                                 ADDshare_p *s1, ADDshare_p *s2) {
#ifdef DEBUG_MODE
    if (!res->has_preprocess) {
        error("ADDshare_p_mul_res_cal_mult must be invoked after preprocess");
    }
    if (!s1->has_shared || !s2->has_shared) {
        error("ADDshare_p_mul_res_cal_mult requires s1 and s2 to have been shared");
    }
    if (s1->p != res->p || s2->p != res->p) {
        error("ADDshare_p_mul_res_cal_mult: modulus p mismatch");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    ShareValue d = (s1->v - res->x + res->p) % res->p;
    ShareValue e = (s2->v - res->y + res->p) % res->p;

    // P1和P2重构d和e，P0不参与
    ShareValue d_sum = 0, e_sum = 0;
    netio.send_data(3 - party_id, &d, res->BYTELEN);
    netio.send_data(3 - party_id, &e, res->BYTELEN);

    netio.recv_data(3 - party_id, &d_sum, res->BYTELEN);
    netio.recv_data(3 - party_id, &e_sum, res->BYTELEN);

    d_sum = (d_sum + d) % res->p;
    e_sum = (e_sum + e) % res->p;

    res->v = (res->z + (d_sum * res->y) % res->p + (e_sum * res->x) % res->p) % res->p;
    if (party_id == 1) {
        res->v = (res->v + (d_sum * e_sum) % res->p) % res->p;
    }
}

ShareValue ADDshare_p_recon(const int party_id, NetIOMP &netio, ADDshare_p *s) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("ADDshare_p_recon requires the input to have been shared");
    }
#endif
    if (party_id == 0) {
        return 0;
    }
    ShareValue total = 0;
    netio.send_data(3 - party_id, &s->v, s->BYTELEN);
    netio.recv_data(3 - party_id, &total, s->BYTELEN);
    total = (total + s->v) % s->p;
    return total;
}

inline std::vector<ShareValue> ADDshare_p_recon_vec(const int party_id, NetIOMP &netio,
                                                    ADDshare_p_v &s) {
    int len = s.size();
#ifdef DEBUG_MODE
    for (int i = 0; i < len; i++) {
        if (!s[i].has_shared) {
            error("ADDshare_p_recon_vec requires the input to have been shared");
        }
    }
#endif
    if (party_id == 0) {
        return std::vector<ShareValue>(len, 0);
    }
    std::vector<ShareValue> result(len);
    for (int i = 0; i < len; i++) {
        netio.store_data(3 - party_id, &s[i].v, s[i].BYTELEN);
    }
    netio.send_stored_data(3 - party_id);
    for (int i = 0; i < len; i++) {
        ShareValue tmp = 0;
        netio.recv_data(3 - party_id, &tmp, s[i].BYTELEN);
        result[i] = (tmp + s[i].v) % s[i].p;
    }
    return result;
}

void ADDshare_p_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio, ADDshare_p_mul_res_v &res,
                                     ADDshare_p_v &s1, ADDshare_p_v &s2) {
    int len = res.size();
#ifdef DEBUG_MODE
    if (s1.size() != len || s2.size() != len) {
        error("ADDshare_p_mul_res_cal_mult_vec: input vector sizes do not match");
    }
    if (res.p != s1.p || res.p != s2.p) {
        error("ADDshare_p_mul_res_cal_mult_vec: modulus mismatch between vectors");
    }
    for (int i = 0; i < len; i++) {
        if (!s1[i].has_shared || !s2[i].has_shared) {
            error("ADDshare_p_mul_res_cal_mult_vec requires both inputs to have been shared");
        }
        if (s1[i].p != res[i].p || s2[i].p != res[i].p) {
            error("ADDshare_p_mul_res_cal_mult_vec: modulus p mismatch at index " +
                  std::to_string(i));
        }
        res[i].has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }

    std::vector<ShareValue> d(len), e(len);
    for (int i = 0; i < len; i++) {
        d[i] = (s1[i].v - res[i].x + res[i].p) % res[i].p;
        e[i] = (s2[i].v - res[i].y + res[i].p) % res[i].p;
    }
    std::vector<ShareValue> d_sum(len), e_sum(len);

    // 发送和接收信息
    for (int i = 0; i < len; i++) {
        netio.store_data(3 - party_id, &d[i], res[i].BYTELEN);
        netio.store_data(3 - party_id, &e[i], res[i].BYTELEN);
    }
    netio.send_stored_data(3 - party_id);

    for (int i = 0; i < len; i++) {
        ShareValue tmp_d = 0, tmp_e = 0;
        netio.recv_data(3 - party_id, &tmp_d, res[i].BYTELEN);
        netio.recv_data(3 - party_id, &tmp_e, res[i].BYTELEN);
        d_sum[i] = (tmp_d + d[i]) % res[i].p;
        e_sum[i] = (tmp_e + e[i]) % res[i].p;

        res[i].v = (res[i].z + (d_sum[i] * res[i].y) % res[i].p + (e_sum[i] * res[i].x) % res[i].p) %
                   res[i].p;
        if (party_id == 1) {
            res[i].v = (res[i].v + (d_sum[i] * e_sum[i]) % res[i].p) % res[i].p;
        }
    }
}
