#include "protocol/ove.h"
#include "protocol/share_party_init.h"
#include <iostream>
#include <vector>

const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;
const int test_rounds = 120;

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

    const int ell = 40;
    vector<int> lens = {1, 2, 7, 15};

    for (int n : lens) {
        for (int round = 0; round < test_rounds; round++) {
            int s_id = round % 3;

            MSSshare_v input_x(n, ell);
            MSSshare_v output_x(1, ell); // 协议内会重置长度
            vector<ShareValue> plain_x(n, 0);

            vector<int> a(n, 1);
            if (party_id == s_id) {
                for (int i = 0; i < n; i++) {
                    ShareValue rnd = 0;
                    private_PRG.gen_random_data(&rnd, sizeof(ShareValue));
                    a[i] = 1 + (int)(rnd % 10); // 1~10
                }
            }
            // 仅用于party0做期望值校验：当S不等于0时，把a发送给party0
            if (s_id != 0) {
                if (party_id == s_id) {
                    netio->send_data(0, a.data(), (int)sizeof(int) * n);
                } else if (party_id == 0) {
                    netio->recv_data(s_id, a.data(), (int)sizeof(int) * n);
                }
            }

            for (int i = 0; i < n; i++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &input_x[i]);
            }

            for (int i = 0; i < n; i++) {
                if (party_id == 0) {
                    private_PRG.gen_random_data(&plain_x[i], sizeof(ShareValue));
                    plain_x[i] &= input_x[i].MASK;
                }
                MSSshare_share_from(0, party_id, *netio, &input_x[i], plain_x[i]);
            }

            const vector<int> *a_ptr = (party_id == s_id) ? &a : nullptr;
            PI_OVE(party_id, s_id, PRGs, *netio, input_x, output_x, a_ptr);

            vector<ShareValue> rec = MSSshare_recon_vec(party_id, *netio, output_x);

            if (party_id == 0) {
                vector<ShareValue> expect;
                expect.reserve(output_x.size());
                for (int i = 0; i < n; i++) {
                    for (int t = 0; t < a[i]; t++) {
                        expect.push_back(plain_x[i]);
                    }
                }
                if (rec != expect) {
                    cout << "PI_OVE failed at n=" << n << ", round=" << round << ", s_id=" << s_id
                         << endl;
                    return 1;
                }
            }
        }
    }

    // 万级压力测试
    {
        const int n = 10000;
        const int s_id = 2;
        MSSshare_v input_x(n, ell);
        MSSshare_v output_x(1, ell);
        vector<ShareValue> plain_x(n, 0);
        vector<int> a(n, 1);
        if (party_id == s_id) {
            for (int i = 0; i < n; i++) {
                ShareValue rnd = 0;
                private_PRG.gen_random_data(&rnd, sizeof(ShareValue));
                a[i] = 1 + (int)(rnd % 10); // 1~10
            }
        }
        if (s_id != 0) {
            if (party_id == s_id) {
                netio->send_data(0, a.data(), (int)sizeof(int) * n);
            } else if (party_id == 0) {
                netio->recv_data(s_id, a.data(), (int)sizeof(int) * n);
            }
        }

        for (int i = 0; i < n; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &input_x[i]);
            if (party_id == 0) {
                private_PRG.gen_random_data(&plain_x[i], sizeof(ShareValue));
                plain_x[i] &= input_x[i].MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &input_x[i], plain_x[i]);
        }

        const vector<int> *a_ptr = (party_id == s_id) ? &a : nullptr;
        PI_OVE(party_id, s_id, PRGs, *netio, input_x, output_x, a_ptr);
        vector<ShareValue> rec = MSSshare_recon_vec(party_id, *netio, output_x);

        if (party_id == 0) {
            vector<ShareValue> expect;
            expect.reserve(output_x.size());
            for (int i = 0; i < n; i++) {
                for (int t = 0; t < a[i]; t++) {
                    expect.push_back(plain_x[i]);
                }
            }
            if (rec != expect) {
                cout << "PI_OVE large case failed" << endl;
                return 1;
            }
        }
    }

    cout << "PI_OVE test passed!" << endl;
    delete netio;
    return 0;
}
