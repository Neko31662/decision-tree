#pragma once

#include "protocol/permutation_share.h"
#include "protocol/shuffle.h"

/* 对共享向量执行“指定置换”的安全置换协议
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param perm_share: 共享排列输入（应为[0, n-1]上的排列）
 * @param input_y: 待置换的共享向量
 * @param output_y: 输出共享向量，结果为perm_share作用后的input_y
 */
void PI_secure_permutation(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           MSSshare_permutation &perm_share, MSSshare_v &input_y,
                           MSSshare_v &output_y);

/* 对共享向量原地执行“指定置换”的安全置换协议
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param perm_share: 共享排列输入（应为[0, n-1]上的排列）
 * @param input_output_y: 输入输出共享向量，函数结束后被置换
 */
void PI_secure_permutation(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                           MSSshare_permutation &perm_share, MSSshare_v &input_output_y);

#include "protocol/secure_permutaion.tpp"

