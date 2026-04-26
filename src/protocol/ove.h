#pragma once

#include "protocol/secure_permutaion.h"

/* Oblivious Vector Expansion（OVE）协议
 * 功能：根据S方给定的复制次数a，将共享向量x做“按元素复制扩展”
 * 例如a=(2,1,2,1), x=(x0,x1,x2,x3)时，输出(x0,x0,x1,x2,x2,x3)
 * @param party_id: 参与方id，0/1/2
 * @param s_id: S方id（仅party_id==s_id的一方提供input_a）
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param input_x: 输入共享向量
 * @param output_x: 输出共享向量（函数内会重置长度为sum(a_i)）
 * @param input_a: 复制次数数组，仅S方需要提供；其他方可传nullptr
 */
void PI_OVE(const int party_id, const int s_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
            MSSshare_v &input_x, MSSshare_v &output_x, const std::vector<int> *input_a);

#include "protocol/ove.tpp"

