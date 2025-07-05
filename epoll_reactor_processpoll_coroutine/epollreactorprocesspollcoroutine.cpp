#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "../corotine/coroutine.h"
#include "../epoll_common/epollctl.hpp"
#include "../echo/common.hpp"

struct EventData{
    EventData(int fd, int epoll_fd) : fd_(fd), epoll_fd_(epoll_fd){};
    int fd_{0};
    int epoll_fd_{0};
    int cid_{MyCoroutine::INVALID_ROUTINE_ID};
    MyCoroutine::Schedule *schedule_{nullptr};
};

void EchoDeal(const std::string reqMessage, std::string &respMessage){
    respMessage = reqMessage;
}

void handlerClient(void *arg){
    EventData *eventData = (EventData *)arg;
    auto releaseConn = [&eventData](){
        EchoServer::ClearEvent(eventData->epoll_fd_, eventData->fd_);
        delete eventData;
    };
    ssize_t ret = 0;
    EchoServer::Codec codec;
    std::string reqMessage;
    std::string respMessage;
    while(true){
        uint8_t data[100];
        ret = read(eventData->fd_, data, 100);
        if (ret == 0){
            perror("peer close connection");
            releaseConn();
            return;
        }
        if (ret < 0){
            if (EINTR == errno) continue;
            if (EAGAIN == errno or EWOULDBLOCK == errno){
                MyCoroutine::CoroutineYield(*eventData->schedule_);
                continue;
            }
            perror("read failed");
            releaseConn();
            return;
        }
        codec.Decode(data, ret);
        if (codec.GetMessage(reqMessage)){
            break;
        }
    }
    EchoDeal(reqMessage, respMessage);
    EchoServer::Packet pkt;
    codec.EnCode(respMessage, pkt);
    EchoServer::ModToWriteEvent(eventData->epoll_fd_, eventData->fd_, eventData);
    ssize_t sendLen = 0;
    while (sendLen != pkt.Len()){
        ret = write(eventData->fd_, pkt.Data() + sendLen, pkt.Len() - sendLen);
        if (ret < 0){
            if (EINTR == errno) continue;
            if (EAGAIN == errno or EWOULDBLOCK == errno){
                MyCoroutine::CoroutineYield(*eventData->schedule_);
                continue;
            }
            perror("write failed");
            releaseConn();
            return;
        }
        sendLen += ret;
    }
    releaseConn();
}

void handler(char **argv){
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), true);
    if (sockFd < 0){
        return;
    }
    epoll_event events[2048];
    int epollFd = epoll_create(1024);
    if (epollFd < 0){
        perror("epoll_create failed");
        return;
    }
    EventData eventData(sockFd, epollFd);
    EchoServer::SetNotBlock(sockFd);
    EchoServer::AddReadEvent(epollFd, sockFd, &eventData);
    MyCoroutine::Schedule schedule;
    MyCoroutine::ScheduleInit(schedule, 10000);
    int msec = -1;
    while(true){
        int num = epoll_wait(epollFd, events, 2048, msec);
        if (num < 0){
            perror("epoll_wait failed");
            continue;
        }
        else if(num == 0){ //没有事件了，下次调用epoll_Wait大概率挂起
            sleep(0); //主动让出cpu
            msec = -1;//大概率挂起，设置超时时间为-1
            continue;
        }
        msec = 0;
        for (int i = 0; i < num; i++){
            EventData *eventData = (EventData*)events[i].data.ptr;
            if (eventData->fd_ == sockFd){
                EchoServer::LoopAccept(sockFd, 2048, [&epollFd](int clientFd){
                    EventData *eventData = new EventData(clientFd, epollFd);
                    EchoServer::SetNotBlock(clientFd);
                    EchoServer::AddReadEvent(epollFd, clientFd, eventData);
                });
                continue;
            }
            if (eventData->cid_ == MyCoroutine::INVALID_ROUTINE_ID){
                if(MyCoroutine::CoroutineCanCreate(schedule)){
                    eventData->schedule_ = &schedule;
                    eventData->cid_ = MyCoroutine::CoroutineCreate(schedule, handlerClient, eventData, 0);
                    MyCoroutine::CoroutineResumeById(schedule, eventData->cid_);
                }else{
                    std::cout << "MyCoroutine is full"  << std::endl;
                }
            }else{
                MyCoroutine::CoroutineResumeById(schedule, eventData->cid_);
            }
        }
        MyCoroutine::ScheduleTryReleaseMemory(schedule);
    }
}

int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "examlpe: ./epollreactorProcessCoroutine 127.0.0.1 54321" << std::endl;
        return -1;
    }
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        pid_t pid = fork();
        if (pid < 0){
            perror("fork failed");
            continue;
        }
        if (0 == pid){
            handler(argv);
            exit(0);
        }
    }
    while(true) sleep(1);
    return 0;
}