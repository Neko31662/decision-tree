#include "protocol/cotp.h"
#include "protocol/share_party_init.h"
#include <iostream>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 180;
const int ell = 31;

using namespace std;

inline long long to_signed_ell(ShareValue x, int ell) {
    ShareValue mask = (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);
    x &= mask;
    ShareValue sign_bit = ShareValue(1) << (ell - 1);
    if ((x & sign_bit) == 0) {
        return (long long)x;
    }
    LongShareValue mod = (LongShareValue(1) << ell);
    return -(long long)(mod - LongShareValue(x));
}

inline ShareValue encode_signed_ell(long long x, int ell) {
    LongShareValue mod = (LongShareValue(1) << ell);
    LongShareValue v = (x >= 0) ? LongShareValue(x) : (mod + LongShareValue(x));
    return (ShareValue)(v % mod);
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

    auto private_seed = gen_seed();
    auto private_PRG = PRGSync(&private_seed);

    ShareValue mask = (ell == ShareValue_BitLength) ? ~ShareValue(0) : ((ShareValue(1) << ell) - 1);

    vector<int> vec_sizes = {1, 2, 16};
    for (int vec_size : vec_sizes) {
        for (int t = 0; t < test_nums; t++) {
            MSSshare x_share(ell), y_share(ell);
            vector<MSSshare> L_share(vec_size, MSSshare(ell));
            vector<MSSshare> R_share(vec_size, MSSshare(ell));
            PI_cotp_intermediate inter(ell, vec_size);

            // preprocess inputs
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share);
            for (int i = 0; i < vec_size; i++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &L_share[i]);
                MSSshare_preprocess(0, party_id, PRGs, *netio, &R_share[i]);
            }

            // preprocess protocol
            PI_cotp_preprocess(party_id, PRGs, *netio, inter);
            netio->send_stored_data_all();

            // generate inputs
            ShareValue x_plain = 0, y_plain = 0;
            if (t % 3 == 0) {
                x_plain = (ShareValue)(50 + t) & mask;
                y_plain = (ShareValue)(10 + (t % 7)) & mask;
            } else if (t % 3 == 1) {
                x_plain = ((ShareValue(1) << (ell - 1)) + (t * 3)) & mask;
                y_plain = ((ShareValue(1) << (ell - 1)) + (t * 5)) & mask;
            } else {
                ShareValue rx = 0, ry = 0;
                private_PRG.gen_random_data(&rx, sizeof(ShareValue));
                private_PRG.gen_random_data(&ry, sizeof(ShareValue));
                // Keep inputs in a safe signed range to avoid subtraction overflow.
                long long bound = (1LL << (ell - 3));
                long long sx = (long long)(rx % (2 * (ShareValue)bound)) - bound;
                long long sy = (long long)(ry % (2 * (ShareValue)bound)) - bound;
                x_plain = encode_signed_ell(sx, ell);
                y_plain = encode_signed_ell(sy, ell);
            }

            vector<ShareValue> L_plain(vec_size), R_plain(vec_size);
            for (int i = 0; i < vec_size; i++) {
                if (t % 2 == 0) {
                    L_plain[i] = (ShareValue)(t * 17 + i * 11 + 5) & mask;
                    R_plain[i] = (ShareValue)(t * 13 + i * 7 + 9) & mask;
                } else {
                    private_PRG.gen_random_data(&L_plain[i], sizeof(ShareValue));
                    private_PRG.gen_random_data(&R_plain[i], sizeof(ShareValue));
                    L_plain[i] &= mask;
                    R_plain[i] &= mask;
                }
            }

            // share inputs
            MSSshare_share_from(0, party_id, *netio, &x_share, x_plain);
            MSSshare_share_from(0, party_id, *netio, &y_share, y_plain);
            for (int i = 0; i < vec_size; i++) {
                MSSshare_share_from(0, party_id, *netio, &L_share[i], L_plain[i]);
                MSSshare_share_from(0, party_id, *netio, &R_share[i], R_plain[i]);
            }

            auto L_ptrs = make_ptr_vec(L_share);
            auto R_ptrs = make_ptr_vec(R_share);
            vector<ShareValue> out_plain =
                PI_cotp(party_id, PRGs, *netio, inter, &x_share, &y_share, L_ptrs, R_ptrs);

            if (party_id == 0) {
                bool cond = to_signed_ell(x_plain, ell) >= to_signed_ell(y_plain, ell);
                for (int i = 0; i < vec_size; i++) {
                    ShareValue expected = cond ? L_plain[i] : R_plain[i];
                    if (out_plain[i] != expected) {
                        cout << "COTP mismatch at vec_size=" << vec_size << ", round=" << t
                             << ", idx=" << i << ", cond=" << cond << ", expected=" << expected
                             << ", got=" << out_plain[i] << endl;
                        return 1;
                    }
                }
            }
        }
    }

    // 批量preprocess测试：多个cotp实例共享一次预处理通信
    {
        const int batch_size = 6;
        const int vec_size = 2;
        std::vector<MSSshare> x_share, y_share;
        std::vector<std::vector<MSSshare>> L_share, R_share;
        std::vector<PI_cotp_intermediate> inters;
        x_share.reserve(batch_size);
        y_share.reserve(batch_size);
        L_share.reserve(batch_size);
        R_share.reserve(batch_size);
        inters.reserve(batch_size);

        for (int b = 0; b < batch_size; b++) {
            x_share.emplace_back(ell);
            y_share.emplace_back(ell);
            L_share.emplace_back(vec_size, MSSshare(ell));
            R_share.emplace_back(vec_size, MSSshare(ell));
            inters.emplace_back(ell, vec_size);

            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share[b]);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share[b]);
            for (int i = 0; i < vec_size; i++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &L_share[b][i]);
                MSSshare_preprocess(0, party_id, PRGs, *netio, &R_share[b][i]);
            }
        }

        std::vector<PI_cotp_intermediate *> inter_ptrs;
        inter_ptrs.reserve(batch_size);
        for (int b = 0; b < batch_size; b++) {
            inter_ptrs.push_back(&inters[b]);
        }
        PI_cotp_preprocess_vec(party_id, PRGs, *netio, inter_ptrs);
        netio->send_stored_data_all();

        for (int b = 0; b < batch_size; b++) {
            ShareValue x_plain = (ShareValue)(100 + 13 * b) & mask;
            ShareValue y_plain = (ShareValue)(90 + 17 * b) & mask;
            if (b % 2 == 1) {
                x_plain = ((ShareValue(1) << (ell - 1)) + 5 * b) & mask;
                y_plain = ((ShareValue(1) << (ell - 1)) + 9 * b) & mask;
            }
            MSSshare_share_from(0, party_id, *netio, &x_share[b], x_plain);
            MSSshare_share_from(0, party_id, *netio, &y_share[b], y_plain);

            std::vector<ShareValue> L_plain(vec_size), R_plain(vec_size);
            for (int i = 0; i < vec_size; i++) {
                L_plain[i] = (ShareValue)(b * 41 + i * 11 + 3) & mask;
                R_plain[i] = (ShareValue)(b * 37 + i * 7 + 5) & mask;
                MSSshare_share_from(0, party_id, *netio, &L_share[b][i], L_plain[i]);
                MSSshare_share_from(0, party_id, *netio, &R_share[b][i], R_plain[i]);
            }

            auto L_ptrs = make_ptr_vec(L_share[b]);
            auto R_ptrs = make_ptr_vec(R_share[b]);
            std::vector<ShareValue> out_plain =
                PI_cotp(party_id, PRGs, *netio, inters[b], &x_share[b], &y_share[b], L_ptrs, R_ptrs);

            if (party_id == 0) {
                bool cond = to_signed_ell(x_plain, ell) >= to_signed_ell(y_plain, ell);
                for (int i = 0; i < vec_size; i++) {
                    ShareValue expected = cond ? L_plain[i] : R_plain[i];
                    if (out_plain[i] != expected) {
                        cout << "COTP batch preprocess mismatch at b=" << b << ", idx=" << i
                             << ", cond=" << cond << ", expected=" << expected
                             << ", got=" << out_plain[i] << endl;
                        return 1;
                    }
                }
            }
        }
    }

    cout << "PI_cotp test passed!" << endl;
    delete netio;
    return 0;
}
