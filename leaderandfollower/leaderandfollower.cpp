#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <mutex> 
#include <thread>
#include "common.hpp"

//领导者-跟随者模式
std::mutex Mutex;
void handleClient(int clientFd)
{
    std::string msg;
    if (not EchoServer::RecvMsg(clientFd, msg)){
        return;
    }
    EchoServer::SendMsg(clientFd, msg);
    close(clientFd);
}

void handler(int sockFd)
{
    while(true){
        int clientFd;
        //跟随者等待获取锁，成为领导者
        {
            std::lock_guard<std::mutex> guard(Mutex);
            clientFd = accept(sockFd, NULL, 0);
            if (clientFd < 0){
                perror("accept failed");
                continue;
            }
        }
        handleClient(clientFd); //处理每个客户端请求
    }
}


int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./LeaderAndFollower 127.0.0.1 54321" << std::endl;
        return -1;
    }
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), false);
    if (sockFd < 0){
        return -1;
    }
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        std::thread(handler, sockFd).detach();
    }
    while(true) sleep(1);

    return 0;
}