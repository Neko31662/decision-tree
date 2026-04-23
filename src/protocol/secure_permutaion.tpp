void PI_secure_permutation(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           MSSshare_permutation &perm_share, MSSshare_v &input_y,
                           MSSshare_v &output_y) {
    int n = perm_share.size();
#ifdef DEBUG_MODE
    if (n != input_y.size() || n != output_y.size()) {
        error("PI_secure_permutation: size mismatch among permutation/input/output");
    }
    for (int i = 0; i < n; i++) {
        if (!perm_share[i].has_shared) {
            error("PI_secure_permutation: permutation input must be shared");
        }
        if (!input_y[i].has_shared) {
            error("PI_secure_permutation: vector input must be shared");
        }
    }
#endif

    MSSshare_v shuffled_perm(n, perm_share.vec().BITLEN);
    MSSshare_v shuffled_y(n, input_y.BITLEN);

    // 第1步：对排列与数据同步执行同一随机混洗
    std::vector<MSSshare_v *> in_list = {&perm_share.vec(), &input_y};
    std::vector<MSSshare_v *> out_list = {&shuffled_perm, &shuffled_y};
    PI_shuffle_sync(party_id, PRGs, netio, in_list, out_list);

    // 第2步：公开混洗后的排列，再局部计算其逆排列
    MSSshare_permutation shuffled_perm_wrap(n, perm_share.vec().BITLEN);
    shuffled_perm_wrap.vec() = shuffled_perm;
    std::vector<ShareValue> opened_perm;
    MSSshare_permutation_recon(party_id, netio, shuffled_perm_wrap, opened_perm);
    std::vector<int> opened_perm_int(n, 0);
    for (int i = 0; i < n; i++) {
        opened_perm_int[i] = (int)opened_perm[i];
    }
    std::vector<int> inv_opened_perm = inv_permutation(opened_perm_int);

    // 第3步：局部重排混洗后的数据，得到最终置换结果
    std::vector<MSSshare> local = shuffled_y.data;
    local = apply_permutation(inv_opened_perm, local);
    for (int i = 0; i < n; i++) {
        output_y[i] = local[i];
    }
}

void PI_secure_permutation(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           MSSshare_permutation &perm_share, MSSshare_v &input_output_y) {
    MSSshare_v out_vec(input_output_y.size(), input_output_y.BITLEN);
    PI_secure_permutation(party_id, PRGs, netio, perm_share, input_output_y, out_vec);
    input_output_y = out_vec;
}
