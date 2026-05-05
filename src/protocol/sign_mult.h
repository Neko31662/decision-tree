#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "utils/misc.h"
#include "utils/permutation.h"

struct PI_sign_mult_intermediate {
    int ell;
    ShareValue p;
    ShareValue mask;
    MSSshare r_prime;
    ShareValue Gamma = 0;
    std::vector<ShareValue> rx_list;
    bool Delta = false;
    std::vector<MSSshare> r_double_prime_list;
    std::vector<MSSshare> ry_rz_share_list;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_sign_mult_intermediate(int ell, int y_size)
        : p(nxt_prime(ell)), mask((ell == ShareValue_BitLength) ? ~ShareValue(0)
                                                                 : ((ShareValue(1) << ell) - 1)),
          r_prime(ell), rx_list(ell, 0), r_double_prime_list(y_size, MSSshare(ell)),
          ry_rz_share_list(y_size, MSSshare(ell)) {
        this->ell = ell;
    }
};

/* 预处理 PI_sign_mult 协议
 * 输入/输出全部采用 MSSshare（mod 2^ell）
 */
void PI_sign_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                             PI_sign_mult_intermediate &intermediate, MSSshare *input_x,
                             std::vector<MSSshare *> &input_y_vec, MSSshare *output_z,
                             std::vector<MSSshare *> &output_w_vec);

/* 预处理 PI_sign_mult 协议（批量版）
 * 说明：多个实例共享一轮 P1<->P2 的 Gamma 还原通信。
 */
void PI_sign_mult_preprocess_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 std::vector<PI_sign_mult_intermediate *> &intermediate_vec,
                                 std::vector<MSSshare *> &input_x_vec,
                                 std::vector<std::vector<MSSshare *>> &input_y_vec_vec,
                                 std::vector<MSSshare *> &output_z_vec,
                                 std::vector<std::vector<MSSshare *>> &output_w_vec_vec);

/* 在线执行 PI_sign_mult 协议
 */
void PI_sign_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                  PI_sign_mult_intermediate &intermediate, MSSshare *input_x,
                  std::vector<MSSshare *> &input_y_vec, MSSshare *output_z,
                  std::vector<MSSshare *> &output_w_vec);

#include "protocol/sign_mult.tpp"
