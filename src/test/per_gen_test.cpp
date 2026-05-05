#include "protocol/per_gen.h"
#include "protocol/share_party_init.h"
#include <algorithm>
#include <iostream>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_rounds = 120;

using namespace std;

/* 检查明文向量是否为[0, n-1]上的排列
 * @param vals: 待检查向量
 */
static bool is_perm_0_to_n_1(const vector<ShareValue> &vals) {
    int n = (int)vals.size();
    vector<ShareValue> sorted = vals;
    sort(sorted.begin(), sorted.end());
    for (int i = 0; i < n; i++) {
        if (sorted[i] != (ShareValue)i) {
            return false;
        }
    }
    return true;
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

    vector<int> lens = {1, 2, 9, 31};
    for (int len : lens) {
        bool seen_non_identity = false;
        for (int round = 0; round < test_rounds; round++) {
            MSSshare_permutation perm_share(1, 1);
            PI_pergen(party_id, PRGs, *netio, len, perm_share);

            vector<ShareValue> rec_perm;
            MSSshare_permutation_recon(party_id, *netio, perm_share, rec_perm);

            if (party_id == 0) {
                if ((int)rec_perm.size() != len || !is_perm_0_to_n_1(rec_perm)) {
                    cout << "PI_pergen permutation validity failed at len=" << len
                         << ", round=" << round << endl;
                    return 1;
                }
                if (len > 1) {
                    bool is_identity = true;
                    for (int i = 0; i < len; i++) {
                        if (rec_perm[i] != (ShareValue)i) {
                            is_identity = false;
                            break;
                        }
                    }
                    if (!is_identity) {
                        seen_non_identity = true;
                    }
                }
            }
        }
        if (party_id == 0 && len > 1 && !seen_non_identity) {
            cout << "PI_pergen seems always identity at len=" << len << endl;
            return 1;
        }
    }

    // 显式指定ell测试
    {
        const int len = 19;
        const int ell = 31;
        MSSshare_permutation perm_share(1, 1);
        PI_pergen(party_id, PRGs, *netio, len, perm_share, ell);
        vector<ShareValue> rec_perm;
        MSSshare_permutation_recon(party_id, *netio, perm_share, rec_perm);
        if (party_id == 0) {
            if (perm_share.vec().BITLEN != ell || !is_perm_0_to_n_1(rec_perm)) {
                cout << "PI_pergen explicit ell failed" << endl;
                return 1;
            }
        }
    }

    // 万级压力测试
    {
        const int len = 10000;
        MSSshare_permutation perm_share(1, 1);
        PI_pergen(party_id, PRGs, *netio, len, perm_share);
        vector<ShareValue> rec_perm;
        MSSshare_permutation_recon(party_id, *netio, perm_share, rec_perm);
        if (party_id == 0) {
            if ((int)rec_perm.size() != len || !is_perm_0_to_n_1(rec_perm)) {
                cout << "PI_pergen large case failed" << endl;
                return 1;
            }
        }
    }

    cout << "PI_pergen test passed!" << endl;
    delete netio;
    return 0;
}
