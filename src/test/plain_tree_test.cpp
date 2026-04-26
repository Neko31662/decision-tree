#include "protocol/tree.h"
#include <iostream>
#include <vector>

using namespace std;

int main() {
    // 基础构造与节点设置
    plain_tree tree(5, 0);
    tree.set_node(0, 10, 1, 2, 1, 2); // 根：比较x[0]与10
    tree.set_node(1, 5, 3, 4, 3, 4);  // 左子树：比较x[1]与5
    tree.set_node(2, 200, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX); // 叶子
    tree.set_node(3, 10, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX); // 叶子
    tree.set_node(4, 20, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX); // 叶子

    if (!tree.shape_valid()) {
        cout << "shape_valid failed" << endl;
        return 1;
    }
    if (!tree.is_leaf_before_dummy(2) || !tree.is_leaf_before_dummy(3) ||
        !tree.is_leaf_before_dummy(4)) {
        cout << "is_leaf_before_dummy failed" << endl;
        return 1;
    }
    if (tree.is_leaf_before_dummy(0) || tree.is_leaf_before_dummy(1)) {
        cout << "is_leaf_before_dummy false positive" << endl;
        return 1;
    }

    // 路径1：x0 < 10 且 x1 < 5 -> label = 10
    {
        vector<ShareValue> x = {8, 4, 0, 0, 0};
        ShareValue y = tree.evaluation(x);
        if (y != 10) {
            cout << "evaluation case1 failed, got " << (unsigned long long)y << endl;
            return 1;
        }
    }

    // 路径2：x0 < 10 且 x1 >= 5 -> label = 20
    {
        vector<ShareValue> x = {3, 7, 0, 0, 0};
        ShareValue y = tree.evaluation(x);
        if (y != 20) {
            cout << "evaluation case2 failed, got " << (unsigned long long)y << endl;
            return 1;
        }
    }

    // 路径3：x0 >= 10 -> label = 200
    {
        vector<ShareValue> x = {15, 1, 2, 3, 4};
        ShareValue y = tree.evaluation(x);
        if (y != 200) {
            cout << "evaluation case3 failed, got " << (unsigned long long)y << endl;
            return 1;
        }
    }

    // 向量构造函数测试
    {
        vector<ShareValue> t = {9, 111, 222};
        vector<ShareValue> L = {1, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX};
        vector<ShareValue> R = {2, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX};
        vector<ShareValue> Lp = {0, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX};
        vector<ShareValue> Rp = {0, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX};
        plain_tree tree2(0, t, L, R, Lp, Rp);
        vector<ShareValue> x = {8};
        if (tree2.evaluation(x) != 111) {
            cout << "vector ctor evaluation failed (left)" << endl;
            return 1;
        }
        x[0] = 10;
        if (tree2.evaluation(x) != 222) {
            cout << "vector ctor evaluation failed (right)" << endl;
            return 1;
        }
    }

    // clear / resize 基础测试
    tree.clear();
    if (!tree.empty()) {
        cout << "clear failed" << endl;
        return 1;
    }
    tree.resize(2);
    if (tree.size() != 2 || !tree.shape_valid()) {
        cout << "resize failed" << endl;
        return 1;
    }

    cout << "plain_tree test passed!" << endl;
    return 0;
}

