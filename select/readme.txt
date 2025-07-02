i/o多路复用 阻塞io
select
可以同时监听多个文件描述符上的事件，有大小限制默认监听1024个文件描述符。
select死循环，监听事件。


g++ -I../echo -o select.out select.cpp