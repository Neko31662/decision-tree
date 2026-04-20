#include "protocol/sign.h"
#include "protocol/share_party_init.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 1000;
const int ell = 63;
const ShareValue k = 67676;

using namespace std;

int main(int argc, char **argv) {
    // net io
    int party_id = atoi(argv[1]);
    init_share_party_refs(party_id);
    NetIOMP *netio = new NetIOMP(party_id, BASE_PORT, NUM_PARTIES);
    netio->sync();

    // setup prg
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
    auto private_seed = gen_seed();
    auto private_PRG = PRGSync(&private_seed);
    block public_seed;
    if (party_id == 0) {
        public_seed = gen_seed();
        netio->send_data(1, &public_seed, sizeof(block));
        netio->send_data(2, &public_seed, sizeof(block));
    } else {
        netio->recv_data(0, &public_seed, sizeof(block));
    }
    PRGSync public_PRG(&public_seed);

    // 非向量测试
    {
        for (int i = 0; i < test_nums; i++) {

            PI_sign_intermediate intermediate(ell, k);
            MSSshare x_share(ell);
            MSSshare_p b_share{k};

            // preprocess
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            PI_sign_preprocess(party_id, PRGs, *netio, intermediate, &x_share, &b_share);
            if (party_id == 0) {
                netio->send_stored_data(1);
                netio->send_stored_data(2);
            }
            if (party_id == 1) {
                netio->send_stored_data(2);
            }

            // share
            ShareValue test_value = i;
            if (i % 3 == 1) {
                test_value = ((ShareValue(1) << ell) - i) & x_share.MASK;
            }
            if (i % 3 == 2) {
                private_PRG.gen_random_data(&test_value, sizeof(ShareValue));
                test_value &= x_share.MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &x_share, test_value);

            // compute
            PI_sign(party_id, PRGs, *netio, intermediate, &x_share, &b_share);

            // reveal
            ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share);
            ShareValue expected = test_value & (ShareValue(1) << (ell - 1)) ? 1 : 0;
            if (rec != expected && party_id == 0) {
                cout << "Test " << i << " failed! Expected: " << expected << ", got: " << rec
                     << endl;
                exit(1);
            }
        }
        cout << "PI_sign test passed!" << endl;
    }

    // 向量测试
    {
        int vec_size = 100;
        for (int i = 0; i < max(1, test_nums / vec_size); i++) {
            int ell_arr[vec_size];
            ShareValue k_arr[vec_size];
            for (int j = 0; j < vec_size; j++) {
                ell_arr[j] = ell - 10 + j;
                if (ell_arr[j] < 1)
                    ell_arr[j] = 1;
                if (ell_arr[j] > ShareValue_BitLength)
                    ell_arr[j] = ShareValue_BitLength;
                k_arr[j] = k * j + 10;
            }
            vector<PI_sign_intermediate> intermediate;
            vector<MSSshare> x_share;
            vector<MSSshare_p> b_share;
            for (int j = 0; j < vec_size; j++) {
                intermediate.emplace_back(ell_arr[j], k_arr[j]);
                x_share.emplace_back(ell_arr[j]);
                b_share.emplace_back(k_arr[j]);
            }

            // preprocess
            for (int j = 0; j < vec_size; j++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share[j]);
                PI_sign_preprocess(party_id, PRGs, *netio, intermediate[j], &x_share[j],
                                   &b_share[j]);
            }
            if (party_id == 0) {
                netio->send_stored_data(1);
                netio->send_stored_data(2);
            }
            if (party_id == 1) {
                netio->send_stored_data(2);
            }

            // share
            ShareValue test_value[vec_size] = {};
            for (int j = 0; j < vec_size; j++) {
                if (i % 3 == 1) {
                    test_value[j] = ((ShareValue(1) << ell_arr[j]) - i) & x_share[j].MASK;
                }
                if (i % 3 == 2) {
                    private_PRG.gen_random_data(&test_value, sizeof(ShareValue));
                    test_value[j] &= x_share[j].MASK;
                }
            }
            for (int j = 0; j < vec_size; j++) {
                MSSshare_share_from(0, party_id, *netio, &x_share[j], test_value[j]);
            }

            // compute
            auto intermediate_vec = make_ptr_vec(intermediate);
            auto x_share_vec = make_ptr_vec(x_share);
            auto b_share_vec = make_ptr_vec(b_share);
            PI_sign_vec(party_id, PRGs, *netio, intermediate_vec, x_share_vec, b_share_vec);

            // reveal
            for (int j = 0; j < vec_size; j++) {
                ShareValue rec = MSSshare_p_recon(party_id, *netio, &b_share[j]);
                ShareValue expected = test_value[j] & (ShareValue(1) << (ell_arr[j] - 1)) ? 1 : 0;
                if (rec != expected && party_id == 0) {
                    cout << "Test " << i << "." << j << " failed! Expected: " << expected
                         << ", got: " << rec << endl;
                    exit(1);
                }
            }
        }
        cout << "PI_sign_vec test passed!" << endl;
    }

    delete netio;
    return 0;
}
