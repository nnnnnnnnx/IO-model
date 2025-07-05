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
#include <thread>
#include "epollctl.hpp"

void handler(char *argv[]){
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
    EchoServer::Conn conn(sockFd, epollFd, true);
    EchoServer::SetNotBlock(sockFd);
    EchoServer::AddReadEvent(&conn);
        
    while(true){
        int num = epoll_wait(epollFd, events, 2048, -1);
        if (num < 0){
            perror("epoll_wait failed");
            continue;
        }
        for (int i = 0; i < num; i++){
            EchoServer::Conn *conn = (EchoServer::Conn *) events[i].data.ptr;
            if (conn->Fd() == sockFd){
                EchoServer::LoopAccept(sockFd, 2048, [epollFd](int clientFd){
                    EchoServer::Conn *conn = new EchoServer::Conn(clientFd, epollFd, true);
                    EchoServer::SetNotBlock(clientFd);
                    EchoServer::AddReadEvent(conn);//添加可读事件
                });
                continue;
            }
            auto releaseConn = [&conn](){
                EchoServer::ClearEvent(conn);
                delete conn;
            };
            if (events[i].events & EPOLLIN){
                if (not conn->Read()){
                    releaseConn();
                    continue;
                }
                if (conn->OneMessage()){
                    EchoServer::ModToWriteEvent(conn);
                }
            }
            if (events[i].events & EPOLLOUT){
                if (not conn->Write()){
                    releaseConn();
                    continue;
                }
                if (conn->FinishWrite()){
                    releaseConn();
                }
            }
        }
    }
}

int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./EpollReactorThreadPool 0.0.0.0 54321" << std::endl;
        return -1;
    }
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        std::thread(handler, argv).detach();
    }
    while(true) sleep(1);
    return 0;
}