#include "protocol/sign_mult.h"
#include "protocol/share_party_init.h"
#include <iostream>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_nums = 240;
const int ell = 31;

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

    auto private_seed = gen_seed();
    auto private_PRG = PRGSync(&private_seed);

    std::vector<int> y_size_cases = {1, 2, 16};
    for (int y_size : y_size_cases) {
        for (int i = 0; i < test_nums; i++) {
            PI_sign_mult_intermediate intermediate(ell, y_size);
            MSSshare x_share(ell);
            vector<MSSshare> y_share(y_size, MSSshare(ell));
            MSSshare z_share(ell);
            vector<MSSshare> w_share(y_size, MSSshare(ell));

            // preprocess
            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share);
            for (int j = 0; j < y_size; j++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share[j]);
            }
            auto y_share_vec = make_ptr_vec(y_share);
            auto w_share_vec = make_ptr_vec(w_share);
            PI_sign_mult_preprocess(party_id, PRGs, *netio, intermediate, &x_share, y_share_vec,
                                    &z_share, w_share_vec);
            netio->send_stored_data_all();

            // share x
            ShareValue x_plain = i;
            if (i % 4 == 1) {
                x_plain = ((ShareValue(1) << ell) - i - 1) & x_share.MASK;
            } else if (i % 4 == 2) {
                private_PRG.gen_random_data(&x_plain, sizeof(ShareValue));
                x_plain &= x_share.MASK;
            } else if (i % 4 == 3) {
                x_plain = (ShareValue(1) << (ell - 1)) | (i & ((ShareValue(1) << (ell - 1)) - 1));
                x_plain &= x_share.MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &x_share, x_plain);

            // share y vector
            vector<ShareValue> y_plain(y_size, 0);
            for (int j = 0; j < y_size; j++) {
                if (i % 3 == 2) {
                    private_PRG.gen_random_data(&y_plain[j], sizeof(ShareValue));
                    y_plain[j] &= x_share.MASK;
                } else {
                    y_plain[j] = (ShareValue(i + 1) * (j + 3) + 17 * j + 11) & x_share.MASK;
                }
                MSSshare_share_from(0, party_id, *netio, &y_share[j], y_plain[j]);
            }

            // compute
            PI_sign_mult(party_id, PRGs, *netio, intermediate, &x_share, y_share_vec, &z_share,
                         w_share_vec);

            // reveal and check
            ShareValue z_rec = MSSshare_recon(party_id, *netio, &z_share);
            vector<ShareValue> w_rec(y_size, 0);
            for (int j = 0; j < y_size; j++) {
                w_rec[j] = MSSshare_recon(party_id, *netio, &w_share[j]);
            }
            ShareValue expected_z = (x_plain >> (ell - 1)) & 1;

            if (party_id == 0) {
                if (z_rec != expected_z) {
                    cout << "z mismatch at y_size=" << y_size << ", round " << i << ": expected "
                         << expected_z << ", got " << z_rec << endl;
                    return 1;
                }
                for (int j = 0; j < y_size; j++) {
                    ShareValue expected_w = expected_z ? y_plain[j] : 0;
                    if (w_rec[j] != expected_w) {
                        cout << "w mismatch at y_size=" << y_size << ", round " << i
                             << ", index " << j << ": expected " << expected_w << ", got "
                             << w_rec[j] << endl;
                        return 1;
                    }
                }
            }
        }
    }

    // 批量preprocess测试：多个sign_mult实例共享一次Gamma预处理通信
    {
        const int batch_size = 7;
        const int y_size = 2;
        std::vector<PI_sign_mult_intermediate> inters;
        std::vector<MSSshare> x_share;
        std::vector<std::vector<MSSshare>> y_share;
        std::vector<MSSshare> z_share;
        std::vector<std::vector<MSSshare>> w_share;
        inters.reserve(batch_size);
        x_share.reserve(batch_size);
        y_share.reserve(batch_size);
        z_share.reserve(batch_size);
        w_share.reserve(batch_size);

        for (int b = 0; b < batch_size; b++) {
            inters.emplace_back(ell, y_size);
            x_share.emplace_back(ell);
            y_share.emplace_back(y_size, MSSshare(ell));
            z_share.emplace_back(ell);
            w_share.emplace_back(y_size, MSSshare(ell));

            MSSshare_preprocess(0, party_id, PRGs, *netio, &x_share[b]);
            for (int j = 0; j < y_size; j++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &y_share[b][j]);
            }
        }

        std::vector<PI_sign_mult_intermediate *> inter_ptrs;
        std::vector<MSSshare *> x_ptrs;
        std::vector<std::vector<MSSshare *>> y_ptrs_vec;
        std::vector<MSSshare *> z_ptrs;
        std::vector<std::vector<MSSshare *>> w_ptrs_vec;
        inter_ptrs.reserve(batch_size);
        x_ptrs.reserve(batch_size);
        y_ptrs_vec.reserve(batch_size);
        z_ptrs.reserve(batch_size);
        w_ptrs_vec.reserve(batch_size);

        for (int b = 0; b < batch_size; b++) {
            inter_ptrs.push_back(&inters[b]);
            x_ptrs.push_back(&x_share[b]);
            y_ptrs_vec.push_back(make_ptr_vec(y_share[b]));
            z_ptrs.push_back(&z_share[b]);
            w_ptrs_vec.push_back(make_ptr_vec(w_share[b]));
        }

        PI_sign_mult_preprocess_vec(party_id, PRGs, *netio, inter_ptrs, x_ptrs, y_ptrs_vec,
                                    z_ptrs, w_ptrs_vec);
        netio->send_stored_data_all();

        for (int b = 0; b < batch_size; b++) {
            ShareValue x_plain = 0;
            if (b % 3 == 0) {
                x_plain = (ShareValue)(100 + b) & x_share[b].MASK;
            } else if (b % 3 == 1) {
                x_plain = ((ShareValue(1) << (ell - 1)) + 19 * b) & x_share[b].MASK;
            } else {
                private_PRG.gen_random_data(&x_plain, sizeof(ShareValue));
                x_plain &= x_share[b].MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &x_share[b], x_plain);

            std::vector<ShareValue> y_plain(y_size, 0);
            for (int j = 0; j < y_size; j++) {
                y_plain[j] = (ShareValue)(b * 31 + j * 7 + 5) & x_share[b].MASK;
                MSSshare_share_from(0, party_id, *netio, &y_share[b][j], y_plain[j]);
            }

            auto y_ptrs = make_ptr_vec(y_share[b]);
            auto w_ptrs = make_ptr_vec(w_share[b]);
            PI_sign_mult(party_id, PRGs, *netio, inters[b], &x_share[b], y_ptrs, &z_share[b], w_ptrs);

            ShareValue z_rec = MSSshare_recon(party_id, *netio, &z_share[b]);
            ShareValue expected_z = (x_plain >> (ell - 1)) & 1;
            if (party_id == 0 && z_rec != expected_z) {
                cout << "batch preprocess z mismatch at b=" << b << ", expected=" << expected_z
                     << ", got=" << z_rec << endl;
                return 1;
            }
            for (int j = 0; j < y_size; j++) {
                ShareValue w_rec = MSSshare_recon(party_id, *netio, &w_share[b][j]);
                ShareValue expected_w = expected_z ? y_plain[j] : 0;
                if (party_id == 0 && w_rec != expected_w) {
                    cout << "batch preprocess w mismatch at b=" << b << ", j=" << j
                         << ", expected=" << expected_w << ", got=" << w_rec << endl;
                    return 1;
                }
            }
        }
    }

    cout << "PI_sign_mult test passed!" << endl;
    delete netio;
    return 0;
}
