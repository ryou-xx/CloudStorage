#include <string>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ServerBackuplog.hpp"

using std::cout;
using std::endl;

const std::string filename = "./logfile.log";

void usage(std::string procgress)
{
    cout << "usage error: " << procgress << "port" << endl;
}

bool file_exist(const std::string &name)
{
    struct stat exist;
    return (stat(name.c_str(), &exist) == 0);
}

void backup_log(const std::string &message)
{
    FILE *fp = fopen(filename.c_str(), "ab");
    if (!fp)
    {
        perror("fopen error");
        assert(false);
    }
    int write_byte = fwrite(message.c_str(), 1, message.size(), fp);
    if (write_byte != message.size())
    {
        perror("fwrite error");
        assert(false);
    }
    fflush(fp);
    fclose(fp);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        usage(argv[0]);
        perror("usage error");
        exit(-1);
    }

    uint16_t port = atoi(argv[1]);
    std::unique_ptr<TcpServer> tcp = std::make_unique<TcpServer>(port, backup_log);

    tcp->init_service();
    tcp->start_service(); // 开始监听

    return 0;
}