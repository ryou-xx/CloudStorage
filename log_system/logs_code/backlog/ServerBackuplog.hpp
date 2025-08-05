#pragma once
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <pthread.h>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <functional>
#include <cstdlib>
#include <ctime>

using std::cout;
using std::endl;

using func_t = std::function<void(const std::string&)>;

const int backlog = 32;

class TcpServer;

// 存储客户端的ip和端口信息，以及服务器信息的结构体指针
class ThreadData{
public:
    ThreadData(int fd, const std::string &ip, const uint16_t &port, TcpServer *ts)
        : sock(fd), client_ip(ip), client_port(port), ts_(ts) {}
public:
    int sock;
    std::string client_ip;
    uint16_t client_port;
    TcpServer *ts_;
};

class TcpServer{
public:
    TcpServer(uint16_t port, func_t func)
        : port_(port), func_(func){}

    // 创建并初始化监听socket
    void init_service()
    {
        listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock_ == -1)
        {
            cout << __FILE__ << __LINE__ << "create socket error"
                << strerror(errno) << endl;
        }

        sockaddr_in local;
        bzero(&local, sizeof(sockaddr_in));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(port_);

        if (bind(listen_sock_, (sockaddr*)&local, sizeof(local)) < 0)
        {
            cout << __FILE__ << __LINE__ << "bind socket error"
                << strerror(errno) << endl;
        }

        if (listen(listen_sock_, backlog) < 0)
        {
            cout << __FILE__ << __LINE__ << "listen error"
                << strerror(errno) << endl;
        }
    }

    //
    static void *threadRoutine(void *args)
    {
        pthread_detach(pthread_self()); // 防止在start_service处阻塞

        ThreadData *td = static_cast<ThreadData *>(args);
        std::string client_info = td->client_ip + ":" + std::to_string(td->client_port);
        td->ts_->service(td->sock, move(client_info));
        close(td->sock);
        delete td; // start_service函数中用了new
        return nullptr;
    }

    void start_service()
    {
        while (true)
        {
            sockaddr_in client_addr;
            socklen_t client_addrlen = sizeof(sockaddr_in);
            std::cout << "waitting for client connection request..." << std::endl;
            int connfd = accept(listen_sock_, (sockaddr*)&client_addr, &client_addrlen);
            if (connfd == -1)
            {
                cout << __FILE__ << __LINE__ << "accept error" << strerror(errno) << endl;
                continue;
            }
            
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            uint16_t client_port = ntohs(client_addr.sin_port);
            std::cout << "A connection has been established with the server" << std::endl;
            std::cout << "IP : " << client_ip << std::endl;
            std::cout << "PORT : " << client_port << std::endl;
            pthread_t tid;
            ThreadData *td = new ThreadData(connfd, client_ip, client_port, this);
            pthread_create(&tid, nullptr, threadRoutine, td);
        }
    }

    void service(int sock, const std::string && client_info)
    {
        char buf[1024];

        int r_ret = read(sock, buf, sizeof(buf));
        if (r_ret == -1)
        {
            cout << __FILE__ << __LINE__ << "read error" << strerror(errno) << endl;
            perror("NULL");
        }
        else if (r_ret > 0)
        {
            buf[r_ret] = 0;
            func_(client_info + std::string(buf)); // 客户端ip+端口+发送的信息
        }
    }

    ~TcpServer() = default;

private:
    int listen_sock_;
    uint16_t port_;
    func_t func_;
};