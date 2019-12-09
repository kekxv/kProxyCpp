//
// Created by caesar on 2019/12/1.
//

#ifndef KPROXYCPP_KCLIENT_H
#define KPROXYCPP_KCLIENT_H

#include <netinet/in.h>
#include <logger.h>
#include <vector>
#include <map>
#include <HttpResponseCode.h>
#include <socket.h>

class kProxy;

class kClient {
private:
public:
private:
    static const char *TAG;
    static logger *_logger;
public:
    HttpResponseCode::ResponseCode response_code = HttpResponseCode::ResponseCode::OK;
    std::string method;
    std::string url_path;
    std::string http_version;
    std::string ContentType = "text/html;charset=UTF-8";
    std::vector<unsigned char> ResponseContent;
    std::map<std::string, std::string> header;
    std::map<std::string, std::string> response_header;
    std::vector<unsigned char> body_data;

    kClient(kProxy *parent, int fd);
    ~kClient();

    int run();

private:
    kProxy *parent = nullptr;
    int fd = 0;
    kekxv::socket *_socket = nullptr;


    void init_header(const char *data, unsigned long int size, bool is_split_n);

    void send_header();
    void send_body();

private:
    static std::string get_localtime();
};


#endif //KPROXYCPP_KCLIENT_H
