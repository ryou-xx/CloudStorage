#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../Util.hpp"


void start_backup(const std::string &message)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << __FILE__ << " " << __LINE__ << " socket error: " << strerror(errno) << std::endl;
    }

    sockaddr_in serv;
    bzero(&serv, sizeof(sockaddr_in));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(mylog::Util::JsonData::GetJsonData().backup_port);
    inet_aton(mylog::Util::JsonData::GetJsonData().backup_addr.c_str(), &serv.sin_addr);

    int cnt = 5;
    while (-1 == connect(sock, (sockaddr*)&serv, sizeof(serv)))
    {
        std::cout << "正在尝试重新连接，重连次数还有：" << cnt << std::endl;
        if (cnt-- <= 0)
        {
            std::cerr << __FILE__ << " " << __LINE__ << " connect error: " << strerror(errno) << std::endl;

            return;
        }
    }

    char buf[1024];
    if (-1 == write(sock, message.c_str(), message.size()))
    {
        std::cerr << __FILE__ << " " << __LINE__ << " send to server error: " << strerror(errno) << std::endl;
    }
    close(sock);
}