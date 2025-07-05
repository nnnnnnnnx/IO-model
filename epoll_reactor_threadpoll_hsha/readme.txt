epoll_threadpool_hsha
HSHA半同步半异步模式的并发模型。将网络IO操作和业务逻辑隔离开来。
并且在他们之间插入共享队列用于通信。模型分为三层，网络io层，
（Half Sync and Half Async）
半同步是指业务逻辑，半异步是网络层io逻辑。


g++ -I../echo -I../epoll_common -I../corotine -o epollreactorthreadpollhsha.out  epollreactorthreadpollhsha.cpp  