reactor模式，读写是非阻塞的。事件型驱动，统一事件管理器，支持监听和事件分发。
有新的连接或可读或可写的时候，分发到对应的时间到不同的时间处理器。
返回EAGAIN或EWOULDBLOCK就结束当前的写操作，然后继续监听新事件的到来。
同一时间点，多个客户端请求在被处理，只不过进程不一样。

g++ -I../echo -I../epoll_common -o epollreactorsingleprocess.out  epollreactorsingleprocess.cpp