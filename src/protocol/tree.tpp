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
