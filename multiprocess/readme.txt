multiprocess
使用fork()创建子进程去负责业务流程(信息收发)
主进程负责建立连接。

g++ -I../echo -o multiprocess.out multiprocess.cpp