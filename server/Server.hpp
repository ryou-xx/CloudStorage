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
                storage_path = Config::GetConfigData().GetLowStorageDir();
            else if (storage_type == "deep")
                storage_path = Config::GetConfigData().GetDeepStorageDir();
            else
            {
                mylog::GetLogger("asynclogger")->Info("StyrageType invalid");
                evhttp_send_error(req, HTTP_BADREQUEST, "storage type invalid");
                return;
            }

            FileUtil dir(storage_path);
            dir.CreateDirectory();      // 若路径不存在，则创建路径

            storage_path += filename;
#ifdef DEBUG
            mylog::GetLogger("asynclogger")->Info("storage_path: %s", storage_path.c_str());
#endif
            FileUtil fu(storage_path);
            if (storage_path.find("low_storage/") != string::npos)
            {
                if (!fu.SetContent(content.data(), len))
                {
                    mylog::GetLogger("asynclogger")->Error("low storage write file failed");
                    evhttp_send_error(req, HTTP_INTERNAL, "server error");
                    return;
                }
                else
                {
                    mylog::GetLogger("asynclogger")->Info("low_storage success");
                }
            }
            else
            {
                if (!fu.Compress(content, Config::GetConfigData().GetBundleFormat()))
                {
                    mylog::GetLogger("asynclogger")->Error("deep storage compress failed");
                    evhttp_send_error(req, HTTP_INTERNAL, "server error");
                    return;
                }
                else
                {
                    mylog::GetLogger("asynclogger")->Info("deep_storage success");
                }
            }

            StorageInfo info;
            info.NewStorageInfo(storage_path);
            DataManager::GetDataManager().Insert(info);

            evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
            mylog::GetLogger("asynclogger")->Info("upload file successfully");
        }


        static std::string GenerateModernFileList(const std::vector<StorageInfo> &files_info)
        {
            std::stringstream ss;
            ss << "<div class = \"file-list\"><h3>云盘文件</h3>";

            for (const auto file : files_info)
            {
                string file_name = FileUtil(file.storage_path_).FileName();

                string storage_type = "low";
                if (file.storage_path_.find("deep_storage/") != string::npos) storage_type = "deep";

                ss << "<div class='file-item'>"
                   << "<div class='file-info'>"
                   << "<span>📄" << file_name << "</span>"
                   << "<span class='file-type'>"
                   << (storage_type == "deep" ? "深度存储" : "普通存储")
                   << "</span>"
                   << "<span>" << FormatSize(file.fsize_) << "</span>"
                   << "<span>" << std::ctime(&file.mtime_) << "</span>"
                   << "</div>"
                   << "<button onclick=\"window.location='" << file.url_ << "'\">⬇️ 下载</button>"
                   << "</div>";
            }
        }

        static string FormatSize(size_t bytes)
        {
            const char *units[] = {"B", "KB", "MB", "GB"};
            int unit_index = 0;
            double size = bytes;

            while (size >= 1024 && unit_index < 3)
            {
                size /= 1024;
                unit_index++;
            }

            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
            return ss.str();
        }
        static void Download(evhttp_request *req, void *args)
        {
            mylog::GetLogger("asynclogger")->Info("Download start");
            string url_path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));
            StorageInfo file_info;
            if(!DataManager::GetDataManager().GetOneByURL(url_path, &file_info))
            {
                evhttp_send_error(req, HTTP_NOTFOUND, "No such file");
                return;
            }
            mylog::GetLogger("asynclogger")->Info("requeset url_path: %s", url_path.c_str());

            string download_path;
            if (file_info.storage_path_.find(Config::GetConfigData().GetDeepStorageDir()) != string::npos)
            {
                FileUtil fu(file_info.storage_path_);
                download_path = Config::GetConfigData().GetTemporaryFileDir() + fu.FileName();
                // 若临时文件文件夹不存在，需要先创建
                FileUtil(Config::GetConfigData().GetTemporaryFileDir()).CreateDirectory();
                fu.UnCompress(download_path);
            }

            FileUtil fu(download_path);
            if (!fu.Exists() && file_info.storage_path_.find("deep_storage/") != string::npos)
            {
                evhttp_send_error(req, HTTP_INTERNAL, "uncompress error");
            }
            else if (!fu.Exists() && file_info.storage_path_.find("low_storage/") != string::npos)
            {
                evhttp_send_error(req, HTTP_BADREQUEST, "file not exist");
            }

            bool retrans = false;
            string old_etag;
            auto if_range = evhttp_find_header(evhttp_request_get_input_headers(req), "If-Range");
            if (if_range)
            {
                old_etag = if_range;

                if (old_etag == GetETag(file_info))
                {
                    retrans = true;
                    mylog::GetLogger("asynclogger")->Info("%s need breakpoint continuous transmission", download_path.c_str());
                }
            }

            if (!fu.Exists())
            {
                mylog::GetLogger("asynclogger")->Info("%s not exists", download_path.c_str());
                download_path += "not exists";
                evhttp_send_error(req, HTTP_NOTFOUND, download_path.c_str());
                return;
            }

            evbuffer *output_buf = evhttp_request_get_output_buffer(req);
            int fd = open(download_path.c_str(), O_RDONLY);
            if (fd == -1)
            {
                mylog::GetLogger("asynclogger")->Error("open file %s error: %s", download_path.c_str(), strerror(errno));
                evhttp_send_error(req, HTTP_INTERNAL, strerror(errno));
                return;
            }

            if (-1 == evbuffer_add_file(output_buf, fd, 0, fu.FileSize()))
            {
                mylog::GetLogger("asynclogger")->Error("evbuffer_add_file %s error: %s", download_path.c_str(), strerror(errno));
                evhttp_send_error(req, HTTP_INTERNAL, "evbuffer_add_file failed");
            }

            evkeyvalq *output_headers =  evhttp_request_get_output_headers(req);
            evhttp_add_header(output_headers, "Accept-Ranges", "bytes");
            evhttp_add_header(output_headers, "ETag", GetETag(file_info).c_str());
            evhttp_add_header(output_headers, "Content-Type", "application/octet-stream");

            if (retrans == false)
            {
                evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
                mylog::GetLogger("asynclogger")->Info("send file without breakpoint continuous transmission");
            }
            else
            {
                evhttp_send_reply(req, 206, "Success", nullptr);
                mylog::GetLogger("asynclogger")->Info("send file with breakpoint continuous transmission");
            }
            if (download_path != file_info.storage_path_) remove(download_path.c_str()); // 删除临时文件
        }
        
        static std::string GetETag(const StorageInfo &info)
        {
            string etag = FileUtil(info.storage_path_).FileName()
                          + "-" + std::to_string(info.fsize_)
                          + "-" + std::to_string(info.mtime_);
            return etag;
        }
        
        static void ListShow(evhttp_request *req, void *args)
        {
            mylog::GetLogger("asynclogger")->Info("ListShow start");

            std::vector<StorageInfo> files_info;
            DataManager::GetDataManager().GetAll(files_info);

            std::ifstream templateFile("index.html");
            string tpContent(
                (std::istreambuf_iterator<char>(templateFile)), 
                std::istreambuf_iterator<char>());

            tpContent = std::regex_replace(tpContent, 
                                            std::regex("\\{\\{FILE_LIST\\}\\}"),
                                            GenerateModernFileList(files_info));
            tpContent = std::regex_replace(tpContent, 
                                            std::regex("\\{\\{BACKEND_URL\\}\\}"),
                                            "http://" + Config::GetConfigData().GetServerIP() + ":" + \
                                            std::to_string(Config::GetConfigData().GetServerPort()));                                
            
            evbuffer_add(evhttp_request_get_output_buffer(req), tpContent.data(), tpContent.size());
            evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html;charset=utf-8");
            evhttp_send_reply(req, HTTP_OK, nullptr, nullptr);
            mylog::GetLogger("asynclogger")->Info("LishShow completed");
        }
    private:
        uint16_t server_port_;
        string server_ip_;
        string download_prefix_;
    }; // class server
}