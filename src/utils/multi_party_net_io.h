#pragma once
#include "config/config.h"
#include <emp-tool/emp-tool.h>

class NetIOMP {
  public:
    std::vector<std::shared_ptr<emp::NetIO>> ios1;
    std::vector<std::shared_ptr<emp::NetIO>> ios2;
    std::vector<std::string> stored_data;
    // NetIO *ios[nP + 1];
    // NetIO *ios2[nP + 1];
    int party;
    int nParty;
    vector<bool> sent;
    // bool sent[nP + 1];

    NetIOMP(int party, int port, int nParty) {
        this->party = party;
        this->nParty = nParty;
        sent.resize(nParty, false);
        ios1.resize(nParty);
        ios2.resize(nParty);
        stored_data.resize(nParty);
        for (int i = 0; i < nParty; ++i)
            for (int j = 0; j < nParty; ++j)
                if (i < j) {
                    if (i == party) {
#ifdef LOCALHOST
                        usleep(1000);
                        ios1[j] =
                            std::make_shared<emp::NetIO>(IP[j], port + 2 * (i * nParty + j), true);
#else
                        usleep(1000);
                        ios1[j] = std::make_shared<emp::NetIO>(IP[j], port + 2 * (i), true);
#endif
                        ios1[j]->set_nodelay();

#ifdef LOCALHOST
                        usleep(1000);
                        ios2[j] = std::make_shared<emp::NetIO>(
                            nullptr, port + 2 * (i * nParty + j) + 1, true);
#else
                        usleep(1000);
                        ios2[j] = std::make_shared<emp::NetIO>(nullptr, port + 2 * (j) + 1, true);
#endif
                        ios2[j]->set_nodelay();
                    } else if (j == party) {
#ifdef LOCALHOST
                        usleep(1000);
                        ios1[i] = std::make_shared<emp::NetIO>(nullptr, port + 2 * (i * nParty + j),
                                                               true);
#else
                        usleep(1000);
                        ios1[i] = std::make_shared<emp::NetIO>(nullptr, port + 2 * (i), true);
#endif
                        ios1[i]->set_nodelay();

#ifdef LOCALHOST
                        usleep(1000);
                        ios2[i] = std::make_shared<emp::NetIO>(
                            IP[i], port + 2 * (i * nParty + j) + 1, true);
#else
                        usleep(1000);
                        ios2[i] = std::make_shared<emp::NetIO>(IP[i], port + 2 * (j) + 1, true);
#endif
                        ios2[i]->set_nodelay();
                    }
                }
    }
    int64_t count() {
        int64_t res = 0;
        for (int i = 0; i < nParty; ++i)
            if (i != party) {
                res += ios1[i]->counter;
                res += ios2[i]->counter;
            }
        return res;
    }

    ~NetIOMP() {}
    void send_data(int dst, const void *data, size_t len) {
        if (dst != party) {
            if (party < dst)
                ios1[dst]->send_data(data, len);
            else
                ios2[dst]->send_data(data, len);
            sent[dst] = true;
        }
#ifdef __MORE_FLUSH
        flush(dst);
#endif
    }
    void store_data(int dst, const void *data, size_t len) {
        if (dst != party) {
            stored_data[dst].append((char *)data, len);
        }
    }
    void send_stored_data(int dst) {
        if (dst != party) {
            if (stored_data[dst].size() > 0) {
                send_data(dst, stored_data[dst].data(), stored_data[dst].size());
                stored_data[dst].clear();
            }
        }
    }
    void send_stored_data_all() {
        for (int i = 0; i < nParty; ++i)
            if (i != party) {
                send_stored_data(i);
            }
    }
    void recv_data(int src, void *data, size_t len) {
        if (src != party) {
            if (sent[src])
                flush(src);
            if (src < party)
                ios1[src]->recv_data(data, len);
            else
                ios2[src]->recv_data(data, len);
        }
    }
    std::shared_ptr<emp::NetIO> &get(size_t idx, bool b = false) {
        if (b)
            return ios1[idx];
        else
            return ios2[idx];
    }
    void flush(int idx) {
        if (party < idx)
            ios1[idx]->flush();
        else
            ios2[idx]->flush();
    }
    void flush_all() {
        for (int i = 0; i < nParty; ++i)
            if (i != party) {
                ios1[i]->flush();
                ios2[i]->flush();
            }
    }
    void sync() {
        for (int i = 0; i < nParty; ++i)
            for (int j = 0; j < nParty; ++j)
                if (i < j) {
                    if (i == party) {
                        ios1[j]->sync();
                        ios2[j]->sync();
                    } else if (j == party) {
                        ios1[i]->sync();
                        ios2[i]->sync();
                    }
                }
    }
};

inline std::vector<uint64_t> get_netio_bytes(int party_id, NetIOMP &netio, int PARTY_NUM = 3) {
    std::vector<uint64_t> res;
    for (int i = 0; i < PARTY_NUM; ++i) {
        if (i != party_id) {
            res.push_back(netio.ios1[i]->counter + netio.ios2[i]->counter);
        } else {
            res.push_back(0);
        }
    }
    return res;
}