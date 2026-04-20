template <typename ShareType>
void ADDshare_share_from(const int secret_holder_id, const int party_id, std::vector<PRGSync> &PRGs,
                         NetIOMP &netio, ADDshare<ShareType> *s, ShareType x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (secret_holder_id == 0) {
        if (party_id == 0 || party_id == 1) {
            PRGs[0].gen_random_data(&s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
        if (party_id == 0) {
            ShareType temp = (x - s->v) & s->MASK;
            netio.send_data(2, &temp, s->BYTELEN);
        } else if (party_id == 2) {
            netio.recv_data(0, &s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
    } else {
        int other_party = 3 - secret_holder_id;
        if (party_id == secret_holder_id || party_id == 0) {
            int prg_idx = (party_id == 0) ? (secret_holder_id - 1) : 0;
            PRGs[prg_idx].gen_random_data(&s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
        if (party_id == secret_holder_id) {
            ShareType temp = (x - s->v) & s->MASK;
            netio.send_data(other_party, &temp, s->BYTELEN);
        } else if (party_id == other_party) {
            netio.recv_data(secret_holder_id, &s->v, s->BYTELEN);
            s->v &= s->MASK;
        }
    }
}

template <typename ShareType>
void ADDshare_share_from_store(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                               ADDshare<ShareType> *s, ShareType x) {
#ifdef DEBUG_MODE
    s->has_shared = true;
#endif
    if (party_id == 0 || party_id == 1) {
        PRGs[0].gen_random_data(&s->v, s->BYTELEN);
        s->v &= s->MASK;
    }
    if (party_id == 0) {
        ShareType temp = (x - s->v) & s->MASK;
        netio.store_data(2, &temp, s->BYTELEN);
    } else if (party_id == 2) {
        netio.recv_data(0, &s->v, s->BYTELEN);
        s->v &= s->MASK;
    }
}

template <typename ShareType>
void ADDshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 ADDshare_mul_res<ShareType> *res) {
#ifdef DEBUG_MODE
    res->has_preprocess = true;
#endif
    ShareType tmpx, tmpy, tmpz;
    if (party_id == 0 || party_id == 1) {
        // P0和P1同步生成x,y,z的P1持有的份额
        PRGs[0].gen_random_data(&res->x, res->BYTELEN);
        PRGs[0].gen_random_data(&res->y, res->BYTELEN);
        PRGs[0].gen_random_data(&res->z, res->BYTELEN);
        res->x &= res->MASK;
        res->y &= res->MASK;
        res->z &= res->MASK;
        if (party_id == 0) {
            tmpx = res->x;
            tmpy = res->y;
            tmpz = res->z;
        }
    }

    if (party_id == 0 || party_id == 2) {
        // P0和P2同步生成x,y的P2持有的份额
        PRGs[(party_id + 1) % 3].gen_random_data(&res->x, res->BYTELEN);
        PRGs[(party_id + 1) % 3].gen_random_data(&res->y, res->BYTELEN);
        res->x &= res->MASK;
        res->y &= res->MASK;
    }

    if (party_id == 0) {
        ShareType temp = (tmpx + res->x) * (tmpy + res->y);
        temp -= res->z;
        temp &= res->MASK;
        netio.store_data(2, &temp, res->BYTELEN);
        // 之后需要调用send_data将temp发送给P2
    } else if (party_id == 2) {
        netio.recv_data(0, &res->z, res->BYTELEN);
        res->z &= res->MASK;
    }
}

template <typename ShareType>
void ADDshare_mul_res_cal_mult(const int party_id, NetIOMP &netio, ADDshare_mul_res<ShareType> *res,
                               ADDshare<ShareType> *s1, ADDshare<ShareType> *s2) {
#ifdef DEBUG_MODE
    if (!s1->has_shared || !s2->has_shared) {
        error("ADDshare_mul_res_cal_mult requires both inputs to have been shared");
    }
    res->has_shared = true;
#endif
    if (party_id == 0) {
        return;
    }

    ShareType d = (s1->v - res->x) & res->MASK;
    ShareType e = (s2->v - res->y) & res->MASK;

    // P1和P2重构d和e，P0不参与
    ShareType d_sum, e_sum;
    netio.send_data(3 - party_id, &d, res->BYTELEN);
    netio.send_data(3 - party_id, &e, res->BYTELEN);

    netio.recv_data(3 - party_id, &d_sum, res->BYTELEN);
    netio.recv_data(3 - party_id, &e_sum, res->BYTELEN);
    d_sum = (d_sum + d) & res->MASK;
    e_sum = (e_sum + e) & res->MASK;

    res->v = (res->z + (d_sum * res->y) + (e_sum * res->x)) & res->MASK;
    if (party_id == 1) {
        res->v = (res->v + (d_sum * e_sum)) & res->MASK;
    }
}

template <typename ShareType>
void ADDshare_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio,
                                   ADDshare_mul_res_v<ShareType> &res, ADDshare_v<ShareType> &s1,
                                   ADDshare_v<ShareType> &s2) {
    int len = res.size();
#ifdef DEBUG_MODE
    if (s1.size() != len || s2.size() != len) {
        error("ADDshare_mul_res_cal_mult_vec: input vector sizes do not match");
    }
    if (res.BITLEN != s1.BITLEN || res.BITLEN != s2.BITLEN) {
        error("ADDshare_mul_res_cal_mult_vec: bitlen mismatch between vectors");
    }
    for (int i = 0; i < len; i++) {
        if (!s1[i].has_shared || !s2[i].has_shared) {
            error("ADDshare_mul_res_cal_mult_vec requires both inputs to have been shared");
        }
        if (s1[i].BITLEN != res[i].BITLEN || s2[i].BITLEN != res[i].BITLEN) {
            error("ADDshare_mul_res_cal_mult_vec: bitlength mismatch between inputs and result");
        }
        res[i].has_shared = true;
    }
#endif
    if (party_id == 0) {
        return;
    }

    std::vector<ShareType> d(len), e(len);
    for (int i = 0; i < len; i++) {
        d[i] = (s1[i].v - res[i].x) & res[i].MASK;
        e[i] = (s2[i].v - res[i].y) & res[i].MASK;
    }
    std::vector<ShareType> d_sum(len), e_sum(len);

    // 发送和接收信息
    for (int i = 0; i < len; i++) {
        netio.store_data(3 - party_id, &d[i], s1[i].BYTELEN);
        netio.store_data(3 - party_id, &e[i], s2[i].BYTELEN);
    }
    netio.send_stored_data(3 - party_id);

    for (int i = 0; i < len; i++) {
        ShareType tmp_d, tmp_e;
        netio.recv_data(3 - party_id, &tmp_d, s1[i].BYTELEN);
        netio.recv_data(3 - party_id, &tmp_e, s2[i].BYTELEN);
        d_sum[i] = (tmp_d + d[i]) & res[i].MASK;
        e_sum[i] = (tmp_e + e[i]) & res[i].MASK;

        res[i].v = (res[i].z + (d_sum[i] * res[i].y) + (e_sum[i] * res[i].x)) & res[i].MASK;
        if (party_id == 1) {
            res[i].v = (res[i].v + (d_sum[i] * e_sum[i])) & res[i].MASK;
        }
    }
}

template <typename ShareType>
ShareType ADDshare_recon(const int party_id, NetIOMP &netio, ADDshare<ShareType> *s) {
#ifdef DEBUG_MODE
    if (!s->has_shared) {
        error("ADDshare_recon requires the input to have been shared");
    }
#endif
    if (party_id == 0) {
        return 0;
    }
    ShareType total;
    netio.send_data(3 - party_id, &s->v, s->BYTELEN);
    netio.recv_data(3 - party_id, &total, s->BYTELEN);
    total = (total + s->v) & s->MASK;
    return total;
}

template <typename ShareType>
inline std::vector<ShareType> ADDshare_recon_vec(const int party_id, NetIOMP &netio,
                                                 ADDshare_v<ShareType> &s) {
    int len = s.size();
#ifdef DEBUG_MODE
    for (int i = 0; i < len; i++) {
        if (!s[i].has_shared) {
            error("ADDshare_recon_vec requires the input to have been shared");
        }
    }
#endif
    if (party_id == 0) {
        return std::vector<ShareType>(len, 0);
    }
    std::vector<ShareType> result(len);
    for (int i = 0; i < len; i++) {
        netio.store_data(3 - party_id, &s[i].v, s[i].BYTELEN);
    }
    netio.send_stored_data(3 - party_id);
    for (int i = 0; i < len; i++) {
        ShareType tmp;
        netio.recv_data(3 - party_id, &tmp, s[i].BYTELEN);
        result[i] = (tmp + s[i].v) & s[i].MASK;
    }
    return result;
}
