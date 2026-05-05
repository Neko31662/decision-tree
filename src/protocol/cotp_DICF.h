#pragma once

#include "config/config.h"
#include "protocol/dicf.h"
#include "protocol/masked_RSS.h"

/*
 * 基于 DICF 的 COTP 中间体（非向量版）
 */
struct PI_cotp_DICF_intermediate {
    using DicfType = Dicf<ShareValue_BitLength, fss::group::Uint<uint32_t>, fss::prg::ChaCha<4>,
                          uint32_t>;

    struct DealerEvalState {
        bool has_eval = false;
        bool eval_party_is_one = false; // DICF.Eval 的 party 参数：false/true
        DicfType::Key key{};
        ShareValue rho_share = 0;
    };

    int ell;
    ShareValue mask;
    ShareValue tau;
    int nonce[2];
    DicfType dicf;
    DealerEvalState dealer_eval[3];
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    explicit PI_cotp_DICF_intermediate(int ell)
        : ell(ell),
          mask((ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1)),
          tau((ell <= 1) ? 0 : ((ShareValue(1) << (ell - 1)) - 1)), nonce{0x13A5B7C9, 0x271D3E5F},
          dicf(fss::prg::ChaCha<4>(nonce)) {}
};

/* 预处理 PI_cotp_DICF 协议（非向量版）
 * 功能：为 3 个 dealer 分别生成 DICF key，并把对应评估 key 分发给两台评估方。
 * @param party_id 当前参与方编号（0/1/2）
 * @param PRGs 同步 PRG（该接口保持与现有协议一致）
 * @param netio 多方通信接口
 * @param intermediate 中间体，保存后续在线阶段所需的 key 和 rho share
 */
void PI_cotp_DICF_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                             PI_cotp_DICF_intermediate &intermediate);

/* 在线执行 PI_cotp_DICF 协议（非向量版）
 * 功能：计算并公开输出 (x >= y) ? L : R。
 * @param party_id 当前参与方编号（0/1/2）
 * @param PRGs 同步 PRG（该接口保持与现有协议一致）
 * @param netio 多方通信接口
 * @param intermediate 对应的预处理中间体
 * @param input_x 比较输入 x（MSSshare）
 * @param input_y 比较输入 y（MSSshare）
 * @param input_L 左分支候选值 L（MSSshare）
 * @param input_R 右分支候选值 R（MSSshare）
 * @return 公开输出明文
 */
ShareValue PI_cotp_DICF(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                        PI_cotp_DICF_intermediate &intermediate, MSSshare *input_x,
                        MSSshare *input_y, MSSshare *input_L, MSSshare *input_R);

/* 在线执行 PI_cotp_DICF 协议（向量版）
 * 功能：使用同一个比较结果 b=(x>=y)，对每个位置独立选择 (L_i,R_i)。
 * @param party_id 当前参与方编号（0/1/2）
 * @param PRGs 同步 PRG（该接口保持与现有协议一致）
 * @param netio 多方通信接口
 * @param intermediate 对应的预处理中间体
 * @param input_x 比较输入 x（MSSshare）
 * @param input_y 比较输入 y（MSSshare）
 * @param input_L_vec 左分支候选向量
 * @param input_R_vec 右分支候选向量
 * @return 公开输出向量，逐元素为 (x>=y)?L_i:R_i
 */
std::vector<ShareValue> PI_cotp_DICF_vec(const int party_id, std::vector<PRGSync> &PRGs,
                                         NetIOMP &netio, PI_cotp_DICF_intermediate &intermediate,
                                         MSSshare *input_x, MSSshare *input_y,
                                         std::vector<MSSshare *> &input_L_vec,
                                         std::vector<MSSshare *> &input_R_vec);

#include "protocol/cotp_DICF.tpp"
