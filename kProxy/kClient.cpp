//
// Created by caesar on 2019/12/1.
//

#include "kClient.h"
#include "UTF8Url.h"
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>

using namespace std;

#ifdef __FILENAME__
const char *kClient::TAG = __FILENAME__;
#else
const char *kClient::TAG = "kClient";
#endif

logger *kClient::_logger = logger::instance();

kClient::kClient(kProxy *parent, int fd) {
    if (_logger->min_level != logger::log_rank_DEBUG) {
        _logger->min_level = logger::log_rank_DEBUG;
#ifdef _LOGGER_USE_THREAD_POOL_
        _logger->wait_show = false;
#endif
        _logger->console_show = true;
    }

    struct sockaddr_in remote_addr{};
    socklen_t sin_size = 0;
    getpeername(fd, (struct sockaddr *) &remote_addr, &sin_size);
    this->parent = parent;
    this->fd = fd;
    this->_socket = new kekxv::socket(fd);
}

kClient::~kClient() {
    delete this->_socket;
}


int kClient::run() {
    vector<unsigned char> data;
    unsigned long int split_index = 0;
    bool is_split_n = true;
    /********* 读取数据内容 *********/
    do {
        vector<unsigned char> buffer;
        if (_socket->check_read_count(3 * 1000) <= 0) {
            break;
        }
        auto size = this->_socket->read(buffer);
        if (size == 0) {
            if (_socket->check_read_count(3 * 1000) <= 0) {
                break;
            } else {
                size = this->_socket->read(buffer);
            }
        }
        if (0 == size) {//说明socket关闭
            _logger->w(TAG, __LINE__, "read size is %ld for socket: %d", size, fd);
            shutdown(fd, SHUT_RDWR);
            close(fd);
            return 0;
        } else if (0 > size && (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)) {
            _logger->w(TAG, __LINE__, "read size is %ld for socket: %d is errno:", size, fd, errno);
            shutdown(fd, SHUT_RDWR);
            close(fd);
            return 0;
        }
        if (!data.empty()) {
            if (data.size() >= 2 && data[data.size() - 2] == '\r' && data[data.size() - 1] == '\n') {
                if (buffer[0 + 0] == '\r' && buffer[0 + 1] == '\n') {
                    split_index = data.size() + 2;
                    is_split_n = false;
                }
            } else if (data[data.size() - 1] == '\n') {
                if (buffer[0] == '\n') {
                    split_index = data.size() + 1;
                    is_split_n = true;
                }
            }
        }
        if (split_index == 0) {
            for (ssize_t i = 0; i < size; i++) {
                if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
                    if (buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
                        split_index = data.size() + i + 4;
                        is_split_n = false;
                    }
                } else if (buffer[i] == '\n') {
                    if (buffer[i + 1] == '\n') {
                        split_index = data.size() + i + 2;
                        is_split_n = true;
                    }
                }
            }
        }
        data.insert(data.end(), &buffer[0], &buffer[size]);

    } while (split_index == 0);
    /********* 初始化http头 *********/
    init_header((const char *) data.data(), split_index, is_split_n);
    ssize_t ContentLength = 0;
    if (header.find("Content-Length") != header.end()) {
        ContentLength = stoll(header["Content-Length"]);
    } else if (header.find("content-length") != header.end()) {
        ContentLength = stoll(header["content-length"]);
    }
    /********* 读取body数据内容 *********/
    body_data.insert(body_data.end(), data.begin() + split_index, data.end());
    if (ContentLength > 0) {
        while (body_data.size() < ContentLength) {
            vector<unsigned char> buffer;
            if (_socket->check_read_count(10) <= 0) {
                break;
            }
            auto size = this->_socket->read(buffer);
            if (0 == size) {//说明socket关闭
                _logger->w(TAG, __LINE__, "read size is %ld for socket: %d", size, fd);
                shutdown(fd, SHUT_RDWR);
                close(fd);
                fd = 0;
                return fd;
                break;
            } else if (0 > size && (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)) {
                _logger->w(TAG, __LINE__, "read size is %ld for socket: %d is errno:", size, fd, errno);
                shutdown(fd, SHUT_RDWR);
                close(fd);
                fd = 0;
                return fd;
                break;
            }
            body_data.insert(body_data.end(), &buffer[0], &buffer[size]);
        };
    }

    /********* 输出body内容 *********/
    if (!body_data.empty()) {
        string post_data = (char *) body_data.data();
        _logger->d(TAG, __LINE__, "body:\n%s\n", (post_data).c_str());
        _logger->d(TAG, __LINE__, "body:\n%s\n", UTF8Url::Decode(post_data).c_str());
    }

    this->response_code = HttpResponseCode::ResponseCode::NotFound;
    string body = "未找到页面:" + url_path;
    this->ResponseContent.insert(ResponseContent.begin(), &body.data()[0], &body.data()[body.size()]);


    /********* 回复http 头 *********/
    send_header();
    /********* 回复http body内容 *********/
    send_body();
    return fd;
}

string kClient::get_localtime() {
    char localtm[512] = {0};
    time_t now; //实例化time_t结构
    struct tm *timenow; //实例化tm结构指针
    time(&now);     //time函数读取现在的时间(国际标准时间非北京时间)，然后传值给now

    timenow = localtime(&now);

    char daytime[100];
    asctime_r(timenow, daytime);

    char year[20] = {0}, week[20] = {0}, day[20] = {0}, mon[20] = {0}, _time[20] = {0};
    sscanf(daytime, "%s %s %s %s %s", week, mon, day, _time, year);
    sprintf(localtm, "%s, %s %s %s %s GMT", week, day, mon, year, _time);
    return localtm;
}

void kClient::init_header(const char *data, unsigned long int size, bool is_split_n) {
    unsigned long int offset = 0;
    int space_index = 0;
    for (; offset < size; offset++) {
        if (data[offset] == '\r' && data[offset + 1] == '\n') {
            offset += 2;
            break;
        } else if (data[offset] == '\n') {
            offset++;
            break;
        } else if (data[offset] == ' ') {
            space_index++;
        }
        switch (space_index) {
            case 0:
                if (!method.empty() || data[offset] != ' ')
                    method.push_back(data[offset]);
                break;
            case 1:
                if (!url_path.empty() || data[offset] != ' ')
                    url_path.push_back(data[offset]);
                break;
            default:
                if (!http_version.empty() || data[offset] != ' ')
                    http_version.push_back(data[offset]);
                break;
        }
    }
    if (method.empty())method = "GET";
    if (url_path.empty())url_path = "/";
    if (http_version.empty())http_version = "HTTP/1.0";
    _logger->d(TAG, __LINE__, "%s %s %s", method.c_str(), url_path.c_str(), http_version.c_str());

    space_index = 0;
    string key, value;
    for (; offset < size; offset++) {
        if (data[offset] == '\r' && data[offset + 1] == '\n') {
            offset += 1;
            space_index = 0;
            if (!key.empty() && !value.empty())
                header[key] = value;
            key = "";
            value = "";
            continue;
        } else if (data[offset] == '\n') {
            space_index = 0;
            if (!key.empty() && !value.empty())
                header[key] = value;
            key = "";
            value = "";
            continue;
        } else if (data[offset] == ':') {
            space_index++;
            if (data[offset + 1] == ' ') {
                offset++;
            }
        }
        if (space_index == 0) {
            key.push_back(data[offset]);
        } else {
            value.push_back(data[offset]);
        }
    }

    _logger->d(TAG, __LINE__, "header size:%ld", header.size());
    for (auto &item : header) {
        _logger->d(TAG, __LINE__, "%s : %s", item.first.c_str(), item.second.c_str());
    }

}

void kClient::send_header() {
    string line_one = http_version + " " + to_string(response_code) + " " +
                      HttpResponseCode::get_response_code_string(response_code);
    _logger->d(TAG, __LINE__, "%s", line_one.c_str());
    line_one.push_back('\n');
    this->_socket->send(line_one);
    response_header["Content-Type"] = ContentType;
    response_header["Date"] = get_localtime();
    response_header["Content-Length"] = to_string(ResponseContent.size());
    for (auto &item : response_header) {
        string line = item.first + ": " + item.second;
        _logger->d(TAG, __LINE__, "%s", line.c_str());
        line.push_back('\n');
        this->_socket->send(line);
    }
    this->_socket->send("\n");
}

void kClient::send_body() {
    auto len = ResponseContent.size();
    ssize_t offset = 0;
    while (offset < len) {
        auto ret = this->_socket->send(ResponseContent.data(), offset, len - offset);
        if (0 >= ret)break;
        offset += ret;
    }
}
