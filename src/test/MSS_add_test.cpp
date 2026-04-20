#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/share_party_init.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;

const int len = 20;
// circuit: Sigma(coeff[i] * secret[i])^2
uint64_t secret[len] = {1111, 2222, 3333, 4444, 5555, 6666, 7777};
int coeff[len] = {1, -253, 5, -9, 197, -3, 0};

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

    for (int test_round = 0; test_round < 2; test_round++) {
        // mod 2^l
        {
            const int ell = 60;
            // shares allocation
            std::vector<MSSshare> s(len, MSSshare(ell));
            MSSshare s_add(ell);
            MSSshare_mul_res s_mul(ell);

            for (int i = 0; i < (int)s.size(); i++) {
                MSSshare_preprocess(i % 3, party_id, PRGs, *netio, &s[i]);
            }
            s_add = s[0] * coeff[0];
            for (int i = 1; i < 7; i++) {
                s_add = s_add + (s[i] * coeff[i]);
            }
            MSSshare_mul_res_preprocess(party_id, PRGs, *netio, &s_mul, &s_add, &s_add);

            // preprocess后需要调用这个
            if (party_id == 0) {
                netio->send_stored_data(2);
            }

            // share
            for (int holder = 0; holder < 3; holder++) {
                for (int i = 0; i < 7; i++) {
                    if (i % 3 == holder)
                        MSSshare_share_from(i % 3, party_id, *netio, &s[i], secret[i]);
                }
            }

            // calculate
            s_add = s[0] * coeff[0];
            for (int i = 1; i < 7; i++) {
                s_add = s_add + (s[i] * coeff[i]);
            }
            MSSshare_mul_res_calc_mul(party_id, *netio, &s_mul, &s_add, &s_add);

            // reconstruct
            ShareValue rec = MSSshare_recon(party_id, *netio, &s_mul);
            ShareValue res = 0;
            for (int i = 0; i < len; i++) {
                res += coeff[i] * secret[i];
                res = res & ((ShareValue(1) << ell) - 1);
            }
            res = (res * res) & ((ShareValue(1) << ell) - 1);
            cout << "Reconstructed result: " << rec << endl;
            cout << "Expected result: " << res << endl;
            if (rec == res) {
                cout << "MSS add test passed!" << endl;
            } else {
                cout << "MSS add test failed!" << endl;
                return 1;
            }
        }

        // mod p
        {
            const ShareValue p = 67676767;
            // shares allocation
            std::vector<MSSshare_p> s(len, p);
            MSSshare_p s_add(p);
            MSSshare_p_mul_res s_mul(p);

            for (int i = 0; i < (int)s.size(); i++) {
                MSSshare_p_preprocess(i % 3, party_id, PRGs, *netio, &s[i]);
            }
            s_add = s[0] * coeff[0];
            for (int i = 1; i < 7; i++) {
                s_add = s_add + (s[i] * coeff[i]);
            }
            MSSshare_p_mul_res_preprocess(party_id, PRGs, *netio, &s_mul, &s_add, &s_add);

            // preprocess后需要调用这个
            if (party_id == 0) {
                netio->send_stored_data(2);
            }

            // share
            for (int holder = 0; holder < 3; holder++) {
                for (int i = 0; i < 7; i++) {
                    if (i % 3 == holder)
                        MSSshare_p_share_from(i % 3, party_id, *netio, &s[i], secret[i]);
                }
            }

            // calculate
            s_add = s[0] * coeff[0];
            for (int i = 1; i < 7; i++) {
                s_add = s_add + (s[i] * coeff[i]);
            }
            MSSshare_p_mul_res_calc_mul(party_id, *netio, &s_mul, &s_add, &s_add);

            // reconstruct
            ShareValue rec = MSSshare_p_recon(party_id, *netio, &s_mul);
            long long res = 0;
            for (int i = 0; i < len; i++) {
                // int coeff_mod;
                // if (coeff_vec[i] >= 0) {
                //     coeff_mod = coeff_vec[i] % p;
                // } else {
                //     int tmp = (-coeff_vec[i]) % p;
                //     coeff_mod = (p - tmp) % p;
                // }

                res += coeff[i] * (long long)secret[i];
                res = (res % (long long)p + (long long)p) % (long long)p;
            }
            res = (res * res) % (long long)p;
            cout << "Reconstructed result: " << rec << endl;
            cout << "Expected result: " << res << endl;
            if (rec == res) {
                cout << "MSS_p add test passed!" << endl;
            } else {
                cout << "MSS_p add test failed!" << endl;
                return 1;
            }
        }
    }
    delete netio;
    return 0;
}
