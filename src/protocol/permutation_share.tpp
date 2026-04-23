inline MSSshare_permutation::MSSshare_permutation(int len, int ell) : shares(len, ell) {}

template <typename ShareType>
inline ADDshare_permutation<ShareType>::ADDshare_permutation(int len, int ell)
    : shares(len, ell) {}

namespace permutation_share_detail {

/* 检查明文向量是否为[0, n-1]上的一个排列
 * @param vals: 待检查明文向量
 */
template <typename T> inline bool is_plain_permutation_0_to_n_1(const std::vector<T> &vals) {
    int n = (int)vals.size();
    std::vector<char> seen(n, 0);
    for (int i = 0; i < n; i++) {
        if (vals[i] >= (T)n) {
            return false;
        }
        int idx = (int)vals[i];
        if (seen[idx]) {
            return false;
        }
        seen[idx] = 1;
    }
    return true;
}

} // namespace permutation_share_detail

inline void MSSshare_permutation_recon(const int party_id, NetIOMP &netio, MSSshare_permutation &s,
                                       std::vector<ShareValue> &out) {
    out = MSSshare_recon_vec(party_id, netio, s.vec());
#ifdef DEBUG_MODE
    if (!permutation_share_detail::is_plain_permutation_0_to_n_1(out)) {
        error("MSSshare_permutation_recon: reconstructed result is not a permutation [0, n-1]");
    }
#endif
}

template <typename ShareType>
inline void ADDshare_permutation_recon(const int party_id, NetIOMP &netio,
                                       ADDshare_permutation<ShareType> &s,
                                       std::vector<ShareType> &out) {
    out = ADDshare_recon_vec(party_id, netio, s.vec());
#ifdef DEBUG_MODE
    // ADDshare重构中，party0按当前语义拿到的是全0占位值，不参与排列合法性检查。
    if (party_id != 0 && !permutation_share_detail::is_plain_permutation_0_to_n_1(out)) {
        error("ADDshare_permutation_recon: reconstructed result is not a permutation [0, n-1]");
    }
#endif
}
