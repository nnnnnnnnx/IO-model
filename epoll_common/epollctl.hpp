#pragma once
#include "conn.hpp"
#include <sys/epoll.h>

namespace EchoServer{
    inline void AddReadEvent(Conn *conn, bool isET = false, bool isOneShot = false){
        epoll_event event;
        event.data.ptr = (void *)conn;
        event.events = EPOLLIN;
        if (isET) event.events |= EPOLLET;
        if (isOneShot) event.events |= EPOLLONESHOT;
        assert(epoll_ctl(conn->EpollFd(), EPOLL_CTL_ADD, conn->Fd(), &event) != -1);
    }
    inline void AddReadEvent(int epollFd, int fd, void *userData){
        epoll_event event;
        event.data.ptr = userData;
        event.events = EPOLLIN;
        assert(epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) != -1);
    }
    inline void ReStartReadEvent(Conn *conn){
        epoll_event event;
        event.data.ptr = (void *) conn;
        event.events = EPOLLIN | EPOLLONESHOT;
        assert(epoll_ctl(conn->EpollFd(), EPOLL_CTL_MOD, conn->Fd(), &event) != -1);
    }
    inline void ModToWriteEvent(Conn *conn, bool isET = false){
        epoll_event event;
        event.data.ptr = (void *) conn;
        event.events = EPOLLOUT;
        if (isET) event.events |= EPOLLET;
        assert(epoll_ctl(conn->EpollFd(), EPOLL_CTL_MOD, conn->Fd(), &event) != -1);
    }
    inline void ModToWriteEvent(int epollFd, int fd, void *userData){
        epoll_event event;
        event.data.ptr = userData;
        event.events = EPOLLOUT;
        assert(epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event) != -1);
    }
    inline void ClearEvent(Conn *conn, bool isClose = true){
        assert(epoll_ctl(conn->EpollFd(), EPOLL_CTL_DEL, conn->Fd(), nullptr) != -1);
        if (isClose) close(conn->Fd());
    }
    inline void ClearEvent(int epollFd, int fd){
        assert(epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) != -1);
        close(fd);    
    }
}