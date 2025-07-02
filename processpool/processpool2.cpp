#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "common.hpp"

/*
在processpool1中，所有子进程都会调用accept来接受新的客户请求，可能会导致
新的客户端来到的时候，各个子进程之间会存在争夺接受连接的机会，在2.6版本linux之前，所有子进程都会被唤醒。
但是只有一个调用成功。使用SO_REUSEPORT 让多个进程同时，监听在相同的网络地址（ip地址+端口），从而linux内核会
自动在多个进程之中形成负载均衡，而不会出现互斥等待行为。从而提高系统的性能和可靠性。
*/

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
        close(clientFd);
    }
}

int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./ProcessPool2 127.0.0.1 54321" << std::endl;
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

