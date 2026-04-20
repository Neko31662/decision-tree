#pragma once

#include "config/config.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

// 这里的加法分享是P1和P2之间的加法分享，P0起第三方辅助作用
class ADDshare_p {
  public:
    ShareValue v = 0;
    static inline ShareValue ADDshare_p::*prev_member = &ADDshare_p::v;
    static inline ShareValue ADDshare_p::*nxt_member = &ADDshare_p::v;
#ifdef DEBUG_MODE
    static inline bool party_initialized = false;
#endif
    ShareValue p;
#ifdef DEBUG_MODE
    bool has_shared = false;
#endif
    int BITLEN;
    int BYTELEN;

    ADDshare_p(ShareValue p) {
#ifdef DEBUG_MODE
        if (!party_initialized) {
            error("ADDshare_p::init_party must be called before constructing ADDshare_p");
        }
#endif
        this->p = p;
        BITLEN = 0;
        p--;
        while (p) {
            p >>= 1;
            BITLEN++;
        }
        BYTELEN = (BITLEN + 7) / 8;
        if (BITLEN > sizeof(ShareValue) * 8 / 2 && this->p != (ShareValue(1) << BITLEN)) {
            error("ADDshare_p: length of p exceeds ShareValue/2 =" +
                  std::to_string(sizeof(ShareValue) * 8 / 2) + ", and p is not power of 2");
        }
    }
    virtual ~ADDshare_p() = default;
    ADDshare_p &operator=(const ShareValue &) = delete;

    static inline void init_party(int party) {
        (void)party;
        prev_member = &ADDshare_p::v;
        nxt_member = &ADDshare_p::v;
#ifdef DEBUG_MODE
        party_initialized = true;
#endif
    }

    inline ShareValue &x_prev() noexcept { return this->*prev_member; }
    inline const ShareValue &x_prev() const noexcept { return this->*prev_member; }
    inline ShareValue &x_nxt() noexcept { return this->*nxt_member; }
    inline const ShareValue &x_nxt() const noexcept { return this->*nxt_member; }
};

class ADDshare_p_mul_res : public ADDshare_p {
  public:
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    using ADDshare_p::has_shared;
#endif
    using ADDshare_p::BITLEN;
    using ADDshare_p::BYTELEN;
    using ADDshare_p::p;
    using ADDshare_p::v;

    ShareValue x = 0, y = 0, z = 0;

    ADDshare_p_mul_res(ShareValue p) : ADDshare_p(p) {}
    virtual ~ADDshare_p_mul_res() = default;
    ADDshare_p_mul_res &operator=(const ShareValue &) = delete;
};

class ADDshare_p_v {
  public:
    ShareValue p;
    int BITLEN;
    int BYTELEN;
    int LEN;
    std::vector<ADDshare_p> data;

    ADDshare_p_v(int len, ShareValue p) : p(p), BITLEN(0), BYTELEN(0), LEN(len), data(len, p) {
        ShareValue q = p - 1;
        while (q) {
            q >>= 1;
            BITLEN++;
        }
        BYTELEN = (BITLEN + 7) / 8;
    }

    inline int size() const { return LEN; }
    inline ADDshare_p &operator[](int i) { return data[i]; }
    inline const ADDshare_p &operator[](int i) const { return data[i]; }
};

class ADDshare_p_mul_res_v {
  public:
    ShareValue p;
    int BITLEN;
    int BYTELEN;
    int LEN;
    std::vector<ADDshare_p_mul_res> data;

    ADDshare_p_mul_res_v(int len, ShareValue p)
        : p(p), BITLEN(0), BYTELEN(0), LEN(len), data(len, p) {
        ShareValue q = p - 1;
        while (q) {
            q >>= 1;
            BITLEN++;
        }
        BYTELEN = (BITLEN + 7) / 8;
    }

    inline int size() const { return LEN; }
    inline ADDshare_p_mul_res &operator[](int i) { return data[i]; }
    inline const ADDshare_p_mul_res &operator[](int i) const { return data[i]; }
};

// ===============================================================
// ======================= implementation ========================
// ===============================================================

/* 从secret_holder_id处获取秘密值x的ADDshare_p分享，存储在s中
 * @param secret_holder_id: 秘密值持有方id，0/1/2
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param s: 存储分享结果的ADDshare_p对象指针
 * @param x: 待分享的秘密值，仅secret_holder_id方提供有效值
 */
inline void ADDshare_p_share_from(const int secret_holder_id, const int party_id,
                                  std::vector<PRGSync> &PRGs, NetIOMP &netio, ADDshare_p *s,
                                  ShareValue x);

/*与ADDshare_p_share_from类似，但将分享结果存储在netio的缓存中，等待后续发送，此时secret_holder_id必须为0
 */
inline void ADDshare_p_share_from_store(const int party_id, std::vector<PRGSync> &PRGs,
                                        NetIOMP &netio, ADDshare_p *s, ShareValue x);

/* 预处理ADDshare_p_mul_res对象，由P0为P1和P2生成Beaver三元组
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param res: 预处理的ADDshare_p_mul_res对象指针
 */
inline void ADDshare_p_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs,
                                          NetIOMP &netio, ADDshare_p_mul_res *res);

/* 计算两个ADDshare_p的乘法结果，结果存储在res中，仅P1和P2参与计算
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 乘法结果存储的ADDshare_p_mul_res对象指针
 * @param s1: 第一个加法分享对象指针
 * @param s2: 第二个加法分享对象指针
 */
inline void ADDshare_p_mul_res_cal_mult(const int party_id, NetIOMP &netio, ADDshare_p_mul_res *res,
                                        ADDshare_p *s1, ADDshare_p *s2);

/* ADDshare_p_mul_res_cal_mult的向量化版本
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 乘法结果存储的ADDshare_p_mul_res对象指针
 * @param s1: 第一个加法分享对象指针
 * @param s2: 第二个加法分享对象指针
 */
inline void ADDshare_p_mul_res_cal_mult_vec(const int party_id, NetIOMP &netio,
                                            ADDshare_p_mul_res_v &res, ADDshare_p_v &s1,
                                            ADDshare_p_v &s2);

/* 重构ADDshare_p对象，返回重构后的秘密值，仅P1,P2参与重构
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的ADDshare_p对象指针
 */
inline ShareValue ADDshare_p_recon(const int party_id, NetIOMP &netio, ADDshare_p *s);

inline std::vector<ShareValue> ADDshare_p_recon_vec(const int party_id, NetIOMP &netio,
                                ADDshare_p_v &s);

#include "protocol/addictive_share_p.tpp"
