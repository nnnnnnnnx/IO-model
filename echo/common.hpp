#pragma once
#include <assert.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <functional>
#include "codec.hpp"

namespace EchoServer{
    int GetNProcs() {return get_nprocs(); } //获取系统有多少可用的CPU
    bool SendMsg(int fd, const std::string message) //在阻塞io模式下发送应答消息
    {
        EchoServer::Packet pkt;
        EchoServer::Codec codec;
        codec.EnCode(message, pkt);
        ssize_t sendLen = 0;
        while(sendLen != pkt.Len()){
            ssize_t ret = write(fd, pkt.Data() + sendLen, pkt.Len() - sendLen);
            if (ret < 0){
                if (errno == EINTR) continue; //中断情况可以重试
                perror("write failed");
                return false;
            }
            sendLen += ret;
        }
        return true;
    }
    bool RecvMsg(int fd, std::string &message) //在阻塞io模式下接受请求命令
    {
        uint8_t data[4 * 1024];
        EchoServer::Codec codec;
        while(not codec.GetMessage(message)){
            ssize_t ret = read(fd, data, 4 * 1024);
            if(ret <= 0){
                if(errno == EINTR) continue;
                perror("read failed");
                return false;
            }
            codec.Decode(data, ret);
        }
        return true;
    }
    void SetNotBlock(int fd){
        int oldOpt = fcntl(fd, F_GETFL);
        assert(oldOpt != -1);
        assert(fcntl(fd, F_SETFL, oldOpt | O_NONBLOCK) != -1);
    }
    void SetTimeOut(int fd, int64_t sec, int64_t usec){
        struct timeval tv;
        tv.tv_sec = sec; //秒
        tv.tv_usec = usec; //微秒
        assert(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != -1);
        assert(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != -1);
    }
    int CreateListenSocket(char *ip, int port, bool isReusePort)
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip);
        int sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockFd < 0){
            perror("socket failed");
            return -1;
        }
        int reuse = 1;
        int opt = SO_REUSEADDR;
        if(isReusePort) opt = SO_REUSEPORT;
        if (setsockopt(sockFd, SOL_SOCKET, opt, &reuse, sizeof(reuse)) != 0){
            perror("setsockopt failed");
            return -1;
        }
        if(bind(sockFd, (sockaddr*)&addr, sizeof(addr)) != 0){
            perror("bind failed");
            return -1;
        }
        if(listen(sockFd, 1024) != 0){
            perror("listen failed");
            return -1;
        }
        return sockFd;
    }
    //在调用本函数之前，一定要设置socket非阻塞
    void LoopAccept(int sockFd, int maxConn, std::function<void(int clientFd)> clientAcceptCallBack){
        while(maxConn--){
            int clientFd = accept(sockFd, NULL, 0);
            if(clientFd > 0){
                clientAcceptCallBack(clientFd);
                continue;
            }
            if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR){
                perror("accept failed");
            }
            break;
        }
    }
}