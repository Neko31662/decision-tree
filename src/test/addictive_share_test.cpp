#include "protocol/addictive_share.h"
#include "protocol/addictive_share_p.h"
#include "protocol/share_party_init.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int ell = 31;

const int len = 10;
ShareValue secret[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

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

    // 非向量部分，mod 2^ell
    {
        vector<ADDshare<>> s(len, ADDshare<>(ell));
        for (int i = 0; i < len; i++) {
            ADDshare_share_from(i % 3, party_id, PRGs, *netio, &s[i], secret[i]);
        }
        vector<ADDshare_mul_res<>> s_mul(len, ADDshare_mul_res(ell));
        for (int i = 0; i < len; i++) {
            ADDshare_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        for (int i = 0; i < len - 1; i++) {
            ADDshare_mul_res_cal_mult(party_id, *netio, &s_mul[i], &s[i], &s[i + 1]);
        }
        if (party_id != 0) {
            for (int i = 0; i < len - 1; i++) {
                ShareValue res = ADDshare_recon(party_id, *netio, &s_mul[i]);
                ShareValue expect = (secret[i] * secret[i + 1]) & s[0].MASK;
                if (res != expect) {
                    cout << "Error at index " << i << ": got " << (uint64_t)res << ", expected "
                         << (uint64_t)expect << endl;
                    return 1;
                }
            }
        }
        cout << "ADDshare test passed!" << endl;
    }

    // 向量部分，mod 2^ell
    {
        ADDshare_v<> s1_vec(len - 1, ell);
        ADDshare_v<> s2_vec(len - 1, ell);
        ADDshare_mul_res_v<> res_vec(len - 1, ell);
        for (int i = 0; i < len - 1; i++) {
            ADDshare_mul_res_preprocess(party_id, PRGs, *netio, &res_vec[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(2);
        }
        for (int holder = 0; holder < 3; holder++) {
            for (int i = 0; i < len - 1; i++) {
                if (i % 3 == holder) {
                    ADDshare_share_from(i % 3, party_id, PRGs, *netio, &s1_vec[i], secret[i]);
                }
            }
            for (int i = 0; i < len - 1; i++) {
                if ((i + 1) % 3 == holder) {
                    ADDshare_share_from((i + 1) % 3, party_id, PRGs, *netio, &s2_vec[i],
                                        secret[i + 1]);
                }
            }
        }

        ADDshare_mul_res_cal_mult_vec(party_id, *netio, res_vec, s1_vec, s2_vec);
        if (party_id != 0) {
            for (int i = 0; i < len - 1; i++) {
                ShareValue res = ADDshare_recon(party_id, *netio, &res_vec[i]);
                ShareValue expect = (secret[i] * secret[i + 1]) & s1_vec.MASK;
                if (res != expect) {
                    cout << "Error at index " << i << ": got " << (uint64_t)res << ", expected "
                         << (uint64_t)expect << endl;
                    return 1;
                }
            }
        }
        cout << "ADDshare_vec test passed!" << endl;
    }

    // 非向量部分，mod p
    {
        const ShareValue p = 12289;
        vector<ADDshare_p> s(len, p);
        for (int i = 0; i < len; i++) {
            ADDshare_p_share_from(i % 3, party_id, PRGs, *netio, &s[i], secret[i]);
        }
        vector<ADDshare_p_mul_res> s_mul(len, p);
        for (int i = 0; i < len; i++) {
            ADDshare_p_mul_res_preprocess(party_id, PRGs, *netio, &s_mul[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(2);
        }

        for (int i = 0; i < len - 1; i++) {
            ADDshare_p_mul_res_cal_mult(party_id, *netio, &s_mul[i], &s[i], &s[i + 1]);
        }
        if (party_id != 0) {
            for (int i = 0; i < len - 1; i++) {
                ShareValue res = ADDshare_p_recon(party_id, *netio, &s_mul[i]);
                ShareValue expect = (secret[i] * secret[i + 1]) % p;
                if (res != expect) {
                    cout << "Error at index " << i << ": got " << (uint64_t)res << ", expected "
                         << (uint64_t)expect << endl;
                    return 1;
                }
            }
        }
        cout << "ADDshare_p test passed!" << endl;
    }

    // 向量部分，mod 2^ell
    {
        const ShareValue p = 12289;
        ADDshare_p_v s1_vec(len - 1, p);
        ADDshare_p_v s2_vec(len - 1, p);
        ADDshare_p_mul_res_v res_vec(len - 1, p);
        for (int i = 0; i < len - 1; i++) {
            ADDshare_p_mul_res_preprocess(party_id, PRGs, *netio, &res_vec[i]);
        }
        if (party_id == 0) {
            netio->send_stored_data(2);
        }
        for (int holder = 0; holder < 3; holder++) {
            for (int i = 0; i < len - 1; i++) {
                if (i % 3 == holder) {
                    ADDshare_p_share_from(i % 3, party_id, PRGs, *netio, &s1_vec[i], secret[i]);
                }
            }
            for (int i = 0; i < len - 1; i++) {
                if ((i + 1) % 3 == holder) {
                    ADDshare_p_share_from((i + 1) % 3, party_id, PRGs, *netio, &s2_vec[i],
                                          secret[i + 1]);
                }
            }
        }

        ADDshare_p_mul_res_cal_mult_vec(party_id, *netio, res_vec, s1_vec, s2_vec);
        if (party_id != 0) {
            for (int i = 0; i < len - 1; i++) {
                ShareValue res = ADDshare_p_recon(party_id, *netio, &res_vec[i]);
                ShareValue expect = (secret[i] * secret[i + 1]) % p;
                if (res != expect) {
                    cout << "Error at index " << i << ": got " << (uint64_t)res << ", expected "
                         << (uint64_t)expect << endl;
                    return 1;
                }
            }
        }
        cout << "ADDshare_p_vec test passed!" << endl;
    }

    cout << "ADDshare test passed!" << endl;
    delete netio;
    return 0;
}
