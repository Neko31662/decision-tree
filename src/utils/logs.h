#pragma once
#include "config/config.h"
#include <cstdio>
#include <iostream>
#include <string>

inline void error(std::string msg) {
    std::cout << "ERROR: " << msg << std::endl;
    exit(1);
}

// 自定义缓冲区
class BufferStreamBuf : public std::streambuf {
  public:
    std::string buffer;

  protected:
    // 每次写入一个字符时调用
    int overflow(int c) override {
        if (c != EOF)
            buffer.push_back(static_cast<char>(c));
        return c;
    }

    // 刷新时调用（例如 std::endl）
    int sync() override {
        return 0; // 不需要立即输出
    }
};

// 自定义 ostream 封装类
class MemoryOStream : public std::ostream {
  public:
    MemoryOStream() : std::ostream(&buf_) {}

    // 获取保存的内容
    std::string str() const { return buf_.buffer; }

    // 输出保存的内容到控制台
    void show() const { std::cout << buf_.buffer; }

    // 清空缓存
    void clear_buffer() {
        buf_.buffer.clear();
        this->std::ostream::clear(std::ios_base::goodbit);
    }

  private:
    BufferStreamBuf buf_;
};
inline MemoryOStream myout;

inline void log(std::string msg, std::ostream &out = std::cout) {
    fflush(stderr);
    fflush(stdout);
    out << msg << std::endl;
    fflush(stdout);
}