#include "utils/multi_party_net_io.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

// 基础端口号
const int BASE_PORT = 12345;
const int NUM_PARTIES = 3;

// 测试基本的发送和接收功能
void test_basic_send_recv(NetIOMP *netio, int party_id) {
    cout << "\n=== Party " << party_id << " starting basic send/recv test ===" << endl;

    // 准备发送的消息
    string base_msg = "Hello from Party " + to_string(party_id);

    // 使用轮次机制避免死锁
    // 每一轮由一个party发送消息给其他所有party
    for (int sender = 0; sender < NUM_PARTIES; sender++) {
        if (sender == party_id) {
            // 当前party是发送方
            cout << "Party " << party_id << " sending messages to all other parties..." << endl;

            for (int receiver = 0; receiver < NUM_PARTIES; receiver++) {
                if (receiver == party_id)
                    continue;

                string msg = base_msg + " to Party " + to_string(receiver);
                int msg_len = msg.length() + 1; // 包含'\0'

                // 先发送长度
                netio->send_data(receiver, &msg_len, sizeof(int));
                // 再发送消息内容
                netio->send_data(receiver, msg.c_str(), msg_len);

                cout << "  -> Sent to Party " << receiver << ": \"" << msg << "\"" << endl;
            }

            // 刷新所有发送缓冲区
            netio->flush_all();
            cout << "Party " << party_id << " finished sending in round " << sender << endl;

        } else {
            // 当前party是接收方
            cout << "Party " << party_id << " receiving message from Party " << sender << "..."
                 << endl;

            int msg_len = 0;
            // 先接收长度
            netio->recv_data(sender, &msg_len, sizeof(int));

            // 接收消息内容
            char *recv_buffer = new char[msg_len];
            netio->recv_data(sender, recv_buffer, msg_len);

            cout << "  <- Received from Party " << sender << ": \"" << recv_buffer << "\"" << endl;

            delete[] recv_buffer;
        }
    }

    cout << "Party " << party_id << " completed all send/recv operations." << endl;
}

// 测试双向通信
void test_bidirectional_comm(NetIOMP *netio, int party_id) {
    cout << "\n=== Party " << party_id << " starting bidirectional communication test ===" << endl;

    // 每个party与其他party进行一次双向通信
    for (int other = 0; other < NUM_PARTIES; other++) {
        if (other == party_id)
            continue;

        if (party_id < other) {
            // 先发送后接收
            string send_msg =
                "Bidirectional message from P" + to_string(party_id) + " to P" + to_string(other);
            int send_len = send_msg.length() + 1;

            cout << "Party " << party_id << " -> Party " << other << ": \"" << send_msg << "\""
                 << endl;
            netio->send_data(other, &send_len, sizeof(int));
            netio->send_data(other, send_msg.c_str(), send_len);
            netio->flush(other);

            // 接收回复
            int recv_len = 0;
            netio->recv_data(other, &recv_len, sizeof(int));
            char *recv_buffer = new char[recv_len];
            netio->recv_data(other, recv_buffer, recv_len);

            cout << "Party " << party_id << " <- Party " << other << ": \"" << recv_buffer << "\""
                 << endl;
            delete[] recv_buffer;

        } else {
            // 先接收后发送
            int recv_len = 0;
            netio->recv_data(other, &recv_len, sizeof(int));
            char *recv_buffer = new char[recv_len];
            netio->recv_data(other, recv_buffer, recv_len);

            cout << "Party " << party_id << " <- Party " << other << ": \"" << recv_buffer << "\""
                 << endl;
            delete[] recv_buffer;

            // 发送回复
            string send_msg = "Reply from P" + to_string(party_id) + " to P" + to_string(other);
            int send_len = send_msg.length() + 1;

            cout << "Party " << party_id << " -> Party " << other << ": \"" << send_msg << "\""
                 << endl;
            netio->send_data(other, &send_len, sizeof(int));
            netio->send_data(other, send_msg.c_str(), send_len);
            netio->flush(other);
        }
    }

    cout << "Party " << party_id << " completed bidirectional communication test." << endl;
}

// 测试同步功能
void test_sync(NetIOMP *netio, int party_id) {
    cout << "\n=== Party " << party_id << " testing synchronization ===" << endl;

    auto start = chrono::high_resolution_clock::now();

    cout << "Party " << party_id << " calling sync()..." << endl;
    netio->sync();

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Party " << party_id << " sync completed in " << duration << " ms" << endl;
}

int test_once(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <party_id>" << endl;
        cerr << "  party_id: 0, 1, or 2" << endl;
        return 1;
    }

    int party_id = atoi(argv[1]);

    if (party_id < 0 || party_id >= NUM_PARTIES) {
        cerr << "Error: party_id must be between 0 and " << (NUM_PARTIES - 1) << endl;
        return 1;
    }

    cout << "========================================" << endl;
    cout << "Multi-Party NetIO Test" << endl;
    cout << "Party ID: " << party_id << endl;
    cout << "Base Port: " << BASE_PORT << endl;
    cout << "Number of Parties: " << NUM_PARTIES << endl;
    cout << "========================================" << endl;

    try {
        // 创建NetIOMP实例
        cout << "\nParty " << party_id << " initializing NetIOMP..." << endl;
        NetIOMP *netio = new NetIOMP(party_id, BASE_PORT, NUM_PARTIES);
        cout << "Party " << party_id << " NetIOMP initialized successfully." << endl;

        // 初始同步，确保所有连接建立完成
        cout << "\nParty " << party_id << " performing initial sync..." << endl;
        netio->sync();
        cout << "Party " << party_id << " initial sync completed." << endl;

        // 测试1: 基本发送接收
        test_basic_send_recv(netio, party_id);
        netio->sync();

        // 测试2: 双向通信
        test_bidirectional_comm(netio, party_id);
        netio->sync();

        // 测试3: 同步测试
        test_sync(netio, party_id);

        // 统计通信量
        int64_t total_bytes = netio->count();
        cout << "\nParty " << party_id << " total communication: " << total_bytes << " bytes"
             << endl;

        // 清理
        cout << "\nParty " << party_id << " cleaning up..." << endl;
        delete netio;

        cout << "========================================" << endl;
        cout << "Party " << party_id << " test completed successfully!" << endl;
        cout << "========================================" << endl;

        return 0;

    } catch (const exception &e) {
        cerr << "Party " << party_id << " error: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Party " << party_id << " unknown error occurred!" << endl;
        return 1;
    }
}

int main(int argc, char **argv) {
    // 重复运行两次以确保结构体被正确销毁、释放占用的端口
    if (test_once(argc, argv))
        return 1;
    if (test_once(argc, argv))
        return 1;
}
