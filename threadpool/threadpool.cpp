#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <thread>
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

void handler(char *argv[]){
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), true);
    if (sockFd < 0){
        return;
    }
    while(true){
        int clientFd = accept(sockFd, NULL, 0);
        if (clientFd < 0){
            perror("accept failed");
            continue;
        }
        handleClient(clientFd);
    }
}

int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./Threadpool 127.0.0.1 54321" << std::endl;
        return -1;
    }
    for (int i = 0; i < EchoServer::GetNProcs(); i++){
        std::thread(handler, argv).detach();
    }
    while(true) sleep(1);

    return 0;
}
