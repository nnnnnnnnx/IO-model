#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "common.hpp"

void handleClient(int clientFd)
{
    std::string msg;
    if (not EchoServer::RecvMsg(clientFd, msg)){
        return;
    }
    EchoServer::SendMsg(clientFd, msg);
    close(clientFd);
}

void handler(int sockFd){
    while(true){
        int clientFd = accept(sockFd, nullptr, 0);
        if (clientFd < 0){
            perror("accept failed");
            continue;
        }
        handleClient(clientFd);
        close(clientFd);
    }
}

int main(int argc, char **argv){
    if(argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./MultiThread 127.0.0.1 54321" << std::endl;
        return -1;
    }
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), false);
    if (sockFd < 0){
        return -1;
    }
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        pid_t pid = fork();
        if(pid < 0){
            perror("fork failed");
            continue;
        }
        if(0 == pid){
            handler(sockFd);
            exit(0);
        }
    }
    while(true) sleep(1);

    return 0;
}