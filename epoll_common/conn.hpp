#pragma once
#include "common.hpp"

namespace EchoServer{
    class Conn{
        public:
            Conn(int fd, int epoll_fd, bool is_multi_io) : fd_(fd), epoll_fd_(epoll_fd), is_multi_io_(is_multi_io)
            {}
            bool Read(){
                do{
                    uint8_t data[100];
                    ssize_t ret = read(fd_, data, 100);
                    if (ret == 0){
                        perror("peer close connection");
                        return false;
                    }
                    if (ret < 0){
                        if (EINTR == errno) continue;
                        if (EAGAIN == errno or EWOULDBLOCK == errno) return true;
                        perror("read failed");
                        return false;
                    }
                    codec_.Decode(data, ret);
                } while(is_multi_io_);
                return true;
            }
            bool Write(bool autoEncode = true){
                if (autoEncode && 0 == send_len_) {
                    codec_.EnCode(message_, pkt_);
                }
                do {
                    if (send_len_ == pkt_.Len()) return true;
                    ssize_t ret = write(fd_, pkt_.Data() + send_len_, pkt_.Len() - send_len_);
                    if (ret < 0){
                        if (EINTR == errno) continue;
                        if (EAGAIN == errno or EWOULDBLOCK == errno) return true;
                        perror("write failed");
                        return false;
                    }
                    send_len_ += ret;
                }while (is_multi_io_);
                return true;
            }
            bool OneMessage() {return codec_.GetMessage(message_);}
            void Encode() {codec_.EnCode(message_, pkt_); }
            bool FinishWrite() {return send_len_ == pkt_.Len();}
            int Fd() const {return fd_; }
            int EpollFd() const {return epoll_fd_;}
        private:
            int fd_{0};             //关联的客户端连接的fd
            int epoll_fd_{0};       //关联的epoll实例的fd
            bool is_multi_io_{0};   //是否进行多次io，直到返回eagain或ewouldblock
            ssize_t send_len_{0};   //要发送的应答数据长度
            std::string message_;   //对于echoserver来说，即是获取的请求信息也是要发送的应答信息
            Packet pkt_;            //发送应答消息的二进制数据报
            Codec codec_;           //EchoServer的编解码
    };
}