#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include "common.hpp"

//多进程的并发模型存在的问题在于需要频繁的创建和销毁进程，而这样系统开销较大，资源占用也较多，因此可以使用
//多线程的并发模型，创建和销毁系统开销低，资源占用较少

void handlerClient(int clientFd)
{
    std::string msg;
    if(not EchoServer::RecvMsg(clientFd, msg)){
        return;
    }
    EchoServer::SendMsg(clientFd, msg);
    close(clientFd);
}

int main(int argc, char **argv)
{
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "exapmle: ./Multithread 127.0.0.1 1688" << std::endl;
        return -1;
    }
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), false);
    if (sockFd < 0)
    {
        return -1;
    }
    while(true){
        int clientFd = accept(sockFd, NULL, 0);
        if (clientFd < 0){
            perror("accept failed");
            continue;
        }
        std::thread(handlerClient, clientFd).detach(); //detach函数以使创建的线程独立运行
    }

    return 0;
}
