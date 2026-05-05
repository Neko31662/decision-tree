#include "protocol/secure_permutaion.h"
#include "protocol/share_party_init.h"
#include <algorithm>
#include <iostream>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_rounds = 80;

using namespace std;

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
    block private_seed = gen_seed();
    PRGSync private_PRG(&private_seed);
    block public_seed;
    if (party_id == 0) {
        public_seed = gen_seed();
        netio->send_data(1, &public_seed, sizeof(block));
        netio->send_data(2, &public_seed, sizeof(block));
    } else {
        netio->recv_data(0, &public_seed, sizeof(block));
    }
    PRGSync public_PRG(&public_seed);

    // 1) MSSshare_permutation recon 测试
    {
        const int len = 11;
        const int ell = 8;
        MSSshare_permutation perm_share(len, ell);
        vector<ShareValue> perm_plain(len, 0);
        if (party_id == 0) {
            auto p = gen_random_permutation(len, private_PRG);
            for (int i = 0; i < len; i++) {
                perm_plain[i] = p[i];
            }
        }
        for (int i = 0; i < len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &perm_share[i]);
            MSSshare_share_from(0, party_id, *netio, &perm_share[i], perm_plain[i]);
        }
        vector<ShareValue> rec_perm;
        MSSshare_permutation_recon(party_id, *netio, perm_share, rec_perm);
        if (party_id == 0 && rec_perm != perm_plain) {
            cout << "MSSshare_permutation_recon failed" << endl;
            return 1;
        }
    }

    // 2) ADDshare_permutation recon 测试
    {
        const int len = 9;
        const int ell = 8;
        ADDshare_permutation<> perm_share(len, ell);
        vector<ShareValue> perm_plain(len, 0);
        auto p = gen_random_permutation(len, public_PRG);
        for (int i = 0; i < len; i++) {
            perm_plain[i] = p[i];
        }
        for (int i = 0; i < len; i++) {
            ADDshare_share_from(0, party_id, PRGs, *netio, &perm_share[i], perm_plain[i]);
        }
        vector<ShareValue> rec_perm;
        ADDshare_permutation_recon(party_id, *netio, perm_share, rec_perm);
        if ((party_id == 1 || party_id == 2) && rec_perm != perm_plain) {
            cout << "ADDshare_permutation_recon failed" << endl;
            return 1;
        }
    }

    // 3) PI_secure_permutation 测试
    vector<int> lens = {1, 2, 13, 25};
    for (int len : lens) {
        for (int round = 0; round < test_rounds; round++) {
            const int perm_ell = 8;
            const int data_ell = 31;
            MSSshare_permutation perm_share(len, perm_ell);
            MSSshare_v data_share(len, data_ell);
            MSSshare_v out_share(len, data_ell);

            vector<ShareValue> perm_plain(len, 0);
            vector<ShareValue> data_plain(len, 0);
            if (party_id == 0) {
                auto p = gen_random_permutation(len, private_PRG);
                for (int i = 0; i < len; i++) {
                    perm_plain[i] = p[i];
                    private_PRG.gen_random_data(&data_plain[i], sizeof(ShareValue));
                    data_plain[i] &= data_share[i].MASK;
                }
            }

            for (int i = 0; i < len; i++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &perm_share[i]);
                MSSshare_preprocess(0, party_id, PRGs, *netio, &data_share[i]);
                MSSshare_share_from(0, party_id, *netio, &perm_share[i], perm_plain[i]);
                MSSshare_share_from(0, party_id, *netio, &data_share[i], data_plain[i]);
            }

            PI_secure_permutation(party_id, PRGs, *netio, perm_share, data_share, out_share);

            vector<ShareValue> rec(len, 0);
            for (int i = 0; i < len; i++) {
                rec[i] = MSSshare_recon(party_id, *netio, &out_share[i]);
            }

            if (party_id == 0) {
                vector<int> perm_int(len);
                for (int i = 0; i < len; i++) {
                    perm_int[i] = (int)perm_plain[i];
                }
                auto inv_perm = inv_permutation(perm_int);
                vector<ShareValue> expect(len, 0);
                for (int i = 0; i < len; i++) {
                    expect[i] = data_plain[inv_perm[i]];
                }
                if (rec != expect) {
                    cout << "PI_secure_permutation failed at len=" << len << ", round=" << round
                         << endl;
                    return 1;
                }
            }
        }
    }

    // 4) PI_secure_permutation 原地版本测试
    {
        const int len = 17;
        const int perm_ell = 16;
        const int data_ell = 31;
        MSSshare_permutation perm_share(len, perm_ell);
        MSSshare_v data_share(len, data_ell);
        vector<ShareValue> perm_plain(len, 0), data_plain(len, 0);

        if (party_id == 0) {
            auto p = gen_random_permutation(len, private_PRG);
            for (int i = 0; i < len; i++) {
                perm_plain[i] = p[i];
                private_PRG.gen_random_data(&data_plain[i], sizeof(ShareValue));
                data_plain[i] &= data_share[i].MASK;
            }
        }

        for (int i = 0; i < len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &perm_share[i]);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &data_share[i]);
            MSSshare_share_from(0, party_id, *netio, &perm_share[i], perm_plain[i]);
            MSSshare_share_from(0, party_id, *netio, &data_share[i], data_plain[i]);
        }

        PI_secure_permutation(party_id, PRGs, *netio, perm_share, data_share);
        vector<ShareValue> rec(len, 0);
        for (int i = 0; i < len; i++) {
            rec[i] = MSSshare_recon(party_id, *netio, &data_share[i]);
        }

        if (party_id == 0) {
            vector<int> perm_int(len);
            for (int i = 0; i < len; i++) {
                perm_int[i] = (int)perm_plain[i];
            }
            auto inv_perm = inv_permutation(perm_int);
            vector<ShareValue> expect(len, 0);
            for (int i = 0; i < len; i++) {
                expect[i] = data_plain[inv_perm[i]];
            }
            if (rec != expect) {
                cout << "PI_secure_permutation in-place failed" << endl;
                return 1;
            }
        }
    }

    // 5) 万级压力测试
    {
        const int len = 10000;
        const int perm_ell = 16;
        const int data_ell = 31;
        MSSshare_permutation perm_share(len, perm_ell);
        MSSshare_v data_share(len, data_ell);
        MSSshare_v out_share(len, data_ell);
        vector<ShareValue> perm_plain(len, 0), data_plain(len, 0);

        if (party_id == 0) {
            auto p = gen_random_permutation(len, private_PRG);
            for (int i = 0; i < len; i++) {
                perm_plain[i] = p[i];
                private_PRG.gen_random_data(&data_plain[i], sizeof(ShareValue));
                data_plain[i] &= data_share[i].MASK;
            }
        }

        for (int i = 0; i < len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &perm_share[i]);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &data_share[i]);
            MSSshare_share_from(0, party_id, *netio, &perm_share[i], perm_plain[i]);
            MSSshare_share_from(0, party_id, *netio, &data_share[i], data_plain[i]);
        }

        PI_secure_permutation(party_id, PRGs, *netio, perm_share, data_share, out_share);
        vector<ShareValue> rec(len, 0);
        for (int i = 0; i < len; i++) {
            rec[i] = MSSshare_recon(party_id, *netio, &out_share[i]);
        }
        if (party_id == 0) {
            vector<int> perm_int(len);
            for (int i = 0; i < len; i++) {
                perm_int[i] = (int)perm_plain[i];
            }
            auto inv_perm = inv_permutation(perm_int);
            vector<ShareValue> expect(len, 0);
            for (int i = 0; i < len; i++) {
                expect[i] = data_plain[inv_perm[i]];
            }
            if (rec != expect) {
                cout << "PI_secure_permutation large case failed" << endl;
                return 1;
            }
        }
    }

    cout << "PI_secure_permutation test passed!" << endl;
    delete netio;
    return 0;
}
