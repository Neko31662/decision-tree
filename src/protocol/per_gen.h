#pragma once

#include "protocol/permutation_share.h"
#include "protocol/shuffle.h"

/* 生成共享随机排列协议
 * 功能：生成长度为n的随机排列并以MSS形式输出，任何单个参与方都不应知道完整排列
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param n: 排列长度
 * @param output_perm: 输出共享排列（函数内会按n重置长度与比特宽度）
 * @param ell: 元素比特宽度；若<=0则自动使用表示[0,n-1]所需的最小比特宽度
 */
void PI_pergen(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio, int n,
               MSSshare_permutation &output_perm, int ell = -1);

#include "protocol/per_gen.tpp"
