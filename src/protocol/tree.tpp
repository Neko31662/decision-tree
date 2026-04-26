inline plain_tree::plain_tree(int node_num, ShareValue root_alpha) : alpha(root_alpha) {
#ifdef DEBUG_MODE
    if (node_num < 0) {
        error("plain_tree: node_num must be non-negative");
    }
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
    assert_shape_valid();
#endif
}

inline void plain_tree::resize(int node_num) {
#ifdef DEBUG_MODE
    if (node_num < 0) {
        error("plain_tree::resize: node_num must be non-negative");
    }
#endif
    t.resize(node_num, 0);
    L.resize(node_num, INVALID_INDEX);
    R.resize(node_num, INVALID_INDEX);
    Lp.resize(node_num, INVALID_INDEX);
    Rp.resize(node_num, INVALID_INDEX);
}

inline void plain_tree::clear() {
    alpha = 0;
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
