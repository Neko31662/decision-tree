#pragma once

#include "protocol/addictive_share.h"
#include "protocol/masked_RSS.h"

class MSSshare_permutation {
  public:
    MSSshare_v shares;

    /* 构造一个MSS排列分享容器
     * @param len: 向量长度
     * @param ell: 每个元素的比特长度（模2^ell）
     */
    MSSshare_permutation(int len, int ell);

    inline int size() const { return shares.size(); }
    inline MSSshare &operator[](int i) { return shares[i]; }
    inline const MSSshare &operator[](int i) const { return shares[i]; }
    inline MSSshare_v &vec() { return shares; }
    inline const MSSshare_v &vec() const { return shares; }
};

template <typename ShareType = ShareValue> class ADDshare_permutation {
  public:
    ADDshare_v<ShareType> shares;

    /* 构造一个2方加法排列分享容器
     * @param len: 向量长度
     * @param ell: 每个元素的比特长度（模2^ell）
     */
    ADDshare_permutation(int len, int ell);

    inline int size() const { return shares.size(); }
    inline ADDshare<ShareType> &operator[](int i) { return shares[i]; }
    inline const ADDshare<ShareType> &operator[](int i) const { return shares[i]; }
    inline ADDshare_v<ShareType> &vec() { return shares; }
    inline const ADDshare_v<ShareType> &vec() const { return shares; }
};

/* 重构MSS排列分享
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的MSS排列分享
 * @param out: 重构结果输出向量
 */
void MSSshare_permutation_recon(const int party_id, NetIOMP &netio, MSSshare_permutation &s,
                                std::vector<ShareValue> &out);

/* 重构2方加法排列分享
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的2方加法排列分享
 * @param out: 重构结果输出向量
 */
template <typename ShareType = ShareValue>
void ADDshare_permutation_recon(const int party_id, NetIOMP &netio, ADDshare_permutation<ShareType> &s,
                                std::vector<ShareType> &out);

#include "protocol/permutation_share.tpp"
