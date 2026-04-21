#pragma once

#include "protocol/masked_RSS.h"
#include "utils/permutation.h"

/* 对一个MSSshare向量执行安全混洗，输出新向量
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param x_vec: 输入共享向量，长度为n
 * @param y_vec: 输出共享向量，长度也为n，结果为x_vec的同态随机置换
 */
void PI_shuffle(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                MSSshare_v &x_vec, MSSshare_v &y_vec);

/* 对一个MSSshare向量原地执行安全混洗
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param x_vec: 输入输出共享向量，函数结束后被随机置换
 */
void PI_shuffle(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                MSSshare_v &x_vec);

/* 对多个MSSshare向量执行“同步混洗”，输出到另一组向量
 * 说明：每个输入向量都会被同一组随机置换作用（3轮中每轮的置换都同步一致）
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param x_vec_list: 输入共享向量列表（每个元素指向一个MSSshare_v）
 * @param y_vec_list: 输出共享向量列表（每个元素指向一个MSSshare_v）
 */
void PI_shuffle_sync(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     std::vector<MSSshare_v *> &x_vec_list, std::vector<MSSshare_v *> &y_vec_list);

/* 对多个MSSshare向量原地执行“同步混洗”
 * 说明：对所有输入向量应用完全相同的随机置换
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于两两同步随机性的伪随机生成器数组
 * @param netio: 多方通信接口
 * @param x_vec_list: 输入输出共享向量列表（每个元素指向一个MSSshare_v）
 */
void PI_shuffle_sync(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                     std::vector<MSSshare_v *> &x_vec_list);

#include "protocol/shuffle.tpp"
