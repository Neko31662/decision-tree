#pragma once

#include "config/config.h"
#include "protocol/addictive_share_p.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

// 这里的加法分享是P1和P2之间的加法分享，P0起第三方辅助作用
template <typename ShareType = ShareValue> class ADDshare {
  public:
    ShareType v = 0;
    static inline ShareType ADDshare::*prev_member = &ADDshare::v;
    static inline ShareType ADDshare::*nxt_member = &ADDshare::v;
#ifdef DEBUG_MODE
    static inline bool party_initialized = false;
#endif
#ifdef DEBUG_MODE
    bool has_shared = false;
#endif
    ShareType MASK;
    int BYTELEN;
    int BITLEN;
    ADDshare(int ell)
        : MASK((ell == sizeof(ShareType) * 8) ? ~ShareType(0) : ((ShareType(1) << ell) - 1)),
          BYTELEN((ell + 7) / 8), BITLEN(ell) {
#ifdef DEBUG_MODE
        if (!party_initialized) {
            error("ADDshare::init_party must be called before constructing ADDshare");
        }
#endif
        if (ell > sizeof(ShareType) * 8) {
            error("ADDshare: ell exceeds ShareType bit length " +
                  std::to_string(sizeof(ShareType) * 8));
        }
    }

    static inline void init_party(int party) {
        (void)party;
        prev_member = &ADDshare::v;
        nxt_member = &ADDshare::v;
#ifdef DEBUG_MODE
        party_initialized = true;
#endif
    }

    inline ShareType &x_prev() noexcept { return this->*prev_member; }
    inline const ShareType &x_prev() const noexcept { return this->*prev_member; }
    inline ShareType &x_nxt() noexcept { return this->*nxt_member; }
    inline const ShareType &x_nxt() const noexcept { return this->*nxt_member; }

    ADDshare &operator=(const ADDshare_p &other) {
#ifdef DEBUG_MODE
        if (other.p != MASK + 1) {
            error("ADDshare operator=: modulus mismatch");
        }
        has_shared = other.has_shared;
#endif
        v = other.v;
        return *this;
    }
};

template <typename ShareType = ShareValue> class ADDshare_mul_res : public ADDshare<ShareType> {
  public:
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    using ADDshare<ShareType>::has_shared;
#endif
    using ADDshare<ShareType>::MASK;
    using ADDshare<ShareType>::BYTELEN;
    using ADDshare<ShareType>::v;

    ShareType x = 0, y = 0, z = 0;

    ADDshare_mul_res(int ell) : ADDshare<ShareType>(ell) {}
};

template <typename ShareType = ShareValue> class ADDshare_v {
  public:
    ShareType MASK;
    int BYTELEN;
    int BITLEN;
    int LEN;
    std::vector<ADDshare<ShareType>> data;

    ADDshare_v(int len, int ell)
        : MASK((ell == sizeof(ShareType) * 8) ? ~ShareType(0) : ((ShareType(1) << ell) - 1)),
          BYTELEN((ell + 7) / 8), BITLEN(ell), LEN(len), data(len, ADDshare<ShareType>(ell)) {}

    inline int size() const { return LEN; }
    inline ADDshare<ShareType> &operator[](int i) { return data[i]; }
    inline const ADDshare<ShareType> &operator[](int i) const { return data[i]; }
};

template <typename ShareType = ShareValue> class ADDshare_mul_res_v {
  public:
    ShareType MASK;
    int BYTELEN;
    int BITLEN;
    int LEN;
    std::vector<ADDshare_mul_res<ShareType>> data;

    ADDshare_mul_res_v(int len, int ell)
        : MASK((ell == sizeof(ShareType) * 8) ? ~ShareType(0) : ((ShareType(1) << ell) - 1)),
          BYTELEN((ell + 7) / 8), BITLEN(ell), LEN(len),
          data(len, ADDshare_mul_res<ShareType>(ell)) {}

    inline int size() const { return LEN; }
    inline ADDshare_mul_res<ShareType> &operator[](int i) { return data[i]; }
    inline const ADDshare_mul_res<ShareType> &operator[](int i) const { return data[i]; }
};

// ===============================================================
// ======================= implementation ========================
// ===============================================================

/* 从secret_holder_id处获取秘密值x的ADDshare分享，存储在s中
 * @param secret_holder_id: 秘密值持有方id，0/1/2
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param s: 存储分享结果的ADDshare对象指针
 * @param x: 待分享的秘密值，仅secret_holder_id方提供有效值
 */
template <typename ShareType>
inline void ADDshare_share_from(const int secret_holder_id, const int party_id,
                                std::vector<PRGSync> &PRGs, NetIOMP &netio, ADDshare<ShareType> *s,
                                ShareType x);

/*与ADDshare_share_from类似，但将分享结果存储在netio的缓存中，等待后续发送，此时secret_holder_id必须为0
 */
template <typename ShareType>
inline void ADDshare_share_from_store(const int party_id, std::vector<PRGSync> &PRGs,
                                      NetIOMP &netio, ADDshare<ShareType> *s, ShareType x);

/* 预处理ADDshare_mul_res对象，由P0为P1和P2生成Beaver三元组
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param res: 预处理的ADDshare_mul_res对象指针
 */
template <typename ShareType>
inline void ADDshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs,
                                        NetIOMP &netio, ADDshare_mul_res<ShareType> *res);

/* 计算两个ADDshare的乘法结果，结果存储在res中，仅P1和P2参与计算
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 乘法结果存储的ADDshare_mul_res对象指针
 * @param s1: 第一个加法分享对象指针
 * @param s2: 第二个加法分享对象指针
 */
template <typename ShareType>
inline void ADDshare_mul_res_cal_mult(const int party_id, NetIOMP &netio,
                                      ADDshare_mul_res<ShareType> *res, ADDshare<ShareType> *s1,
                                      ADDshare<ShareType> *s2);

/* ADDshare_mul_res_cal_mult的向量化版本
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 乘法结果存储的ADDshare_mul_res对象指针
 * @param s1: 第一个加法分享对象指针
 * @param s2: 第二个加法分享对象指针
 */
template <typename ShareType>
inline void ADDshare_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio,
                                          ADDshare_mul_res_v<ShareType> &res,
                                          ADDshare_v<ShareType> &s1,
                                          ADDshare_v<ShareType> &s2);

/* 重构ADDshare对象，返回重构后的秘密值，仅P1,P2参与重构
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的ADDshare对象指针
 */
template <typename ShareType>
inline ShareType ADDshare_recon(const int party_id, NetIOMP &netio, ADDshare<ShareType> *s);

template <typename ShareType>
inline std::vector<ShareType> ADDshare_recon_vec(const int party_id, NetIOMP &netio,
                                                 ADDshare_v<ShareType> &s);

#include "protocol/addictive_share.tpp"
