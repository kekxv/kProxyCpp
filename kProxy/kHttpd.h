//
// Created by caesar on 2019/11/26.
//

#ifndef KPROXYCPP_KPROXY_H
#define KPROXYCPP_KPROXY_H

#ifdef WIN32
#ifdef _kProxy_HEADER_
#define _kProxy_HEADER_Export  __declspec(dllexport)
#else
#define _kProxy_HEADER_Export  __declspec(dllimport)
#endif
#else
#define _kProxy_HEADER_Export
#endif

#include <logger.h>
#include <event.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include <event2/listener.h>
#include <thread_pool.h>

class _kProxy_HEADER_Export kHttpd {
public:
private:
    static const char *TAG;
    static logger *_logger;


public:
    static void Init();

    explicit kHttpd(int max_thread = 20);

    explicit kHttpd(const char *web_root_path, int max_thread = 20);

    ~kHttpd();

    int listen(int listen_count = 20, short port = 8080, const char *ip = "0.0.0.0");

private:
    int fd = -1;
    thread_pool *threadPool = nullptr;
    bool isRunning = true;
    std::string web_root_path;

private:
    // 同步
    std::mutex m_lock;
    // 条件阻塞
    std::condition_variable cv_task;
    // 是否关闭提交
    std::atomic<bool> stoped{false};
    std::vector<int> socket_fd_list;

    void add_poll_check(int new_fd);

    bool check_host_path(class kHttpdClient *_kHttpdClient, std::string Host, std::string method, std::string url_path);

    friend class kHttpdClient;
};


#endif //KPROXYCPP_KPROXY_H
