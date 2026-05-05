#include "protocol/share_party_init.h"
#include "protocol/strawman.h"
#include "protocol/tree_protocol.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;

using namespace std;

struct SpeedRow {
    string dataset;
    string protocol;
    double preprocess_ms = 0.0;
    double online_ms = 0.0;
};

/* 列出目录下全部数据集文件（按文件名排序） */
static vector<string> list_dataset_files(const string &dir_path) {
    namespace fs = std::filesystem;
    vector<string> files;
    for (const auto &entry : fs::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        files.push_back(entry.path().string());
    }
    sort(files.begin(), files.end());
    return files;
}

/* 计算未prepare树深度（根深度记为1） */
static int compute_tree_depth(const plain_tree &tree) {
    if (tree.empty()) {
        return 0;
    }
    int m = tree.size();
    int max_depth = 1;
    vector<char> vis(m, 0);
    stack<pair<int, int>> st;
    st.push({0, 1});

    while (!st.empty()) {
        auto [u, dep] = st.top();
        st.pop();
        if (u < 0 || u >= m) {
            continue;
        }
        if (vis[u]) {
            continue;
        }
        vis[u] = 1;
        max_depth = max(max_depth, dep);

        bool is_leaf = (tree.L[u] == plain_tree::INVALID_INDEX && tree.R[u] == plain_tree::INVALID_INDEX &&
                        tree.Lp[u] == plain_tree::INVALID_INDEX &&
                        tree.Rp[u] == plain_tree::INVALID_INDEX);
        if (!is_leaf) {
            if (tree.L[u] != plain_tree::INVALID_INDEX) {
                st.push({(int)tree.L[u], dep + 1});
            }
            if (tree.R[u] != plain_tree::INVALID_INDEX) {
                st.push({(int)tree.R[u], dep + 1});
            }
        }
    }
    return max_depth;
}

/* 将树中实际使用到的特征索引重映射为[0, k-1]连续编号 */
static int compact_feature_indices(plain_tree &tree, int fallback_feature_num) {
    vector<int> used;
    used.reserve(tree.size() * 2 + 1);
    used.push_back((int)tree.alpha);
    for (int i = 0; i < tree.size(); i++) {
        if (tree.Lp[i] != plain_tree::INVALID_INDEX) {
            used.push_back((int)tree.Lp[i]);
        }
        if (tree.Rp[i] != plain_tree::INVALID_INDEX) {
            used.push_back((int)tree.Rp[i]);
        }
    }
    sort(used.begin(), used.end());
    used.erase(unique(used.begin(), used.end()), used.end());

    unordered_map<int, int> mp;
    mp.reserve(used.size() * 2 + 1);
    for (int i = 0; i < (int)used.size(); i++) {
        mp[used[i]] = i;
    }

    if (mp.find((int)tree.alpha) == mp.end()) {
        return max(fallback_feature_num, 1);
    }
    tree.alpha = (ShareValue)mp[(int)tree.alpha];
    for (int i = 0; i < tree.size(); i++) {
        if (tree.Lp[i] != plain_tree::INVALID_INDEX) {
            tree.Lp[i] = (ShareValue)mp[(int)tree.Lp[i]];
        }
        if (tree.Rp[i] != plain_tree::INVALID_INDEX) {
            tree.Rp[i] = (ShareValue)mp[(int)tree.Rp[i]];
        }
    }

    int compact_n = (int)used.size();
    if (compact_n <= 0) {
        compact_n = max(fallback_feature_num, 1);
    }
    return compact_n;
}

/* 生成固定输入（带符号风格），确保各次复现实验一致 */
static vector<ShareValue> build_input_x(int feature_num, uint64_t seed_tag) {
    vector<ShareValue> x(feature_num, 0);
    uint64_t s = seed_tag ^ 0x9e3779b97f4a7c15ULL;
    for (int i = 0; i < feature_num; i++) {
        s ^= (s << 7);
        s ^= (s >> 9);
        s ^= (s << 8);
        long long v = (long long)(s % 200001ULL) - 100000LL;
        x[i] = (ShareValue)v;
    }
    return x;
}

/* 汇总三方耗时并在P0取max作为该阶段耗时 */
static double collect_max_time_ms(int party_id, NetIOMP &netio, double local_ms) {
    if (party_id == 0) {
        double t1 = 0.0, t2 = 0.0;
        netio.recv_data(1, &t1, sizeof(double));
        netio.recv_data(2, &t2, sizeof(double));
        return max(local_ms, max(t1, t2));
    }
    netio.send_data(0, &local_ms, sizeof(double));
    return local_ms;
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

    // 使用64位环，避免负数阈值在prepare阶段被截断到更小bit宽。
    const int ell = 31;
    const double scale = 1000.0;

    string dt_dir = string(PROJECT_SOURCE_DIR) + "/dt";
    vector<string> dataset_files = list_dataset_files(dt_dir);
    if (dataset_files.empty()) {
        cout << "tree_speed_test: no dataset files found in " << dt_dir << endl;
        delete netio;
        return 1;
    }

    vector<SpeedRow> rows;
    rows.reserve(dataset_files.size() * 2);

    for (int ds_idx = 0; ds_idx < (int)dataset_files.size(); ds_idx++) {
        string file_path = dataset_files[ds_idx];
        string dataset_name = std::filesystem::path(file_path).filename().string();

        int feature_num_loaded = 0;
        plain_tree base = plain_tree::load_from_sklearn_dot(file_path, scale, &feature_num_loaded);
        int feature_num = compact_feature_indices(base, feature_num_loaded);
        int d = compute_tree_depth(base);
        if (d <= 0) {
            d = 1;
        }

        vector<ShareValue> input_x = build_input_x(feature_num, (uint64_t)ds_idx + 1);

        // ========== Strawman ==========
        {
            plain_tree prepared_tree = base;
            prepared_tree.prepare(feature_num);

            const plain_tree *tree_ptr = (party_id == 0) ? &prepared_tree : nullptr;
            const vector<ShareValue> *x_ptr = (party_id == 1) ? &input_x : nullptr;

            MSSshare out(ell);
            PI_strawman_intermediate inter(ell, feature_num, d);

            auto t0 = std::chrono::steady_clock::now();
            PI_strawman_preprocess(party_id, PRGs, *netio, inter, tree_ptr, x_ptr);
            auto t1 = std::chrono::steady_clock::now();
            PI_strawman(party_id, PRGs, *netio, inter, out);
            auto t2 = std::chrono::steady_clock::now();

            double pre_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double on_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
            double pre_max = collect_max_time_ms(party_id, *netio, pre_ms);
            double on_max = collect_max_time_ms(party_id, *netio, on_ms);

            // 不计入online时间，只用于保障输出可重构
            (void)MSSshare_recon(party_id, *netio, &out);

            if (party_id == 0) {
                rows.push_back({dataset_name, "strawman", pre_max, on_max});
            }
        }

        // ========== Tree Protocol ==========
        {
            plain_tree prepared_tree = base;
            prepared_tree.prepare(feature_num);

            const plain_tree *tree_ptr = (party_id == 0) ? &prepared_tree : nullptr;
            const vector<ShareValue> *x_ptr = (party_id == 1) ? &input_x : nullptr;

            MSSshare out(ell);
            PI_tree_protocol_intermediate inter(ell, feature_num, d);

            auto t0 = std::chrono::steady_clock::now();
            PI_tree_protocol_preprocess(party_id, PRGs, *netio, inter, tree_ptr, x_ptr);
            auto t1 = std::chrono::steady_clock::now();
            PI_tree_protocol(party_id, PRGs, *netio, inter, out);
            auto t2 = std::chrono::steady_clock::now();

            double pre_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double on_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
            double pre_max = collect_max_time_ms(party_id, *netio, pre_ms);
            double on_max = collect_max_time_ms(party_id, *netio, on_ms);

            (void)MSSshare_recon(party_id, *netio, &out);

            if (party_id == 0) {
                rows.push_back({dataset_name, "tree", pre_max, on_max});
            }
        }
    }

    if (party_id == 0) {
        cout << "\n================ Tree Speed Summary ================\n";
        cout << left << setw(16) << "Dataset" << setw(12) << "Protocol" << setw(16)
             << "Preprocess(ms)" << setw(14) << "Online(ms)" << setw(12) << "Total(ms)"
             << "\n";
        cout << string(70, '-') << "\n";
        for (const auto &r : rows) {
            double total = r.preprocess_ms + r.online_ms;
            cout << left << setw(16) << r.dataset << setw(12) << r.protocol << setw(16) << fixed
                 << setprecision(3) << r.preprocess_ms << setw(14) << r.online_ms << setw(12)
                 << total << "\n";
        }
        cout << string(70, '=') << "\n";
    }

    delete netio;
    return 0;
}
