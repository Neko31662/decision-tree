#pragma once

#include "protocol/cotp.h"
#include "protocol/tree.h"
#include "utils/permutation.h"

/* 稻草人协议（固定角色版）
 * 约定：
 * 1) S固定为P0，C固定为P1，P2为辅助计算方
 * 2) S输入明文树，C输入明文特征
 * 3) feature_num与d作为公开参数传入（S/C均可预先知道）
 *
 * @param party_id: 当前参与方id，0/1/2
 * @param PRGs: 两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param input_tree: 仅P0提供；其它方传nullptr
 * @param input_x: 仅P1提供；其它方传nullptr
 * @param feature_num: 原始特征长度n（公开）
 * @param d: 原始树深度（根深度记为1，公开）
 * @param output_r: 输出MSS分享结果
 * @param ell: 计算环比特宽度
 */
void PI_strawman(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                 const plain_tree *input_tree, const std::vector<ShareValue> *input_x,
                 int feature_num, int d, MSSshare &output_r, int ell);

#include "protocol/strawman.tpp"
