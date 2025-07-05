#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include "epollctl.hpp"

//异步 i/o操作和业务逻辑操作隔离开来
//半同步半异步

std::mutex Mutex;
std::condition_variable Cond;
std::queue<EchoServer::Conn *> Queue;

void pushInQueue(EchoServer::Conn *conn)
{
    {
        std::unique_lock<std::mutex> locker(Mutex);
        Queue.push(conn);
    }
    Cond.notify_one();
}

EchoServer::Conn *getQueueData(){
    std::unique_lock<std::mutex> locker(Mutex);
    Cond.wait(locker, []()->bool{
        return Queue.size() > 0;
    });
    EchoServer::Conn *conn = Queue.front();
    Queue.pop();
    return conn;
}

void workerHandler(bool directSend)
{
    while(true)
    {
        EchoServer::Conn *conn = getQueueData();
        conn->Encode();
        if (directSend){
            while(not conn->FinishWrite())
            {
                if (not conn->Write(false)){
                    break;
                }
            }
            EchoServer::ClearEvent(conn);
            delete conn;
        }
        else{
            EchoServer::ModToWriteEvent(conn); //监听写事件，数据通过io线程发送
        }
    }
}

void ioHandler(char *argv[])
{
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
    int msec = -1;
    while(true){
        int num = epoll_wait(epollFd, events, 2048, msec);
        if (num < 0){
            perror("epoll_wait failed");
            continue;
        }
        for (int i = 0; i < num; i++){
            EchoServer::Conn *conn = (EchoServer::Conn *)events[i].data.ptr;
            if (conn->Fd() == sockFd){
                EchoServer::LoopAccept(sockFd, 2048, [epollFd](int clientFd){
                    EchoServer::Conn *conn = new EchoServer::Conn(clientFd, epollFd, true);
                    EchoServer::SetNotBlock(clientFd);
                    EchoServer::AddReadEvent(conn, false, true);
                });
                continue;
            }
            auto releaseConn = [&conn](){
                EchoServer::ClearEvent(conn);
                delete conn;
            };
            if (events[i].events & EPOLLIN) //可读
            {
                if (not conn->Read()){
                    releaseConn();
                    continue;
                }
                if (conn->OneMessage()){
                    pushInQueue(conn); //加入共享队列，有锁
                }else{
                    EchoServer::ReStartReadEvent(conn); //重新监听开启oneshot
                }
            }
            if (events[i].events & EPOLLOUT){
                //可写
                if (not conn->Write(false)) //执行非阻塞写
                {
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
    if (argc != 4){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./epollreactorthreadpollhsha 127.0.0.1 54321 1" << std::endl;
        return -1;
    }
    bool directSend = (std::string(argv[3]) == "1");
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        std::thread(workerHandler, directSend).detach();
    }
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        std::thread(ioHandler, argv).detach();
    }  
    while(true)
        sleep(1);
    return 0;
}