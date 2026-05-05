#include "protocol/share_party_init.h"
#include "protocol/shuffle.h"
#include <algorithm>
#include <iostream>
#include <unordered_map>
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

    vector<int> lens = {1, 2, 9, 25};
    const int ell = 31;

    for (int len : lens) {
        bool seen_non_identity = false;
        for (int round = 0; round < test_rounds; round++) {
            MSSshare_v input(len, ell);
            MSSshare_v output(len, ell);
            vector<ShareValue> plain(len, 0);

            for (int i = 0; i < len; i++) {
                MSSshare_preprocess(0, party_id, PRGs, *netio, &input[i]);
            }

            for (int i = 0; i < len; i++) {
                if (party_id == 0) {
                    private_PRG.gen_random_data(&plain[i], sizeof(ShareValue));
                    plain[i] &= input[i].MASK;
                }
                MSSshare_share_from(0, party_id, *netio, &input[i], plain[i]);
            }

            PI_shuffle(party_id, PRGs, *netio, input, output);

            vector<ShareValue> rec_in(len, 0), rec_out(len, 0);
            for (int i = 0; i < len; i++) {
                rec_in[i] = MSSshare_recon(party_id, *netio, &input[i]);
            }
            for (int i = 0; i < len; i++) {
                rec_out[i] = MSSshare_recon(party_id, *netio, &output[i]);
            }

            if (party_id == 0) {
                auto sorted_in = rec_in;
                auto sorted_out = rec_out;
                sort(sorted_in.begin(), sorted_in.end());
                sort(sorted_out.begin(), sorted_out.end());
                if (sorted_in != sorted_out) {
                    cout << "shuffle mismatch at len=" << len << ", round=" << round << endl;
                    return 1;
                }
                if (len > 1 && rec_in != rec_out) {
                    seen_non_identity = true;
                }
            }
        }

        if (party_id == 0 && len > 1 && !seen_non_identity) {
            cout << "shuffle seems always identity at len=" << len << endl;
            return 1;
        }
    }

    // 原地混洗接口测试
    {
        const int len = 13;
        MSSshare_v data(len, ell);
        vector<ShareValue> plain(len, 0);

        for (int i = 0; i < len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &data[i]);
        }
        for (int i = 0; i < len; i++) {
            if (party_id == 0) {
                private_PRG.gen_random_data(&plain[i], sizeof(ShareValue));
                plain[i] &= data[i].MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &data[i], plain[i]);
        }

        PI_shuffle(party_id, PRGs, *netio, data);

        vector<ShareValue> rec_out(len, 0);
        for (int i = 0; i < len; i++) {
            rec_out[i] = MSSshare_recon(party_id, *netio, &data[i]);
        }

        if (party_id == 0) {
            auto sorted_plain = plain;
            auto sorted_out = rec_out;
            sort(sorted_plain.begin(), sorted_plain.end());
            sort(sorted_out.begin(), sorted_out.end());
            if (sorted_plain != sorted_out) {
                cout << "原地混洗结果错误" << endl;
                return 1;
            }
        }
    }

    // 同步混洗接口测试：多个对象使用同一置换
    {
        const int len = 21;
        MSSshare_v obj1(len, ell);
        MSSshare_v obj2(len, ell);
        MSSshare_v obj3(len, ell);
        MSSshare_v out1(len, ell);
        MSSshare_v out2(len, ell);
        MSSshare_v out3(len, ell);

        vector<ShareValue> plain1(len, 0), plain2(len, 0), plain3(len, 0);
        for (int i = 0; i < len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &obj1[i]);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &obj2[i]);
            MSSshare_preprocess(0, party_id, PRGs, *netio, &obj3[i]);

            // 构造互不重复的数据，便于校验“同一排列”
            plain1[i] = (ShareValue(100000) + i) & obj1[i].MASK;
            plain2[i] = (ShareValue(200000) + i * 3) & obj2[i].MASK;
            plain3[i] = (ShareValue(300000) + i * 7) & obj3[i].MASK;

            MSSshare_share_from(0, party_id, *netio, &obj1[i], plain1[i]);
            MSSshare_share_from(0, party_id, *netio, &obj2[i], plain2[i]);
            MSSshare_share_from(0, party_id, *netio, &obj3[i], plain3[i]);
        }

        auto in_list = make_ptr_vec(obj1, obj2, obj3);
        auto out_list = make_ptr_vec(out1, out2, out3);
        PI_shuffle_sync(party_id, PRGs, *netio, in_list, out_list);

        vector<ShareValue> rec1(len), rec2(len), rec3(len);
        for (int i = 0; i < len; i++) {
            rec1[i] = MSSshare_recon(party_id, *netio, &out1[i]);
            rec2[i] = MSSshare_recon(party_id, *netio, &out2[i]);
            rec3[i] = MSSshare_recon(party_id, *netio, &out3[i]);
        }

        if (party_id == 0) {
            unordered_map<ShareValue, int> idx1;
            idx1.reserve(len * 2);
            for (int i = 0; i < len; i++) {
                idx1[plain1[i]] = i;
            }
            for (int i = 0; i < len; i++) {
                if (!idx1.count(rec1[i])) {
                    cout << "同步混洗结果在对象1中出现非法值" << endl;
                    return 1;
                }
                int mapped_idx = idx1[rec1[i]];
                if (rec2[i] != plain2[mapped_idx] || rec3[i] != plain3[mapped_idx]) {
                    cout << "同步混洗在不同对象间的置换不一致" << endl;
                    return 1;
                }
            }
        }

        // 原地同步混洗
        PI_shuffle_sync(party_id, PRGs, *netio, in_list);
        for (int i = 0; i < len; i++) {
            rec1[i] = MSSshare_recon(party_id, *netio, &obj1[i]);
            rec2[i] = MSSshare_recon(party_id, *netio, &obj2[i]);
            rec3[i] = MSSshare_recon(party_id, *netio, &obj3[i]);
        }
        if (party_id == 0) {
            auto s1 = rec1, s2 = rec2, s3 = rec3;
            sort(s1.begin(), s1.end());
            sort(s2.begin(), s2.end());
            sort(s3.begin(), s3.end());
            auto p1 = plain1, p2 = plain2, p3 = plain3;
            sort(p1.begin(), p1.end());
            sort(p2.begin(), p2.end());
            sort(p3.begin(), p3.end());
            if (s1 != p1 || s2 != p2 || s3 != p3) {
                cout << "原地同步混洗后多重集校验失败" << endl;
                return 1;
            }
        }
    }

    // 万级压力测试
    {
        const int len = 10000;
        MSSshare_v input(len, ell);
        MSSshare_v output(len, ell);
        vector<ShareValue> plain(len, 0);

        for (int i = 0; i < len; i++) {
            MSSshare_preprocess(0, party_id, PRGs, *netio, &input[i]);
            if (party_id == 0) {
                private_PRG.gen_random_data(&plain[i], sizeof(ShareValue));
                plain[i] &= input[i].MASK;
            }
            MSSshare_share_from(0, party_id, *netio, &input[i], plain[i]);
        }

        PI_shuffle(party_id, PRGs, *netio, input, output);
        vector<ShareValue> rec_out(len, 0);
        for (int i = 0; i < len; i++) {
            rec_out[i] = MSSshare_recon(party_id, *netio, &output[i]);
        }
        if (party_id == 0) {
            auto sorted_plain = plain;
            auto sorted_out = rec_out;
            sort(sorted_plain.begin(), sorted_plain.end());
            sort(sorted_out.begin(), sorted_out.end());
            if (sorted_plain != sorted_out) {
                cout << "万级混洗测试失败" << endl;
                return 1;
            }
        }
    }

    cout << "PI_shuffle test passed!" << endl;
    delete netio;
    return 0;
}
