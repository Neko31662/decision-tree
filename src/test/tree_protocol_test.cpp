#include "protocol/share_party_init.h"
#include "protocol/tree_protocol.h"
#include <iostream>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;

using namespace std;

static plain_tree build_tree_a() {
    // 深度3
    plain_tree tree(5, 0);
    tree.set_node(0, 10, 1, 2, 1, 2);
    tree.set_node(1, 5, 3, 4, 3, 4);
    tree.set_node(2, 200, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    tree.set_node(3, 10, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    tree.set_node(4, 20, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    return tree;
}

static plain_tree build_tree_b() {
    // 非平衡树，最大深度4；保证0~4号特征在(alpha,L',R')集合中均至少出现一次
    plain_tree tree(7, 0);
    tree.set_node(0, 8, 1, 2, 1, 3);
    tree.set_node(1, 3, 3, 4, 2, 2);
    tree.set_node(2, 90, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    tree.set_node(3, 5, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    tree.set_node(4, 6, 5, 6, 3, 4); // 这里引入特征4
    tree.set_node(5, 30, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    tree.set_node(6, 40, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                  plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
    return tree;
}

static plain_tree build_large_stress_tree(int feature_num) {
    // 满二叉结构：m0=10001(万级)，内部节点5000，叶子5001，深度14
    const int m0 = 10001;
    const int internal = (m0 - 1) / 2;
    plain_tree tree(m0, 0);

    for (int i = 0; i < m0; i++) {
        if (i < internal) {
            int l = 2 * i + 1;
            int r = 2 * i + 2;
            ShareValue thr =
                (ShareValue)((i * 1315423911ULL + 2654435761ULL) & ((1ULL << 20) - 1));
            // 2*internal == feature_num，确保每个特征至少出现一次
            ShareValue lp = (ShareValue)((2 * i) % feature_num);
            ShareValue rp = (ShareValue)((2 * i + 1) % feature_num);
            tree.set_node(i, thr, (ShareValue)l, (ShareValue)r, lp, rp);
        } else {
            ShareValue label =
                (ShareValue)((i * 11400714819323198485ULL + 889523592379ULL) & ((1ULL << 20) - 1));
            tree.set_node(i, label, plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX,
                          plain_tree::INVALID_INDEX, plain_tree::INVALID_INDEX);
        }
    }
    return tree;
}

int main(int argc, char **argv) {
    int party_id = atoi(argv[1]);
    init_share_party_refs(party_id);
    NetIOMP *netio = new NetIOMP(party_id, BASE_PORT, NUM_PARTIES);
    netio->sync();

    block seed1;
    block seed2;
    if (party_id == 0) {
        seed1 = gen_seed();
        seed2 = gen_seed();
        netio->send_data(1, &seed1, sizeof(block));
        netio->send_data(2, &seed2, sizeof(block));
    } else if (party_id == 1) {
        netio->recv_data(0, &seed1, sizeof(block));
        seed2 = gen_seed();
        netio->send_data(2, &seed2, sizeof(block));
    } else if (party_id == 2) {
        netio->recv_data(0, &seed1, sizeof(block));
        netio->recv_data(1, &seed2, sizeof(block));
    }
    vector<PRGSync> PRGs = {PRGSync(&seed1), PRGSync(&seed2)};

    const int ell = 31;
    vector<plain_tree> trees = {build_tree_a(), build_tree_b()};
    vector<int> depths = {3, 4};
    vector<vector<vector<ShareValue>>> x_cases = {
        {
            {8, 4, 0, 0, 0},  // -> 10
            {3, 7, 0, 0, 0},  // -> 20
            {15, 1, 0, 0, 0}, // -> 200
        },
        {
            {1, 2, 3, 4, 5},  // -> 5
            {2, 9, 1, 0, 0},  // -> 30
            {2, 9, 7, 0, 0},  // -> 40
            {9, 0, 0, 0, 0},  // -> 90
        }};

    for (int ti = 0; ti < (int)trees.size(); ti++) {
        plain_tree base = trees[ti];
        int feature_num = (int)x_cases[ti][0].size();
        int d = depths[ti];
        plain_tree prepared = base;
        prepared.prepare(feature_num);

        for (int k = 0; k < (int)x_cases[ti].size(); k++) {
            const vector<ShareValue> &x = x_cases[ti][k];
            ShareValue expect = base.evaluation(x);

            const plain_tree *tree_ptr = (party_id == 0) ? &prepared : nullptr;
            const vector<ShareValue> *x_ptr = (party_id == 1) ? &x : nullptr;

            MSSshare out(ell);
            PI_tree_protocol_intermediate inter(ell, feature_num, d);
            PI_tree_protocol_preprocess(party_id, PRGs, *netio, inter, tree_ptr, x_ptr);
            PI_tree_protocol(party_id, PRGs, *netio, inter, out);
            ShareValue rec = MSSshare_recon(party_id, *netio, &out);

            if (party_id == 0 && rec != expect) {
                cout << "PI_tree_protocol mismatch: tree=" << ti << ", case=" << k
                     << ", got=" << (unsigned long long)rec
                     << ", expect=" << (unsigned long long)expect << endl;
                return 1;
            }
        }
    }

    // 万级压力测试
    {
        const int feature_num = 10000;
        const int d = 14;
        plain_tree base = build_large_stress_tree(feature_num);
        plain_tree prepared = base;
        prepared.prepare(feature_num);

        vector<ShareValue> x(feature_num, 0);
        for (int i = 0; i < feature_num; i++) {
            x[i] = (ShareValue)((i * 17ULL + 3ULL) & ((1ULL << 20) - 1));
        }
        ShareValue expect = base.evaluation(x);

        const plain_tree *tree_ptr = (party_id == 0) ? &prepared : nullptr;
        const vector<ShareValue> *x_ptr = (party_id == 1) ? &x : nullptr;
        MSSshare out(ell);
        PI_tree_protocol_intermediate inter(ell, feature_num, d);
        PI_tree_protocol_preprocess(party_id, PRGs, *netio, inter, tree_ptr, x_ptr);
        PI_tree_protocol(party_id, PRGs, *netio, inter, out);
        ShareValue rec = MSSshare_recon(party_id, *netio, &out);

        if (party_id == 0 && rec != expect) {
            cout << "PI_tree_protocol stress mismatch: got=" << (unsigned long long)rec
                 << ", expect=" << (unsigned long long)expect << endl;
            return 1;
        }
    }

    cout << "PI_tree_protocol test passed!" << endl;
    delete netio;
    return 0;
}
