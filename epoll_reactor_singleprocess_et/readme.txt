reactor-et模式
需要循环执行IO读写操作，直到返回EAGAIN或EWOULDBLOCK。中断的情况下还有重新启动io读写。否则会出现数据仍然可以读，
但是没有读写的情况。

g++ -I../echo -I../epoll_common -o epollreactorsingleprocesset.out  epollreactorsingleprocesset.cpp