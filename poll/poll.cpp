#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <unordered_set>
#include "common.hpp"

/*
select的问题在于支持监听的文件描述符存在上限，一般为1024
poll没有上限
*/

void updateFds(std::unordered_set<int> &clientFds, pollfd **fds, int &nfds){
    if (*fds != nullptr){
        delete [] (*fds);
    }
    nfds = clientFds.size();
    *fds = new pollfd[nfds];
    int index = 0;
    for (const auto &clientFd : clientFds){
        (*fds)[index].fd = clientFd;
        (*fds)[index].events = POLLIN;
        (*fds)[index].revents = 0;
        index++;
    }
}

void handlerClient(int clientFd){
    std::string msg;
    if (not EchoServer::RecvMsg(clientFd, msg)){
        return;
    }
    EchoServer::SendMsg(clientFd, msg);
}

int main(int argc, char **argv){
    if (argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: ./poll 127.0.0.1 54321" << std::endl;
        return -1;
    }
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), false);
    if (sockFd < 0){
        return -1;
    }
    int nfds = 0;
    pollfd *fds = nullptr;
    std::unordered_set<int> clientFds;
    clientFds.insert(sockFd);
    EchoServer::SetNotBlock(sockFd);
    while(true)
    {
        updateFds(clientFds, &fds, nfds);
        int ret = poll(fds, nfds, -1);
        if (ret <= 0){
            if(ret < 0) perror("poll failed");
            continue;
        }
        for (int i = 0; i < nfds; i++){
            if (not (fds[i].revents && POLLIN))
                continue;
            int curFd = fds[i].fd;
            if(curFd == sockFd){
                EchoServer::LoopAccept(sockFd, 1024, [&clientFds](int clientFd){
                    clientFds.insert(clientFd);
                });
                continue;
            }
            handlerClient(curFd);
            clientFds.erase(curFd);
            close(curFd);
        }
    }

    return 0;
}