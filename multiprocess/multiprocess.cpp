#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "common.hpp"

void handlerClient(int clientFd)
{
    std::string msg;
    if(not EchoServer::RecvMsg(clientFd, msg)){
        return;
    }
    EchoServer::SendMsg(clientFd, msg);
    close(clientFd);
}

void childExitSignalHandler()
{
    struct sigaction act;
    act.sa_handler = SIG_IGN; //设置信号处理函数，忽略子进程退出信号
    sigemptyset(&act.sa_mask); //信号屏蔽设置为空，在执行 SIGCHLD 信号处理函数时，不额外屏蔽其他任何信号。
    act.sa_flags = 0; //标志位设置为0设置标志位为 0，表示使用默认行为。
    //sa_flags 的其他可选值比如 SA_RESTART（自动重启被中断的系统调用），SA_NOCLDWAIT（不生成僵尸进程）等，但这里我们直接设置为 0 表示不使用这些额外行为。
    sigaction(SIGCHLD, &act, NULL);
}

int main(int argc, char **argv)
{
    if(argc != 3){
        std::cout << "invalid input" << std::endl;
        std::cout << "example: /MultiProcess 127.0.0.1 54321" << std::endl;
        return -1;
    }
    int sockFd = EchoServer::CreateListenSocket(argv[1], atoi(argv[2]), false);
    if(sockFd < 0){
        return -1;
    }
    childExitSignalHandler();
    while(true)
    {
        int clientFd = accept(sockFd, nullptr, 0);
        if (clientFd < 0){
            perror("accept failed");
            continue;
        }
        pid_t pid = fork();
        if (pid == -1){
            perror("fork failed");
            continue;
        }
        if (pid == 0) //子进程
        {
            handlerClient(clientFd);
            exit(0); //处理完子进程直接退出
        }else{
            close(clientFd);
            //父进程直接关闭客户端连接，否者会泄露
        }
    }
}
