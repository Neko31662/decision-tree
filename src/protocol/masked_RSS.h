#pragma once

#include "config/config.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"
#include "utils/multi_party_net_io.h"
#include "utils/prg_sync.h"

class MSSshare {
  public:
    // For party 0, v1: r1, v2: r2
    // For party 1, v1: m , v2: r1
    // For party 2, v1: m , v2: r2
    ShareValue v1 = 0, v2 = 0;
    static inline ShareValue MSSshare::*prev_member = &MSSshare::v1;
    static inline ShareValue MSSshare::*nxt_member = &MSSshare::v2;
#ifdef DEBUG_MODE
    static inline bool party_initialized = false;
#endif
#ifdef DEBUG_MODE
    bool has_preprocess = false;
    bool has_shared = false;
#endif
    ShareValue MASK;
    int BYTELEN;
    int BITLEN;

    MSSshare(int ell)
        : MASK((ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1)),
          BYTELEN((ell + 7) / 8), BITLEN(ell) {
#ifdef DEBUG_MODE
        if (!party_initialized) {
            error("MSSshare::init_party must be called before constructing MSSshare");
        }
#endif
        if (ell > ShareValue_BitLength) {
            error("MSSshare only supports ell <= " + std::to_string(ShareValue_BitLength) + " now");
        }
    }

    virtual ~MSSshare() = default;
    MSSshare &operator=(const ShareValue &) = delete;

    static inline void init_party(int party) {
        if (party == 2) {
            prev_member = &MSSshare::v2;
            nxt_member = &MSSshare::v1;
        } else {
            prev_member = &MSSshare::v1;
            nxt_member = &MSSshare::v2;
        }
#ifdef DEBUG_MODE
        party_initialized = true;
#endif
    }

    inline ShareValue &x_prev() noexcept { return this->*prev_member; }
    inline const ShareValue &x_prev() const noexcept { return this->*prev_member; }
    inline ShareValue &x_nxt() noexcept { return this->*nxt_member; }
    inline const ShareValue &x_nxt() const noexcept { return this->*nxt_member; }

    MSSshare &operator=(const MSSshare &other) {
#ifdef DEBUG_MODE
        if (this->BITLEN != other.BITLEN) {
            error("MSSshare operator=: bit-length mismatch");
        }
        this->has_preprocess = other.has_preprocess;
        this->has_shared = other.has_shared;
#endif
        this->v1 = other.v1;
        this->v2 = other.v2;
        return *this;
    }

    MSSshare &operator=(const MSSshare_p &other) {
#ifdef DEBUG_MODE
        if (other.p != this->MASK + 1) {
            error("MSSshare operator=: modulus mismatch");
        }
        this->has_preprocess = other.has_preprocess;
        this->has_shared = other.has_shared;
#endif
        this->v1 = other.v1;
        this->v2 = other.v2;
        return *this;
    }

    MSSshare operator+(const MSSshare &other) const noexcept {
#ifdef DEBUG_MODE
        if (this->BITLEN != other.BITLEN) {
            error("MSSshare operator+: bit-length mismatch");
        }
#endif
        MSSshare res = *this;
        res.v1 = (this->v1 + other.v1) & MASK;
        res.v2 = (this->v2 + other.v2) & MASK;
        return res;
    }

    MSSshare operator-(const MSSshare &other) const noexcept {
#ifdef DEBUG_MODE
        if (this->BITLEN != other.BITLEN) {
            error("MSSshare operator-: bit-length mismatch");
        }
#endif
        MSSshare res = *this;
        res.v1 = (this->v1 - other.v1) & MASK;
        res.v2 = (this->v2 - other.v2) & MASK;
        return res;
    }

    MSSshare operator*(ShareValue plain_val) const noexcept {
        MSSshare res = *this;
        res.v1 = (this->v1 * plain_val) & MASK;
        res.v2 = (this->v2 * plain_val) & MASK;
        return res;
    }
};

class MSSshare_mul_res : public MSSshare {
  public:
    // To calculate s3 = s1 * s2:
    // For party 0, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 1, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_1
    // For party 2, s3.v3 = [ (s1.r1 + s1.r2) * (s2.r1 + s2.r2) ]_2
    ShareValue v3 = 0;

    MSSshare_mul_res(int ell) : MSSshare(ell) {}
    virtual ~MSSshare_mul_res() = default;
    MSSshare_mul_res &operator=(const ShareValue &) = delete;

    MSSshare_mul_res &operator=(const MSSshare_mul_res &other) {
#ifdef DEBUG_MODE
        if (this->BITLEN != other.BITLEN) {
            error("MSSshare_mul_res operator=: bit-length mismatch");
        }
#endif
        memcpy(this, &other, sizeof(MSSshare_mul_res));
        return *this;
    }
};

class MSSshare_v {
  public:
    ShareValue MASK;
    int BYTELEN;
    int BITLEN;
    int LEN;
    std::vector<MSSshare> data;

    MSSshare_v(int len, int ell)
        : MASK((ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1)),
          BYTELEN((ell + 7) / 8), BITLEN(ell), LEN(len), data(len, MSSshare(ell)) {}

    inline int size() const { return LEN; }
    inline MSSshare &operator[](int i) { return data[i]; }
    inline const MSSshare &operator[](int i) const { return data[i]; }

    MSSshare_v operator+(const MSSshare_v &other) const noexcept {
#ifdef DEBUG_MODE
        if (LEN != other.LEN || BITLEN != other.BITLEN) {
            error("MSSshare_v operator+: shape mismatch");
        }
#endif
        MSSshare_v res(LEN, BITLEN);
        for (int i = 0; i < LEN; i++) {
            res.data[i] = data[i] + other.data[i];
        }
        return res;
    }

    MSSshare_v operator-(const MSSshare_v &other) const noexcept {
#ifdef DEBUG_MODE
        if (LEN != other.LEN || BITLEN != other.BITLEN) {
            error("MSSshare_v operator-: shape mismatch");
        }
#endif
        MSSshare_v res(LEN, BITLEN);
        for (int i = 0; i < LEN; i++) {
            res.data[i] = data[i] - other.data[i];
        }
        return res;
    }

    MSSshare_v operator*(ShareValue plain_val) const noexcept {
        MSSshare_v res(LEN, BITLEN);
        for (int i = 0; i < LEN; i++) {
            res.data[i] = data[i] * plain_val;
        }
        return res;
    }
};

class MSSshare_mul_res_v {
  public:
    ShareValue MASK;
    int BYTELEN;
    int BITLEN;
    int LEN;
    std::vector<MSSshare_mul_res> data;

    MSSshare_mul_res_v(int len, int ell)
        : MASK((ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1)),
          BYTELEN((ell + 7) / 8), BITLEN(ell), LEN(len), data(len, MSSshare_mul_res(ell)) {}

    inline int size() const { return LEN; }
    inline MSSshare_mul_res &operator[](int i) { return data[i]; }
    inline const MSSshare_mul_res &operator[](int i) const { return data[i]; }
};

// ===============================================================
// ======================= implementation ========================
// ===============================================================

/* 预处理MSSshare对象
 * @param secret_holder_id: 秘密值持有方id，0/1,2，如果该对象之后不进行share_from，必须传入0
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param s: MSSshare对象指针
 */
inline void MSSshare_preprocess(const int secret_holder_id, const int party_id,
                                std::vector<PRGSync> &PRGs, NetIOMP &netio, MSSshare *s);

/* 预处理MSSshare_mul_res对象
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param res: 预处理的MSSshare_mul_res对象指针
 * @param s1: 第一个作为乘法输入的分享对象的指针
 * @param s2: 第二个作为乘法输入的分享对象的指针
 */
inline void MSSshare_mul_res_preprocess(const int party_id, std::vector<PRGSync> &PRGs,
                                        NetIOMP &netio, MSSshare_mul_res *res, const MSSshare *s1,
                                        const MSSshare *s2);

/* 从secret_holder_id处获取秘密值x的MSSshare分享，存储在s中
 * @param secret_holder_id: 秘密值持有方id，0/1/2
 * @param party_id: 参与方id，0/1/2
 * @param x: 待分享的秘密值，仅secret_holder_id方提供有效值
 * @param netio: 多方通信接口
 * @param s: 存储分享结果的MSSshare对象指针
 */
inline void MSSshare_share_from(const int secret_holder_id, const int party_id, NetIOMP &netio,
                                MSSshare *s, ShareValue x);

/*与MSSshare_share_from类似，但将分享结果存储在netio的缓存中，等待后续发送，此时secret_holder_id必须为0
 */
inline void MSSshare_share_from_store(const int party_id, NetIOMP &netio, MSSshare *s,
                                      ShareValue x);

/*明文加法
 * @param party_id: 参与方id，0/1/2
 * @param s: 存储分享结果的MSSshare对象指针
 * @param x: 待加的明文值
 */
inline void MSSshare_add_plain(const int party_id, MSSshare *s, ShareValue x);

/*密文乘法
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param res: 作为结果输出的MSSshare_mul_res对象指针
 * @param s1: 第一个作为乘法输入的分享对象的指针
 * @param s2: 第二个作为乘法输入的分享对象的指针
 */
inline void MSSshare_mul_res_calc_mul(const int party_id, NetIOMP &netio, MSSshare_mul_res *res,
                                      const MSSshare *s1, const MSSshare *s2);

/*密文乘法的向量化版本
 */
inline void MSSshare_mul_res_calc_mul_vec(const int party_id, NetIOMP &netio,
                                          MSSshare_mul_res_v &res_vec, MSSshare_v &s1_vec,
                                          MSSshare_v &s2_vec);

/* 重构MSSshare对象，返回重构后的秘密值
 * @param party_id: 参与方id，0/1/2
 * @param netio: 多方通信接口
 * @param s: 待重构的MSSshare对象指针
 */
inline ShareValue MSSshare_recon(const int party_id, NetIOMP &netio, MSSshare *s);

inline std::vector<ShareValue> MSSshare_recon_vec(const int party_id, NetIOMP &netio,
                                                  MSSshare_v &s);
#include "protocol/masked_RSS.tpp"
