#pragma once
#include <string>
#include <vector>
#include <cassert>
#include "Util.hpp"

namespace mylog{
    class Buffer{
    protected:
        std::vector<char> buffer_;  // 缓冲区
        size_t write_pos_;          // 有效数据的长度
        size_t read_pos_;           // 已读数据的长度

        void ToBeEnough(size_t len)
        {
            int buffer_size = buffer_.size();
            if (len >= WriteableSize())
            {
                // 在缓冲区较小时使用倍率扩充，当缓冲区较大是使用线性扩充
                if (buffer_.size() < Util::JsonData::GetJsonData().threadhold)
                {
                    buffer_.resize(2 * buffer_.size() + buffer_size);
                }
                else
                {
                    buffer_.resize(Util::JsonData::GetJsonData().linear_growth + buffer_size);
                }
            }
        }
    public:
        Buffer() : write_pos_(0), read_pos_(0)
        {
            buffer_.resize(Util::JsonData::GetJsonData().buffer_size);
        }

        void Push(const char *data, size_t len)
        {
            ToBeEnough(len);
            
            std::copy(data, data + len, &buffer_[write_pos_]);
            write_pos_ += len;
        }

        bool IsEmpty() { return write_pos_ == read_pos_;}

        void Swap(Buffer &buf)
        {
            buffer_.swap(buf.buffer_);
            std::swap(read_pos_, buf.read_pos_);
            std::swap(write_pos_, buf.write_pos_);
        }

        // 获取写空间剩余容量
        size_t WriteableSize()
        {
            return buffer_.size() - write_pos_;
        }

        // 获取读空间剩余容量
        size_t ReadableSize()
        {
            return write_pos_ - read_pos_;
        }

        const char *Begin() { return &buffer_[read_pos_]; }

        void MoveWirtePos(int len)
        {
            assert(len <= WriteableSize());
            write_pos_ += len;
        }

        void MoveReadPos(int len)
        {
            assert(len <= ReadableSize());
            read_pos_ += len;
        }

        // 重置缓冲区
        void Reset()
        {
            write_pos_ = 0;
            read_pos_ = 0;
        }
    };
}