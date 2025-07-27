#pragma once
#include "DataManager.hpp"

#include <sys/queue.h>
#include <event.h>

#include <evhttp.h>
#include <event2/http.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <regex> // 正则表达式
#include <queue>

#include "base64.h"

namespace storage{
    class Server{
    public:
        Server()
        {
#ifdef DEBUG_LOG
            mylog::GetLogger("asynclogger")->Debug("Server Construct start");
#endif
            server_port_ = Config::GetConfigData().GetServerPort();
            server_ip_ = Config::GetConfigData().GetServerIP();
            download_prefix_ = Config::GetConfigData().GetDownLoadPrefix();
#ifdef DEBUG_LOG
            mylog::GetLogger("asynclogger")->Debug("Server Construct end");
#endif
        }

        bool RunModule()
        {
            event_base *base = event_base_new();
            if (base == nullptr)
            {
                mylog::GetLogger("asynclogger")->Fatal("event_base_new error");
                return false;
            }

            evhttp *httpd = evhttp_new(base);
            if(evhttp_bind_socket(httpd, server_ip_.c_str(), server_port_) != 0)
            {
                mylog::GetLogger("asynclogger")->Fatal("evhttp_bind_socket failed");
                return false;
            }

            // 设置全局回调函数对所有的URL响应
            evhttp_set_gencb(httpd, GenHandler, nullptr);

            if (base)
            {
#ifdef DEBUG_LOG
                mylog::GetLogger("asynclogger")->Debug("event_base_dispatch");
#endif
                if (-1 == event_base_dispatch(base))  
                {
                    mylog::GetLogger("asynclogger")->Debug("event_base_dispatch error");
                }              
            }
            if (base) event_base_free(base);
            if (httpd) evhttp_free(httpd);
            return true;
        }

    private:
        static void GenHandler(evhttp_request *req, void *args)
        {
            string path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));
            path = UrlDecode(path); // 解码
            mylog::GetLogger("asynclogger")->Info("get req, uri_path: %s", path.c_str());

            if (path.find("/download") != string::npos) Download(req, args);
            else if (path == "/upload") Upload(req, args);
            else if (path == "/") ListShow(req, args);
            else evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        }

        static void Upload(evhttp_request *req, void *args)
        {
            mylog::GetLogger("asynclogger")->Info("Upload start");

            // 若客户端发来的请求中包含“low_storage"，则说明请求中存在文件数据，且需要普通存储
            // 若包含“deep_storage”，则压缩后存储

            // 获取请求体内容
            evbuffer *buf = evhttp_request_get_input_buffer(req);
            if (buf == nullptr)
            {
                mylog::GetLogger("asynclogger")->Info("request buffer is empty");
                return;
            }

            size_t len = evbuffer_get_length(buf); // 获取请求体大小  
            if (0 == len)
            {
                evhttp_send_error(req, HTTP_BADREQUEST, "file empty");
                mylog::GetLogger("asynclogger")->Info("request body is empty");
                return;
            }
            mylog::GetLogger("asynclogger")->Info("evbuffer_get_length: %u", len);

            string content(len,0);
            if (-1 == evbuffer_copyout(buf, &(content[0]), len))
            {
                mylog::GetLogger("asynclogger")->Info("evbuffer_copyout error");
                evhttp_send_error(req, HTTP_INTERNAL, nullptr);
            }

            string filename = evhttp_find_header(req->input_headers, "FileName");
            filename = base64_decode(filename); // 解码文件名

            string storage_type = evhttp_find_header(req->input_headers, "StorageType");

            string storage_path;
            if (storage_type == "low") 
                storage_path = Config::GetConfigData().GetLowStorageDirt();
            else if (storage_type == "deep")
                storage_path = Config::GetConfigData().GetDeepStorageDir();
            else
            {
                mylog::GetLogger("asynclogger")->Info("StyrageType invalid");
                evhttp_send_error(req, HTTP_BADREQUEST, "storage type invalid");
                return;
            }

        }
        static void Download(evhttp_request *req, void *args){}
        static void ListShow(evhttp_request *req, void *args){}
    private:
        uint16_t server_port_;
        string server_ip_;
        string download_prefix_;
    }; // class server
}