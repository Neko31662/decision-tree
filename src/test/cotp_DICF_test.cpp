#include "protocol/cotp_DICF.h"
#include "protocol/share_party_init.h"

#include <iostream>

using namespace std;

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int TEST_NUMS = 160;
const int ELL = 31;

inline long long to_signed_ell(ShareValue x, int ell) {
    ShareValue mask = (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
    x &= mask;
    ShareValue sign_bit = ShareValue(1) << (ell - 1);
    if ((x & sign_bit) == 0) {
        return static_cast<long long>(x);
    }
    LongShareValue mod = (LongShareValue(1) << ell);
    return -static_cast<long long>(mod - LongShareValue(x));
}

inline ShareValue encode_signed_ell(long long x, int ell) {
    LongShareValue mod = (LongShareValue(1) << ell);
    LongShareValue v = (x >= 0) ? LongShareValue(x) : (mod + LongShareValue(x));
    return static_cast<ShareValue>(v % mod);
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
    } else {
        netio->recv_data(0, &seed1, sizeof(block));
        netio->recv_data(1, &seed2, sizeof(block));
    }
    vector<PRGSync> PRGs = {PRGSync(&seed1), PRGSync(&seed2)};

    auto private_seed = gen_seed();
    auto private_PRG = PRGSync(&private_seed);
    ShareValue mask = (ELL == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ELL) - 1);

    for (int t = 0; t < TEST_NUMS; t++) {
        MSSshare x_share(ELL), y_share(ELL), L_share(ELL), R_share(ELL);
        PI_cotp_DICF_intermediate inter(ELL);

        MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &L_share);
        MSSshare_preprocess(0, party_id, PRGs, *netio, &R_share);

        PI_cotp_DICF_preprocess(party_id, PRGs, *netio, inter);
        netio->send_stored_data_all();

        ShareValue x_plain = 0, y_plain = 0, L_plain = 0, R_plain = 0;
        if (t % 4 == 0) {
            x_plain = (ShareValue)(150 + t) & mask;
            y_plain = (ShareValue)(80 + t / 2) & mask;
        } else if (t % 4 == 1) {
            x_plain = ((ShareValue(1) << (ELL - 1)) + 7 * t) & mask;
            y_plain = ((ShareValue(1) << (ELL - 1)) + 3 * t) & mask;
        } else {
            long long bound = (1LL << (ELL - 3));
            ShareValue rx = 0, ry = 0;
            private_PRG.gen_random_data(&rx, sizeof(ShareValue));
            private_PRG.gen_random_data(&ry, sizeof(ShareValue));
            long long sx = (long long)(rx % (2 * (ShareValue)bound)) - bound;
            long long sy = (long long)(ry % (2 * (ShareValue)bound)) - bound;
            x_plain = encode_signed_ell(sx, ELL);
            y_plain = encode_signed_ell(sy, ELL);
        }
        private_PRG.gen_random_data(&L_plain, sizeof(ShareValue));
        private_PRG.gen_random_data(&R_plain, sizeof(ShareValue));
        L_plain &= mask;
        R_plain &= mask;

        MSSshare_share_from(0, party_id, *netio, &x_share, x_plain);
        MSSshare_share_from(0, party_id, *netio, &y_share, y_plain);
        MSSshare_share_from(0, party_id, *netio, &L_share, L_plain);
        MSSshare_share_from(0, party_id, *netio, &R_share, R_plain);

        ShareValue got =
            PI_cotp_DICF(party_id, PRGs, *netio, inter, &x_share, &y_share, &L_share, &R_share);

        if (party_id == 0) {
            ShareValue expect = (to_signed_ell(x_plain, ELL) >= to_signed_ell(y_plain, ELL)) ? L_plain
                                                                                               : R_plain;
            if (got != expect) {
                cout << "cotp_DICF mismatch at round=" << t << ", expect=" << expect
                     << ", got=" << got << ", x=" << to_signed_ell(x_plain, ELL)
                     << ", y=" << to_signed_ell(y_plain, ELL) << endl;
                return 1;
            }
        }
    }

    // 向量化版本：同一个比较条件下批量选择 (L_i, R_i)
    {
        vector<int> vec_sizes = {1, 2, 16};
        for (int vec_size : vec_sizes) {
            for (int t = 0; t < 80; t++) {
                MSSshare x_share(ELL), y_share(ELL);
                vector<MSSshare> L_share(vec_size, MSSshare(ELL));
                vector<MSSshare> R_share(vec_size, MSSshare(ELL));
                PI_cotp_DICF_intermediate inter(ELL);

                MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
                MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
                for (int i = 0; i < vec_size; i++) {
                    MSSshare_preprocess(0, party_id, PRGs, *netio, &L_share[i]);
                    MSSshare_preprocess(0, party_id, PRGs, *netio, &R_share[i]);
                }

                PI_cotp_DICF_preprocess(party_id, PRGs, *netio, inter);
                netio->send_stored_data_all();

                ShareValue x_plain = 0, y_plain = 0;
                if (t % 2 == 0) {
                    x_plain = (ShareValue)(200 + t) & mask;
                    y_plain = (ShareValue)(100 + t / 2) & mask;
                } else {
                    long long bound = (1LL << (ELL - 3));
                    ShareValue rx = 0, ry = 0;
                    private_PRG.gen_random_data(&rx, sizeof(ShareValue));
                    private_PRG.gen_random_data(&ry, sizeof(ShareValue));
                    long long sx = (long long)(rx % (2 * (ShareValue)bound)) - bound;
                    long long sy = (long long)(ry % (2 * (ShareValue)bound)) - bound;
                    x_plain = encode_signed_ell(sx, ELL);
                    y_plain = encode_signed_ell(sy, ELL);
                }
                MSSshare_share_from(0, party_id, *netio, &x_share, x_plain);
                MSSshare_share_from(0, party_id, *netio, &y_share, y_plain);

                vector<ShareValue> L_plain(vec_size, 0), R_plain(vec_size, 0);
                for (int i = 0; i < vec_size; i++) {
                    private_PRG.gen_random_data(&L_plain[i], sizeof(ShareValue));
                    private_PRG.gen_random_data(&R_plain[i], sizeof(ShareValue));
                    L_plain[i] &= mask;
                    R_plain[i] &= mask;
                    MSSshare_share_from(0, party_id, *netio, &L_share[i], L_plain[i]);
                    MSSshare_share_from(0, party_id, *netio, &R_share[i], R_plain[i]);
                }

                auto L_ptrs = make_ptr_vec(L_share);
                auto R_ptrs = make_ptr_vec(R_share);
                vector<ShareValue> got =
                    PI_cotp_DICF_vec(party_id, PRGs, *netio, inter, &x_share, &y_share, L_ptrs, R_ptrs);

                if (party_id == 0) {
                    bool cond = to_signed_ell(x_plain, ELL) >= to_signed_ell(y_plain, ELL);
                    for (int i = 0; i < vec_size; i++) {
                        ShareValue expect = cond ? L_plain[i] : R_plain[i];
                        if (got[i] != expect) {
                            cout << "cotp_DICF_vec mismatch at vec_size=" << vec_size
                                 << ", round=" << t << ", idx=" << i << ", expect=" << expect
                                 << ", got=" << got[i] << endl;
                            return 1;
                        }
                    }
                }
            }
        }
    }

    cout << "PI_cotp_DICF (non-vector + vector) test passed!" << endl;
    delete netio;
    return 0;
}
