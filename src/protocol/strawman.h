#pragma once

#include "protocol/cotp.h"
#include "protocol/tree.h"
#include "utils/permutation.h"
#include <memory>

struct PI_strawman_intermediate {
    int ell;
    int feature_num;
    int d;
    int node_total = 0;
    int feat_total = 0;
    ShareValue I0 = 0;
    ShareValue I1 = 0;

    std::unique_ptr<MSSshare_v> sh_t;
    std::unique_ptr<MSSshare_v> sh_L;
    std::unique_ptr<MSSshare_v> sh_R;
    std::unique_ptr<MSSshare_v> sh_Lp;
    std::unique_ptr<MSSshare_v> sh_Rp;
    std::unique_ptr<MSSshare_v> sh_x;
    std::vector<PI_cotp_intermediate> cotp_inters;
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_strawman_intermediate(int ell, int feature_num, int d)
        : ell(ell), feature_num(feature_num), d(d) {}
};

/* 稻草人协议预处理（固定角色版）
 * 约定：
 * 1) S固定为P0，C固定为P1，P2为辅助计算方
 * 2) S输入“已prepare”的明文树，C输入明文特征
 * 3) feature_num与d作为公开参数传入（S/C均可预先知道）
 *
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param intermediate: 稻草人协议中间体（保存预处理结果）
 * @param input_tree: 仅P0提供；其它方传nullptr。要求该树已在函数外完成prepare
 * @param input_x: 仅P1提供；其它方传nullptr
 */
void PI_strawman_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                            PI_strawman_intermediate &intermediate, const plain_tree *input_tree,
                            const std::vector<ShareValue> *input_x);

/* 稻草人协议在线阶段（固定角色版）
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param intermediate: 稻草人协议中间体（须先完成preprocess）
 * @param output_r: 输出MSS分享结果
 */
void PI_strawman(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 PI_strawman_intermediate &intermediate, MSSshare &output_r);

#include "protocol/strawman.tpp"
