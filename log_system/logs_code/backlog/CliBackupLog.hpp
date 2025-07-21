#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../Util.hpp"

extern mylog::Util::JsonData *g_conf_data;

void start_backup(const std::string &message)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cout << __FILE__ << __LINE__ << "socket error: " << strerror(errno) << std::endl;
        perror(NULL);
    }

    sockaddr_in serv;
    bzero(&serv, sizeof(sockaddr_in));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(g_conf_data->backup_port);
    inet_aton(g_conf_data->backup_addr.c_str(), &serv.sin_addr);

    int cnt = 5;
    while (-1 == connect(sock, (sockaddr*)&serv, sizeof(serv)))
    {
        std::cout << "正在尝试重新连接，重连次数还有：" << cnt << std::endl;
        if (cnt <= 0)
        {
            std::cout << __FILE__ << __LINE__ << "connect error: " << strerror(errno) << std::endl;
            perror(NULL);
            return;
        }
    }

    char buf[1024];
    if (-1 == write(sock, message.c_str(), message.size()))
    {
        std::cout << __FILE__ << __LINE__ << "send to server error: " << strerror(errno) << std::endl;
        perror(nullptr); 
    }
    close(sock);
}