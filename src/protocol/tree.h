#pragma once

#include "config/config.h"
#include "utils/logs.h"
#include <functional>
#include <string>
#include <vector>

class plain_tree {
  public:
    // 未dummy化叶子节点在索引类字段中的占位值（例如原始输入中的-1）
    static inline const ShareValue INVALID_INDEX = ~ShareValue(0);

    // 根节点所用的特征索引 alpha
    ShareValue alpha = 0;
#ifdef DEBUG_MODE
    // 是否已执行prepare（用于区分prepare前后的树）
    bool prepared = false;
#endif

    // 节点字段向量：N_i = (t_i, L_i, R_i, L'_i, R'_i)
    std::vector<ShareValue> t;
    std::vector<ShareValue> L;
    std::vector<ShareValue> R;
    std::vector<ShareValue> Lp;
    std::vector<ShareValue> Rp;

    /* 构造空明文决策树 */
    plain_tree() = default;

    /* 按节点数构造明文决策树
     * @param node_num: 节点个数
     * @param root_alpha: 根节点特征索引alpha
     */
    plain_tree(int node_num, ShareValue root_alpha = 0);

    /* 由完整字段向量构造明文决策树
     * @param root_alpha: 根节点特征索引alpha
     * @param t_vec: 阈值/标签向量t
     * @param L_vec: 左子节点索引向量L
     * @param R_vec: 右子节点索引向量R
     * @param Lp_vec: 左路径对应特征索引向量L'
     * @param Rp_vec: 右路径对应特征索引向量R'
     */
    plain_tree(ShareValue root_alpha, const std::vector<ShareValue> &t_vec,
               const std::vector<ShareValue> &L_vec, const std::vector<ShareValue> &R_vec,
               const std::vector<ShareValue> &Lp_vec, const std::vector<ShareValue> &Rp_vec);

    inline int size() const { return (int)t.size(); }
    inline bool empty() const { return t.empty(); }

    /* 重置节点个数
     * @param node_num: 新的节点个数
     */
    void resize(int node_num);

    /* 清空所有节点字段 */
    void clear();

    /* 检查5个字段向量长度是否一致 */
    bool shape_valid() const;

    /* 断言5个字段向量长度一致，不一致则报错退出 */
    void assert_shape_valid() const;

    /* 设置根节点特征索引alpha
     * @param root_alpha: 根节点特征索引
     */
    inline void set_root_alpha(ShareValue root_alpha) { alpha = root_alpha; }

    /* 设置第idx个节点的5个字段
     * @param idx: 节点下标
     * @param t_i: 阈值/标签
     * @param L_i: 左子节点索引
     * @param R_i: 右子节点索引
     * @param Lp_i: 左路径对应特征索引
     * @param Rp_i: 右路径对应特征索引
     */
    void set_node(int idx, ShareValue t_i, ShareValue L_i, ShareValue R_i, ShareValue Lp_i,
                  ShareValue Rp_i);

    /* 判断节点是否为“未dummy化叶子”格式
     * 判定规则：L_i、R_i、L'_i、R'_i均为INVALID_INDEX
     * @param idx: 节点下标
     */
    bool is_leaf_before_dummy(int idx) const;

    /* 对明文特征向量执行决策树推理，输出标签
     * 说明：当前仅支持dummy化之前的树结构；在DEBUG_MODE下，若检测到疑似dummy化结构会报错
     * @param x: 明文特征向量
     */
    ShareValue evaluation(const std::vector<ShareValue> &x) const;

    /* 对明文树执行prepare：叶子阈值差分化 + 追加dummy链
     * 说明：
     * 1) 按给定协议生成d-2个dummy节点并追加到树尾
     * 2) 将真实叶子改写为 (t_e - 路径和, m, m, n, n)
     * 3) 时间复杂度O(树大小)
     * @param feature_num: 特征数n；若<=0则从当前树中自动推断
     */
    void prepare(int feature_num = -1);

    /* 将浮点数按固定scale转换为ShareValue整数表示
     * @param value: 待转换浮点值
     * @param scale: 缩放因子（例如1000表示保留3位小数）
     */
    static ShareValue scale_to_share(double value, double scale = 1000.0);

    /* 读取sklearn导出的dot决策树并转换为plain_tree
     * 语法假设：符合export_graphviz常见格式（节点label中含X[i] <= threshold与value字段）
     * 转换规则：
     * 1) 内部节点阈值t按round(threshold * scale)缩放为整数；
     * 2) 叶子节点若value是向量，则标签取argmax下标；若value是标量，则按scale缩放；
     * 3) L'/R'由子节点的划分特征索引推导；若子节点为叶子，则退化为当前节点特征索引。
     *
     * @param file_path: dot文件路径
     * @param scale: 浮点转整数缩放因子
     * @param feature_num_out: 可选输出参数，返回推导出的特征维度（max_feature_index + 1）
     */
    static plain_tree load_from_sklearn_dot(const std::string &file_path, double scale = 1000.0,
                                            int *feature_num_out = nullptr);
};

#include "protocol/tree.tpp"
