单进程并发设计
开启监听，服务陷入死循环，一个客户端一个客户端处理
./singleprocess 127.0.0.1(地址) 54321(端口号)

g++ -I../echo -o singleprocess.out singleprocess.cpp 