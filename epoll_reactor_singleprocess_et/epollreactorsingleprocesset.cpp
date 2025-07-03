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
#include "epollctl.hpp"

int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./EpollReactorSingleProcessEt 127.0.0.1 54321" << std::endl;
        return -1;
    }
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), false);
    if (sockFd < 0){
        return -1;
    }
    epoll_event events[2048];
    int epollFd = epoll_create(1024);
    if (epollFd < 0){
        perror("epoll_create failed");
        return -1;
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
            EchoServer::Conn *conn = (EchoServer::Conn *)events[i].data.ptr;
            if(conn->Fd() == sockFd){
                EchoServer::LoopAccept(sockFd, 2048, [epollFd](int clientFd){
                    EchoServer::Conn *conn = new EchoServer::Conn(clientFd, epollFd, true);
                    EchoServer::SetNotBlock(clientFd);
                    EchoServer::AddReadEvent(conn, true);
                });
                continue;
            }
            auto releaseConn = [&conn]{
                EchoServer::ClearEvent(conn);
                delete conn;
            };
            if (events[i].events & EPOLLIN){
                if (not conn->Read()){
                    releaseConn();
                    continue;
                }
                if (conn->OneMessage()){
                    EchoServer::ModToWriteEvent(conn, true);
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


    return 0;
}