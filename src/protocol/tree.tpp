#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>

namespace plain_tree_dot_detail {

inline std::string trim_copy(const std::string &s) {
    size_t l = 0;
    size_t r = s.size();
    while (l < r && std::isspace((unsigned char)s[l])) {
        l++;
    }
    while (r > l && std::isspace((unsigned char)s[r - 1])) {
        r--;
    }
    return s.substr(l, r - l);
}

inline bool parse_first_int(const std::string &s, int &out) {
    size_t i = 0;
    while (i < s.size() && !std::isdigit((unsigned char)s[i]) && s[i] != '-') {
        i++;
    }
    if (i >= s.size()) {
        return false;
    }
    size_t j = i + 1;
    while (j < s.size() && std::isdigit((unsigned char)s[j])) {
        j++;
    }
    try {
        out = std::stoi(s.substr(i, j - i));
    } catch (...) {
        return false;
    }
    return true;
}

inline bool parse_feature_threshold(const std::string &label, int &feat_idx, double &threshold) {
    size_t p = label.find("X[");
    if (p == std::string::npos) {
        return false;
    }
    size_t q = label.find(']', p + 2);
    if (q == std::string::npos) {
        return false;
    }
    size_t le = label.find("<=", q + 1);
    if (le == std::string::npos) {
        return false;
    }
    std::string feat_str = trim_copy(label.substr(p + 2, q - (p + 2)));
    std::string thr_str = label.substr(le + 2);
    size_t end = thr_str.find('\n');
    if (end != std::string::npos) {
        thr_str = thr_str.substr(0, end);
    }
    thr_str = trim_copy(thr_str);
    try {
        feat_idx = std::stoi(feat_str);
        threshold = std::stod(thr_str);
    } catch (...) {
        return false;
    }
    return true;
}

inline bool parse_value_numbers(const std::string &label, std::vector<double> &values,
                                bool &is_vector_value) {
    values.clear();
    is_vector_value = false;
    size_t p = label.find("value =");
    if (p == std::string::npos) {
        return false;
    }
    p += 7;
    while (p < label.size() && std::isspace((unsigned char)label[p])) {
        p++;
    }
    if (p >= label.size()) {
        return false;
    }

    if (label[p] == '[') {
        is_vector_value = true;
        size_t q = label.find(']', p + 1);
        if (q == std::string::npos) {
            return false;
        }
        std::string body = label.substr(p + 1, q - (p + 1));
        std::string cur;
        for (char ch : body) {
            if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '.' || ch == 'e' ||
                ch == 'E') {
                cur.push_back(ch);
            } else if (!cur.empty()) {
                try {
                    values.push_back(std::stod(cur));
                } catch (...) {
                    return false;
                }
                cur.clear();
            }
        }
        if (!cur.empty()) {
            try {
                values.push_back(std::stod(cur));
            } catch (...) {
                return false;
            }
        }
        return !values.empty();
    }

    size_t q = p;
    while (q < label.size() && !std::isspace((unsigned char)label[q])) {
        q++;
    }
    try {
        values.push_back(std::stod(label.substr(p, q - p)));
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace plain_tree_dot_detail

inline plain_tree::plain_tree(int node_num, ShareValue root_alpha) : alpha(root_alpha) {
#ifdef DEBUG_MODE
    if (node_num < 0) {
        error("plain_tree: node_num must be non-negative");
    }
    prepared = false;
#endif
    t.resize(node_num, 0);
    L.resize(node_num, INVALID_INDEX);
    R.resize(node_num, INVALID_INDEX);
    Lp.resize(node_num, INVALID_INDEX);
    Rp.resize(node_num, INVALID_INDEX);
}

inline plain_tree::plain_tree(ShareValue root_alpha, const std::vector<ShareValue> &t_vec,
                              const std::vector<ShareValue> &L_vec,
                              const std::vector<ShareValue> &R_vec,
                              const std::vector<ShareValue> &Lp_vec,
                              const std::vector<ShareValue> &Rp_vec)
    : alpha(root_alpha), t(t_vec), L(L_vec), R(R_vec), Lp(Lp_vec), Rp(Rp_vec) {
#ifdef DEBUG_MODE
    prepared = false;
    assert_shape_valid();
#endif
}

inline void plain_tree::resize(int node_num) {
#ifdef DEBUG_MODE
    if (node_num < 0) {
        error("plain_tree::resize: node_num must be non-negative");
    }
    prepared = false;
#endif
    t.resize(node_num, 0);
    L.resize(node_num, INVALID_INDEX);
    R.resize(node_num, INVALID_INDEX);
    Lp.resize(node_num, INVALID_INDEX);
    Rp.resize(node_num, INVALID_INDEX);
}

inline void plain_tree::clear() {
    alpha = 0;
#ifdef DEBUG_MODE
    prepared = false;
#endif
    t.clear();
    L.clear();
    R.clear();
    Lp.clear();
    Rp.clear();
}

inline bool plain_tree::shape_valid() const {
    int n = (int)t.size();
    return (int)L.size() == n && (int)R.size() == n && (int)Lp.size() == n && (int)Rp.size() == n;
}

inline void plain_tree::assert_shape_valid() const {
    if (!shape_valid()) {
        error("plain_tree: shape mismatch among t/L/R/Lp/Rp vectors");
    }
}

inline void plain_tree::set_node(int idx, ShareValue t_i, ShareValue L_i, ShareValue R_i,
                                 ShareValue Lp_i, ShareValue Rp_i) {
#ifdef DEBUG_MODE
    assert_shape_valid();
    if (idx < 0 || idx >= size()) {
        error("plain_tree::set_node: idx out of range");
    }
#endif
    t[idx] = t_i;
    L[idx] = L_i;
    R[idx] = R_i;
    Lp[idx] = Lp_i;
    Rp[idx] = Rp_i;
}

inline bool plain_tree::is_leaf_before_dummy(int idx) const {
#ifdef DEBUG_MODE
    assert_shape_valid();
    if (idx < 0 || idx >= size()) {
        error("plain_tree::is_leaf_before_dummy: idx out of range");
    }
#endif
    return L[idx] == INVALID_INDEX && R[idx] == INVALID_INDEX && Lp[idx] == INVALID_INDEX &&
           Rp[idx] == INVALID_INDEX;
}

inline ShareValue plain_tree::evaluation(const std::vector<ShareValue> &x) const {
#ifdef DEBUG_MODE
    assert_shape_valid();
    if (prepared) {
        error("plain_tree::evaluation: prepared tree is not supported by plain evaluation");
    }
    if (empty()) {
        error("plain_tree::evaluation: tree is empty");
    }
    if (x.empty()) {
        error("plain_tree::evaluation: feature vector is empty");
    }
    if (alpha == INVALID_INDEX || alpha >= (ShareValue)x.size()) {
        error("plain_tree::evaluation: alpha out of feature range");
    }
#endif

    int cur = 0;
    ShareValue feat_idx = alpha;

#ifdef DEBUG_MODE
    std::vector<char> visited(size(), 0);
    int step = 0;
#endif

    while (true) {
#ifdef DEBUG_MODE
        if (cur < 0 || cur >= size()) {
            error("plain_tree::evaluation: node index out of range");
        }
        if (visited[cur]) {
            error("plain_tree::evaluation: cycle detected, dummyized tree is not supported");
        }
        visited[cur] = 1;
        step++;
        if (step > size()) {
            error("plain_tree::evaluation: path too long, dummyized tree is not supported");
        }
#endif

        bool all_invalid = (L[cur] == INVALID_INDEX && R[cur] == INVALID_INDEX &&
                            Lp[cur] == INVALID_INDEX && Rp[cur] == INVALID_INDEX);
        bool all_valid = (L[cur] != INVALID_INDEX && R[cur] != INVALID_INDEX &&
                          Lp[cur] != INVALID_INDEX && Rp[cur] != INVALID_INDEX);

#ifdef DEBUG_MODE
        if (!all_invalid && !all_valid) {
            error("plain_tree::evaluation: node has mixed valid/invalid child metadata");
        }
#endif

        // 未dummy化语义下，叶子节点直接在t中存放标签
        if (all_invalid) {
            return t[cur];
        }

#ifdef DEBUG_MODE
        if (feat_idx == INVALID_INDEX || feat_idx >= (ShareValue)x.size()) {
            error("plain_tree::evaluation: feature index out of range");
        }
        if (L[cur] >= (ShareValue)size() || R[cur] >= (ShareValue)size()) {
            error("plain_tree::evaluation: child index out of range");
        }
        if (Lp[cur] >= (ShareValue)x.size() || Rp[cur] >= (ShareValue)x.size()) {
            error("plain_tree::evaluation: next feature index out of range");
        }
#endif

        bool go_left = (x[(size_t)feat_idx] < t[cur]);
        ShareValue next_feat_idx = go_left ? Lp[cur] : Rp[cur];
        int next_node = go_left ? (int)L[cur] : (int)R[cur];
        cur = next_node;
        feat_idx = next_feat_idx;
    }
}

inline void plain_tree::prepare(int feature_num) {
#ifdef DEBUG_MODE
    assert_shape_valid();
    if (empty()) {
        error("plain_tree::prepare: tree is empty");
    }
    if (prepared) {
        error("plain_tree::prepare: tree has already been prepared");
    }
#endif

    const int m = size();

    int n = feature_num;
    if (n <= 0) {
        n = (alpha == INVALID_INDEX) ? 0 : ((int)alpha + 1);
        for (int i = 0; i < m; i++) {
            bool left_valid = (L[i] != INVALID_INDEX);
            bool right_valid = (R[i] != INVALID_INDEX);
#ifdef DEBUG_MODE
            if (left_valid != (Lp[i] != INVALID_INDEX)) {
                error("plain_tree::prepare: left child/feature metadata mismatch");
            }
            if (right_valid != (Rp[i] != INVALID_INDEX)) {
                error("plain_tree::prepare: right child/feature metadata mismatch");
            }
#endif
            if (left_valid) {
                n = std::max(n, (int)Lp[i] + 1);
            }
            if (right_valid) {
                n = std::max(n, (int)Rp[i] + 1);
            }
        }
    }

#ifdef DEBUG_MODE
    if (n <= 0) {
        error("plain_tree::prepare: invalid feature_num");
    }
    if (alpha == INVALID_INDEX || alpha >= (ShareValue)n) {
        error("plain_tree::prepare: alpha out of feature range");
    }
#endif

    int d = 0;
#ifdef DEBUG_MODE
    std::vector<char> color(m, 0);
#endif
    std::function<void(int, ShareValue, ShareValue, int)> dfs =
        [&](int u, ShareValue feat_idx, ShareValue path_sum, int depth) {
#ifdef DEBUG_MODE
            if (u < 0 || u >= m) {
                error("plain_tree::prepare: node index out of range");
            }
            if (color[u] == 1) {
                error("plain_tree::prepare: cycle detected");
            }
            if (color[u] == 2) {
                error("plain_tree::prepare: node has multiple parents");
            }
            color[u] = 1;
#endif
            d = std::max(d, depth);

            bool left_valid = (L[u] != INVALID_INDEX);
            bool right_valid = (R[u] != INVALID_INDEX);
#ifdef DEBUG_MODE
            if (left_valid != (Lp[u] != INVALID_INDEX)) {
                error("plain_tree::prepare: left child/feature metadata mismatch");
            }
            if (right_valid != (Rp[u] != INVALID_INDEX)) {
                error("plain_tree::prepare: right child/feature metadata mismatch");
            }
#endif

            if (!left_valid && !right_valid) {
                // 真实叶子改写为 (t_e - 路径和, m, m, n, n)
                t[u] = t[u] - path_sum;
                L[u] = (ShareValue)m;
                R[u] = (ShareValue)m;
                Lp[u] = (ShareValue)n;
                Rp[u] = (ShareValue)n;
#ifdef DEBUG_MODE
                color[u] = 2;
#endif
                return;
            }

#ifdef DEBUG_MODE
            if (feat_idx == INVALID_INDEX || feat_idx >= (ShareValue)n) {
                error("plain_tree::prepare: feature index out of range");
            }
            if (left_valid && (L[u] >= (ShareValue)m || Lp[u] >= (ShareValue)n)) {
                error("plain_tree::prepare: left child index out of range");
            }
            if (right_valid && (R[u] >= (ShareValue)m || Rp[u] >= (ShareValue)n)) {
                error("plain_tree::prepare: right child index out of range");
            }
#endif

            ShareValue next_sum = path_sum + t[u];
            // 非叶节点若某侧为空，则改为指向首个dummy节点(m)与虚拟特征位(n)。
            if (left_valid) {
                dfs((int)L[u], Lp[u], next_sum, depth + 1);
            } else {
                L[u] = (ShareValue)m;
                Lp[u] = (ShareValue)n;
            }
            if (right_valid) {
                dfs((int)R[u], Rp[u], next_sum, depth + 1);
            } else {
                R[u] = (ShareValue)m;
                Rp[u] = (ShareValue)n;
            }
#ifdef DEBUG_MODE
            color[u] = 2;
#endif
        };

    dfs(0, alpha, 0, 1);

#ifdef DEBUG_MODE
    for (int i = 0; i < m; i++) {
        if (color[i] == 0) {
            error("plain_tree::prepare: disconnected node detected");
        }
    }
#endif

    // 追加 d-2 个dummy节点：N'_i = (0, i+m+1, i+m+1, i+n+1, i+n+1)
    int dummy_cnt = (d >= 2) ? (d - 2) : 0;
    t.reserve(m + dummy_cnt);
    L.reserve(m + dummy_cnt);
    R.reserve(m + dummy_cnt);
    Lp.reserve(m + dummy_cnt);
    Rp.reserve(m + dummy_cnt);
    for (int i = 0; i < dummy_cnt; i++) {
        t.push_back(0);
        L.push_back((ShareValue)(m + i + 1));
        R.push_back((ShareValue)(m + i + 1));
        Lp.push_back((ShareValue)(n + i + 1));
        Rp.push_back((ShareValue)(n + i + 1));
    }

#ifdef DEBUG_MODE
    prepared = true;
#endif
}

inline ShareValue plain_tree::scale_to_share(double value, double scale) {
#ifdef DEBUG_MODE
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        error("plain_tree::scale_to_share: scale must be finite and positive");
    }
    if (!std::isfinite(value)) {
        error("plain_tree::scale_to_share: value must be finite");
    }
#endif
    long double scaled = (long double)value * (long double)scale;
    long long rounded = (long long)std::llround(scaled);
    return (ShareValue)rounded;
}

inline plain_tree plain_tree::load_from_sklearn_dot(const std::string &file_path, double scale,
                                                    int *feature_num_out) {
#ifdef DEBUG_MODE
    if (file_path.empty()) {
        error("plain_tree::load_from_sklearn_dot: file_path is empty");
    }
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        error("plain_tree::load_from_sklearn_dot: scale must be finite and positive");
    }
#endif

    struct ParsedNode {
        bool exists = false;
        bool is_internal = false;
        int feature = -1;
        double threshold = 0.0;
        std::vector<double> value_nums;
        bool value_is_vector = false;
    };
    struct ParsedEdge {
        int left = -1;
        int right = -1;
        std::vector<int> unlabeled;
    };

    std::ifstream fin(file_path);
    if (!fin.is_open()) {
        error("plain_tree::load_from_sklearn_dot: cannot open file " + file_path);
    }

    std::unordered_map<int, ParsedNode> nodes;
    std::unordered_map<int, ParsedEdge> edges;

    auto parse_node_label = [&](int node_id, const std::string &label) {
        ParsedNode &pn = nodes[node_id];
        pn.exists = true;

        int feat = -1;
        double thr = 0.0;
        bool is_internal = plain_tree_dot_detail::parse_feature_threshold(label, feat, thr);
        pn.is_internal = is_internal;
        if (is_internal) {
            pn.feature = feat;
            pn.threshold = thr;
        }

        std::vector<double> vals;
        bool is_vec = false;
        if (plain_tree_dot_detail::parse_value_numbers(label, vals, is_vec)) {
            pn.value_nums = std::move(vals);
            pn.value_is_vector = is_vec;
        }
    };

    std::string line;
    while (std::getline(fin, line)) {
        std::string s = plain_tree_dot_detail::trim_copy(line);
        if (s.empty()) {
            continue;
        }

        // edge: u -> v ...
        size_t arrow = s.find("->");
        if (arrow != std::string::npos) {
            int u = -1, v = -1;
            if (!plain_tree_dot_detail::parse_first_int(s.substr(0, arrow), u)) {
                continue;
            }
            if (!plain_tree_dot_detail::parse_first_int(s.substr(arrow + 2), v)) {
                continue;
            }
            ParsedEdge &pe = edges[u];
            bool is_true = (s.find("headlabel=\"True\"") != std::string::npos);
            bool is_false = (s.find("headlabel=\"False\"") != std::string::npos);
            if (is_true) {
                pe.left = v;
            } else if (is_false) {
                pe.right = v;
            } else {
                pe.unlabeled.push_back(v);
            }
            continue;
        }

        // node: id [label="..."] ;
        size_t lbl = s.find("[label=\"");
        if (lbl != std::string::npos) {
            int node_id = -1;
            if (!plain_tree_dot_detail::parse_first_int(s.substr(0, lbl), node_id)) {
                continue;
            }

            std::string label_buf;
            std::string chunk = s.substr(lbl + 8);
            while (true) {
                size_t end = chunk.find("\"]");
                if (end != std::string::npos) {
                    label_buf += chunk.substr(0, end);
                    break;
                }
                label_buf += chunk;
                label_buf.push_back('\n');
                if (!std::getline(fin, chunk)) {
                    break;
                }
            }

            parse_node_label(node_id, label_buf);
            continue;
        }
    }
    fin.close();

    if (nodes.empty()) {
        error("plain_tree::load_from_sklearn_dot: no nodes parsed from file " + file_path);
    }
    if (nodes.find(0) == nodes.end()) {
        error("plain_tree::load_from_sklearn_dot: root node 0 not found");
    }

    int max_node_id = -1;
    for (const auto &kv : nodes) {
        max_node_id = std::max(max_node_id, kv.first);
    }
    plain_tree tree(max_node_id + 1, 0);

    int max_feat_idx = -1;
    if (nodes[0].is_internal) {
        tree.alpha = (ShareValue)nodes[0].feature;
        max_feat_idx = std::max(max_feat_idx, nodes[0].feature);
    } else {
        tree.alpha = 0;
    }

    for (int i = 0; i <= max_node_id; i++) {
        auto it = nodes.find(i);
        if (it == nodes.end()) {
            error("plain_tree::load_from_sklearn_dot: missing node id " + std::to_string(i));
        }
        ParsedNode &pn = it->second;

        if (pn.is_internal) {
            ParsedEdge pe;
            auto eit = edges.find(i);
            if (eit != edges.end()) {
                pe = eit->second;
            }

            int cur = 0;
            if (pe.left < 0 && cur < (int)pe.unlabeled.size()) {
                pe.left = pe.unlabeled[cur++];
            }
            if (pe.right < 0 && cur < (int)pe.unlabeled.size()) {
                pe.right = pe.unlabeled[cur++];
            }
#ifdef DEBUG_MODE
            if (pe.left < 0 || pe.right < 0) {
                error("plain_tree::load_from_sklearn_dot: internal node missing children at id " +
                      std::to_string(i));
            }
            if (pe.left > max_node_id || pe.right > max_node_id) {
                error("plain_tree::load_from_sklearn_dot: child index out of range");
            }
#endif

            int lf = pn.feature;
            int rf = pn.feature;
            auto lit = nodes.find(pe.left);
            if (lit != nodes.end() && lit->second.is_internal) {
                lf = lit->second.feature;
            }
            auto rit = nodes.find(pe.right);
            if (rit != nodes.end() && rit->second.is_internal) {
                rf = rit->second.feature;
            }

            max_feat_idx = std::max(max_feat_idx, pn.feature);
            max_feat_idx = std::max(max_feat_idx, lf);
            max_feat_idx = std::max(max_feat_idx, rf);

            tree.set_node(i, scale_to_share(pn.threshold, scale), (ShareValue)pe.left,
                          (ShareValue)pe.right, (ShareValue)lf, (ShareValue)rf);
        } else {
            ShareValue leaf_label = 0;
            if (!pn.value_nums.empty()) {
                if (pn.value_is_vector) {
                    int best = 0;
                    double best_v = pn.value_nums[0];
                    for (int k = 1; k < (int)pn.value_nums.size(); k++) {
                        if (pn.value_nums[k] > best_v) {
                            best_v = pn.value_nums[k];
                            best = k;
                        }
                    }
                    leaf_label = (ShareValue)best;
                } else {
                    leaf_label = scale_to_share(pn.value_nums[0], scale);
                }
            }
            tree.set_node(i, leaf_label, INVALID_INDEX, INVALID_INDEX, INVALID_INDEX, INVALID_INDEX);
        }
    }

    if (feature_num_out != nullptr) {
        *feature_num_out = std::max(max_feat_idx + 1, 1);
    }

#ifdef DEBUG_MODE
    tree.prepared = false;
#endif
    return tree;
}
