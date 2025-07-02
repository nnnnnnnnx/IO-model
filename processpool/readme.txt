processpool进程池，预先创建指定数量的进程，每个进程不退出，而是一直为不同的客户端提供服务。
减少进程的创建和销毁，提高并发能力。
processpool2相对于processpool1，添加了socket的SO_REUSEPORT选项，让多个进程同时监控在相同的网络地址（IP地址+端口）。
linux内核会自动在多个进程之间进行连接的负载均衡。否则，多个进程会互斥等待，这样可以提高系统的性能和可靠性。

g++ -I../echo -o processpool1.out processpool1.cpp
g++ -I../echo -o processpool2.out processpool2.cpp