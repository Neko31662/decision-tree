#pragma once

#ifdef USE_DICF
#include "protocol/cotp_DICF.h"
using PI_tree_cotp_intermediate = PI_cotp_DICF_intermediate;
#else
#include "protocol/cotp.h"
using PI_tree_cotp_intermediate = PI_cotp_intermediate;
#endif
#include "protocol/per_gen.h"
#include "protocol/secure_permutaion.h"
#include "protocol/tree.h"
#include "utils/permutation.h"
#include <memory>

struct PI_tree_protocol_intermediate {
    int ell;
    int feature_num;
    int d;
    int m = 0;
    int n = 0;
    ShareValue I0 = 0;
    ShareValue I1 = 0;

    std::unique_ptr<MSSshare_v> t_perm;
    std::unique_ptr<MSSshare_v> L_perm;
    std::unique_ptr<MSSshare_v> R_perm;
    std::unique_ptr<MSSshare_v> Lp_perm;
    std::unique_ptr<MSSshare_v> Rp_perm;
    std::unique_ptr<MSSshare_v> sh_x;
    std::vector<PI_tree_cotp_intermediate> cotp_inters;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_tree_protocol_intermediate(int ell, int feature_num, int d)
        : ell(ell), feature_num(feature_num), d(d) {}
};

/* PI_tree 协议预处理（固定角色版）
 * 约定：
 * 1) S 固定为 P0，输入必须是“已prepare”的决策树；
 * 2) C 固定为 P1，输入原始特征向量（长度为 n0）；
 * 3) d 与 n0 作为公开参数传入，协议内部按 n=n0+d-2 处理特征总长度。
 *
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param intermediate: 协议中间体（保存预处理结果）
 * @param input_tree: 仅P0提供，且必须已prepare；其余方传nullptr
 * @param input_x: 仅P1提供原始特征向量；其余方传nullptr
 */
void PI_tree_protocol_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                                 PI_tree_protocol_intermediate &intermediate,
                                 const plain_tree *input_tree,
                                 const std::vector<ShareValue> *input_x);

/* PI_tree 协议在线阶段（固定角色版）
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param intermediate: 协议中间体（须先完成preprocess）
 * @param output_r: 输出MSS分享结果
 */
void PI_tree_protocol(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                      PI_tree_protocol_intermediate &intermediate, MSSshare &output_r);

#include "protocol/tree_protocol.tpp"
