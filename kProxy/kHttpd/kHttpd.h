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

    /**
     * 是否是 WebSocket
     */
    bool isWebSocket = false;

    ~kHttpd();

    int listen(int listen_count = 20, short port = 8080, const char *ip = "0.0.0.0");


    using url_cb = std::function<int(void *kClient, std::vector<unsigned char> data, std::string url_path,
                                     std::string method,int type,void *arg)>;
    using gen_cb = std::function<int(void *kClient, std::vector<unsigned char> data, std::string url_path,
                                     std::string method,
                                     std::string host,int type,void *arg)>;

    void set_cb(url_cb task,
                const std::string &url_path,
                const std::string &method = "",
                const std::string &host = "");

    void set_gencb(gen_cb task);

private:
    int fd = -1;
    thread_pool *threadPool = nullptr;
    bool isRunning = true;
    std::string web_root_path;

    gen_cb gen_cb_task = nullptr;
    std::map<std::string, url_cb> url_cb_tasks;

private:
    // 同步
    std::mutex m_lock;
    // 条件阻塞
    std::condition_variable cv_task;
    // 是否关闭提交
    std::atomic<bool> stoped{false};
    std::vector<int> socket_fd_list;

    void add_poll_check(int new_fd);

    int check_host_path(class kHttpdClient *_kHttpdClient, const std::string& Host, const std::string& method, const std::string& url_path);

    int check_host_path(class kWebSocketClient *_kWebSocketClient, int type, const std::vector<unsigned char>& data);

    friend class kHttpdClient;

    friend class kWebSocketClient;
};


#endif //KPROXYCPP_KPROXY_H
