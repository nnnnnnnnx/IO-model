简单的领导者跟随者模式（leaderandfollower）
领导者负责监听客户端连接的到来，一次只能有一个领导者。接受完连接以后，变成工作进程。

g++ -I../echo -o leaderandfollower.out leaderandfollower.cpp