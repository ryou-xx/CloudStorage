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
            Config &cf_data = Config::GetConfigData();
            server_port_ = cf_data.GetServerPort();
            server_ip_ = cf_data.GetServerIP();
            download_prefix_ = cf_data.GetDownLoadPrefix();
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
            if(evhttp_bind_socket(httpd, "0.0.0.0", server_port_) != 0)
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
            if (httpd) evhttp_free(httpd);
            if (base) event_base_free(base);
            return true;
        }

    private:
        static void GenHandler(evhttp_request *req, void *args)
        {
            string path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));
            path = UrlDecode(path); // 解码
            mylog::GetLogger("asynclogger")->Info("get req, uri_path: %s", path.c_str());

            char *client_ip;
            uint16_t client_port;
            evhttp_connection_get_peer(evhttp_request_get_connection(req), &client_ip, &client_port);
            LoginManager::GetLoginManager().UpdateRegister();
            if (LoginManager::GetLoginManager().CheckLoggedIn(client_ip))
            {
                if (path.find("/download") != string::npos) Download(req, args);
                else if (path.find("/delete") != string::npos) Delete(req, args);
                else if (path == "/upload") Upload(req, args);
                else if (path == "/logOut") LogOut(req, args, client_ip);
                else if (path == "/")  ListShow(req, args);    
                else evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
            }
            else
            {
                if (path == "/login") 
                    LogIn(req, args);
                else
                    LoginPage(req, args);
            }
        }

        static void LogOut(evhttp_request *req, void *args, const char *client_ip)
        {
            LoginManager::GetLoginManager().LogOut(client_ip);
            mylog::GetLogger("asynclogger")->Info("IP: %s log out", client_ip);
            LoginPage(req, args);
        }

        static void LogIn(evhttp_request *req, void *args)
        {
            char *client_ip;
            uint16_t client_port;
            string buf(128, 0);
            
            evhttp_connection_get_peer(evhttp_request_get_connection(req), &client_ip, &client_port);

            evbuffer *input_buf = evhttp_request_get_input_buffer(req);
            size_t copied = evbuffer_copyout(input_buf, &buf[0], buf.size());
            size_t prefix_len = string("password=").size(); 
            if (copied == 0 || copied <= prefix_len || buf.substr(0,prefix_len) != "password=") 
            {
                mylog::GetLogger("asynclogger")->Info("login request format error");
                evhttp_send_reply(req, HTTP_BADREQUEST, "request format error",nullptr);
                return;
            }
            buf.resize(copied);
            buf = UrlDecode(buf);           
            if (Config::GetConfigData().GetPassword() != buf.substr(prefix_len))
            {
                mylog::GetLogger("asynclogger")->Info("wrong password, client IP: %s", client_ip);
                evhttp_send_reply(req, HTTP_BADREQUEST, "wrong password",nullptr);
                return;
            }

            LoginManager::GetLoginManager().Login(client_ip);
            mylog::GetLogger("asynclogger")->Info("IP: %s register", client_ip);
            evhttp_send_reply(req, HTTP_OK, "Ok", nullptr);
            // ListShow(req, args);
        }

        static void LoginPage(evhttp_request *req, void *args)
        {
            if (!FileUtil("./login.html").Exists())
            {
                mylog::GetLogger("asynclogger")->Error("login.html not exist!");
                evhttp_send_error(req, HTTP_INTERNAL, "server error");
                return;
            }

            int fd = open("login.html", O_RDONLY);
            if (fd == -1)
            {
                mylog::GetLogger("asynclogger")->Error("login.html open failed");
                evhttp_send_error(req, HTTP_INTERNAL, "server error");
                return;
            }

            evbuffer *output_buf = evhttp_request_get_output_buffer(req);
            evbuffer_add_file(output_buf, fd, 0, FileUtil("./login.html").FileSize());
            evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html;charset=utf-8");
            evhttp_send_reply(req, HTTP_OK, nullptr, nullptr);
            mylog::GetLogger("asynclogger")->Info("login page show");
        }

        static bool MergeChunksAndFinalize(string upload_id, string filename, string storage_type)
        {
            mylog::GetLogger("asynclogger")->Info("Starting merge for Upload-ID: %s", upload_id.c_str());

            string temp_upload_dir = Config::GetConfigData().GetTemporaryFileDir() + upload_id;
            
            // 扫描临时目录中的所有分片文件
            std::vector<fs::path> chunks;
            try {
                for (const auto& entry : fs::directory_iterator(temp_upload_dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".part") {
                        chunks.push_back(entry.path());
                    }
                }
            } catch (const std::exception& e) {
                mylog::GetLogger("asynclogger")->Error("Failed to scan chunk directory %s: %s", 
                    temp_upload_dir.c_str(), e.what());
                FileUtil(temp_upload_dir).RemoveDirectory();
                return false;
            }

            // 对分片进行数字排序
            std::sort(chunks.begin(), chunks.end(), [](const fs::path& a, const fs::path& b) {
                return std::stoi(a.stem().string()) < std::stoi(b.stem().string());
            });

            // 确定最终存储路径
            string final_storage_dir = (storage_type == "low") ? 
                                       Config::GetConfigData().GetLowStorageDir() :
                                       Config::GetConfigData().GetDeepStorageDir();
            FileUtil(final_storage_dir).CreateDirectory();
            string final_storage_path = final_storage_dir + filename;

            // 合并所有分片到最终文件
            // 对于压缩存储，流式压缩到目标文件中

            std::ofstream final_file(final_storage_path, std::ios::binary);
            if (!final_file.is_open())
            {
                mylog::GetLogger("asynclogger")->Error("Failed to open final file for writing: %s", 
                    final_storage_path.c_str());
                FileUtil(temp_upload_dir).RemoveDirectory();
                return false;
            }

            // 如果是普通存储，直接合并文件并存放到对应的路径中
            if (storage_type == "low")
            {
                for (const auto& chunk : chunks)
                {
                    std::ifstream chunk_file(chunk, std::ios::binary);
                    if (!chunk_file.is_open())
                    {
                        mylog::GetLogger("asynclogger")->Error("Failed to open chunk file for reading: %s", chunk.c_str());
                        FileUtil(temp_upload_dir).RemoveDirectory();
                        return false;
                    }
                    final_file << chunk_file.rdbuf();
                    chunk_file.close();
                }
                final_file.close();
            } 
            else // 如果是压缩存储，使用lzib库的流式压缩
            {
                if (FileUtil(final_storage_path).Compress(chunks) == false)
                {
                    mylog::GetLogger("asynclogger")->Error("Compress error");
                    FileUtil(temp_upload_dir).RemoveDirectory();
                    return false;
                }
            }

            // 清理临时文件
            FileUtil(temp_upload_dir).RemoveDirectory();

            // 更新文件信息
            StorageInfo info;
            if (info.NewStorageInfo(final_storage_path))
            {
                DataManager::GetDataManager().Insert(info);
            }

            mylog::GetLogger("asynclogger")->Info("File chunks merged successfully to %s", final_storage_path.c_str());
            mylog::GetLogger("asynclogger")->Info("Upload file %s succussfully",filename.c_str());
            
            return true;
        } 
        
        static void Upload(evhttp_request *req, void *args)
        {
            // 若客户端发来的请求中包含“low_storage"，则说明请求中存在文件数据，且需要普通存储
            // 若包含“deep_storage”，则压缩后存储

            /* 获取http头中携带的信息 */
            const char* filename_c = evhttp_find_header(req->input_headers, "FileName");         
            const char* storage_type_c = evhttp_find_header(req->input_headers, "StorageType");
            const char* upload_id_c = evhttp_find_header(req->input_headers,"Upload-Id");
            const char* chunk_index_c = evhttp_find_header(req->input_headers,"Chunk-Index");
            const char* total_chunks_c = evhttp_find_header(req->input_headers,"Total-Chunks");

            if (!filename_c || !storage_type_c || !upload_id_c || !chunk_index_c || !total_chunks_c)
            {
                mylog::GetLogger("asynclogger")->Error("Chunk upload missing required headers");
                evhttp_send_reply(req, HTTP_BADREQUEST, "Missin chunk upload headers", nullptr);
                return;
            }

            string filename(filename_c);
            filename = base64_decode(filename); // 解码文件名
            string storage_type(storage_type_c);
            string upload_id(upload_id_c);
            upload_id += "-" + filename;
            int chunk_index = atoi(chunk_index_c);
            int total_chunks = atoi(total_chunks_c);

            if (chunk_index == 0)
                mylog::GetLogger("asynclogger")->Info("Upload file %s start", filename.c_str());

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
                evhttp_send_reply(req, HTTP_BADREQUEST, "file empty",nullptr);
                mylog::GetLogger("asynclogger")->Info("request body is empty");
                return;
            }
            // mylog::GetLogger("asynclogger")->Info("evbuffer_get_length: %u", len);

            // 创建临时存储文件夹存储分片文件
            string temp_upload_dir = Config::GetConfigData().GetTemporaryFileDir() + upload_id;
            if (!FileUtil(temp_upload_dir).CreateDirectory())
            {
                mylog::GetLogger("asynclogger")->Error("Failed to create temporary directory: %s",temp_upload_dir);
                evhttp_send_reply(req, HTTP_INTERNAL, "Server error", nullptr);
                return;
            }

            // 将数据写入分片文件
            string chunk_path = temp_upload_dir + "/" + std::to_string(chunk_index) + ".part";
            std::ofstream chunk_file(chunk_path, std::ios::binary);
            if (!chunk_file.is_open())
            {
                mylog::GetLogger("asynclogger")->Error("Failed to open chunk file: %s",chunk_path);
                evhttp_send_reply(req, HTTP_INTERNAL, "Server error", nullptr);
                return;
            }

            char buffer[8192]; // 8KB 缓冲区
            int n_read = 0;
            // 循环地从evbuffer中移除数据并写入文件，直到缓冲区为空
            while ((n_read = evbuffer_remove(buf, buffer, sizeof(buffer))) > 0) {
                chunk_file.write(buffer, n_read);
            }
            chunk_file.close();

#ifdef DEBUG_LOG
            mylog::GetLogger("asynclogger")->Info("Saved chunk %d/%d for %s to %s", 
                chunk_index + 1, total_chunks, upload_id.c_str(), chunk_path.c_str());
#endif
            evhttp_send_reply(req, HTTP_OK, "Success", nullptr);

            if (chunk_index == total_chunks - 1)
            {
                // 创建文件合并线程，避免服务器阻塞
                std::thread(MergeChunksAndFinalize, upload_id, filename, storage_type).detach();
            }
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
                   << (storage_type == "deep" ? "压缩存储" : "普通存储")
                   << "</span>"
                   << "<span>" << FormatSize(file.fsize_) << "</span>"
                   << "<span>" << std::ctime(&file.mtime_) << "</span>"
                   << "</div>"
                   << "<div class='file-actions'>"
                   << "<button onclick=\"DeleteFile('" << "/delete/" <<file_name << "')\"> 删除</button>"
                   << "<button onclick=\"window.location='" << file.url_ << "'\">下载</button>"
                   << "</div>"
                   << "</div>";
            }
            ss << "</div=>";
            return ss.str();
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
            url_path = UrlDecode(url_path);
            StorageInfo file_info;
            if(!DataManager::GetDataManager().GetOneByURL(url_path, &file_info))
            {
                evhttp_send_error(req, HTTP_NOTFOUND, "No such file");
                return;
            }
            mylog::GetLogger("asynclogger")->Info("requeset url_path: %s", url_path.c_str());

            string download_path = file_info.storage_path_;
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

            // 确认是否需要断点续传 //  
            bool retrans = false;
            // If-Range 携带ETag
            auto if_range = evhttp_find_header(evhttp_request_get_input_headers(req), "If-Range");
            if (if_range)
            {
                if (string(if_range) == GetETag(file_info))
                {
                    retrans = true;
                    mylog::GetLogger("asynclogger")->Info("%s need breakpoint continuous transmission", download_path.c_str());
                }
            }

            evbuffer *output_buf = evhttp_request_get_output_buffer(req);
            int fd = open(download_path.c_str(), O_RDONLY);
            if (fd == -1)
            {
                mylog::GetLogger("asynclogger")->Error("open file %s error: %s", download_path.c_str(), strerror(errno));
                evhttp_send_error(req, HTTP_INTERNAL, strerror(errno));
                return;
            }

            // 设置通用响应头
            evkeyvalq *output_headers =  evhttp_request_get_output_headers(req);
            evhttp_add_header(output_headers, "Accept-Ranges", "bytes");
            evhttp_add_header(output_headers, "ETag", GetETag(file_info).c_str());
            evhttp_add_header(output_headers, "Content-Type", "application/octet-stream");

            if (retrans) //断点续传
            {
                auto input_header = evhttp_request_get_input_headers(req);
                const char* range_value = evhttp_find_header(input_header,"Range");
                if (range_value == nullptr)
                {
                    retrans = false; // 退化为全文件传输
                }
                else
                {
                    const string range_str(range_value);
                    std::smatch match;
                    std::regex str_regex("byte=(\\d+)-(\\d*)"); // \d+表示出现至少一个数字 \d*表示出现0个或多个数字
                    if (std::regex_search(range_str, match, str_regex) && !match[1].str().empty()) // 找到匹配内容且捕获组1的内容不为空
                    {
                        long long start_byte = std::stoll(match[1].str());
                        long long end_byte = -1;
                        if (match.size() > 2 && !match[2].str().empty()) end_byte = std::stoll(match[2].str());

                        size_t total_size = fu.FileSize();
                        if (end_byte == -1 || end_byte >= total_size) end_byte = total_size - 1;

                        size_t request_len = end_byte - start_byte + 1;
                        if (start_byte >= total_size || request_len <= 0)
                        {
                            evhttp_add_header(output_headers, "Content-Range", ("bytes */" + std::to_string(total_size)).c_str());
                            evhttp_send_reply(req, 416, "Range Not Saticfiable", nullptr);
                            close(fd);
                            return;
                        }
                        string cr_str = "bytes " + std::to_string(start_byte) + \
                                        "-" + std::to_string(end_byte) + "/" +  std::to_string(total_size);
                        
                        evhttp_add_header(output_headers, "Content-Range", cr_str.c_str());
                        if (-1 == evbuffer_add_file(output_buf, fd, start_byte, request_len))
                        {
                            mylog::GetLogger("asynclogger")->Error("evbuffer_add_file partial content: %s error", 
                                download_path.c_str(), strerror(errno));
                            evhttp_send_error(req, HTTP_INTERNAL, "evbuffer_add_file partial content error");
                            close(fd);
                            return;
                        }

                        evhttp_send_reply(req, 206, "Partial content", nullptr);
                        mylog::GetLogger("asynclogger")->Info("send file with breakpoint continuous transmission");
                    }
                    else
                    {
                        retrans = false;
                    }             
                }            
            }

            // 如果不需要断点续传，直接传输整个文件
            if (!retrans)
            {
                if (-1 == evbuffer_add_file(output_buf, fd, 0, fu.FileSize()))
                {
                    mylog::GetLogger("asynclogger")->Error("evbuffer_add_file %s error: %s", 
                        download_path.c_str(), strerror(errno));
                    evhttp_send_error(req, HTTP_INTERNAL, "evbuffer_add_file failed");
                }
                evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
                mylog::GetLogger("asynclogger")->Info("send file without breakpoint continuous transmission");
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

        static void Delete(evhttp_request *req, void *args)
        {
            mylog::GetLogger("asynclogger")->Info("Delete start");
            string delete_path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));
            delete_path = UrlDecode(delete_path);
            auto pos = delete_path.find_last_of("/");
            if (pos == string::npos)
            {
                mylog::GetLogger("asynclogger")->Info("Invalid URL format");
                evhttp_send_reply(req, HTTP_BADREQUEST, "Invalid URL format", nullptr);
                return;
            }

            string url_path = Config::GetConfigData().GetDownLoadPrefix() + delete_path.substr(pos + 1);

            StorageInfo file_info;
            if (!DataManager::GetDataManager().GetOneByURL(url_path,&file_info))
            {
                // 文件不存在，直接返回成功
                DataManager::GetDataManager().Update();
                evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
                return;
            }

            if (!FileUtil(file_info.storage_path_).Exists())
            {
                // 文件不存在，直接返回成功
                DataManager::GetDataManager().Update();
                evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
                return;
            }

            if (remove(file_info.storage_path_.c_str()) == -1)
            {
                mylog::GetLogger("asynclogger")->Info("delete file %s failed: %s", file_info.storage_path_.c_str(), strerror(errno));
                evhttp_send_reply(req, HTTP_INTERNAL, "Delete failed", nullptr);
            }

            DataManager::GetDataManager().Remove(file_info.url_);
            evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
            mylog::GetLogger("asynclogger")->Info("delete file %s successfully", file_info.storage_path_.c_str());
        }
        
        static void ListShow(evhttp_request *req, void *args)
        {
            mylog::GetLogger("asynclogger")->Info("ListShow start");

            std::vector<StorageInfo> files_info;
            DataManager::GetDataManager().Update();
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