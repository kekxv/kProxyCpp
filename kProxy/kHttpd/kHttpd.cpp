//
// Created by caesar on 2019/11/26.
//

#include "kHttpd.h"
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>
#else

#include <arpa/inet.h>
#include <fcntl.h>

#endif

#include <kHttpdClient.h>
#include <kWebSocketClient.h>
#include <socket.h>

using namespace std;
using namespace kekxv;

#ifdef __FILENAME__
const char *kHttpd::TAG = __FILENAME__;
#else
const char *kHttpd::TAG = "kHttpd";
#endif

logger *kHttpd::_logger = logger::instance();;

void kHttpd::Init() {
    _logger->min_level = logger::log_rank_DEBUG;
#ifdef _LOGGER_USE_THREAD_POOL_
    _logger->wait_show = false;
#endif
    _logger->console_show = true;

#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    (void)WSAStartup(wVersionRequested, &wsaData);
#endif

}

kHttpd::kHttpd(int max_thread) {
    this->threadPool = new thread_pool(max_thread);
}

kHttpd::kHttpd(const char *web_root_path, int max_thread) {
    this->threadPool = new thread_pool(max_thread);
    if (web_root_path != nullptr) {
        this->web_root_path = realpath(web_root_path, nullptr);
    }
}


kHttpd::~kHttpd() {
    delete this->threadPool;
    this->threadPool = nullptr;
}

int kHttpd::listen(int listen_count, short port, const char *ip) {
    this->fd = kekxv::socket::listen(port, ip, listen_count);
    if (fd <= 0) {
        _logger->e(TAG, __LINE__, "cannot listen");
        return fd;
    }

    _logger->i(__FILENAME__, __LINE__, " http://%s:%d", ip, port);
//    std::thread _poll_check_run(poll_check_run, this);

    do {

        ssize_t list_len = socket_fd_list.size();
        auto *poll_fd = new struct pollfd[list_len + 1];
        for (ssize_t i = 0; i < list_len; i++) {
            poll_fd[i].fd = socket_fd_list[i];
            poll_fd[i].events = POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND | \
                     POLLERR | POLLHUP | POLLNVAL;
            poll_fd[i].revents = 0;
        }
        poll_fd[list_len].fd = fd;
        poll_fd[list_len].events = POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND | \
                     POLLERR | POLLHUP | POLLNVAL;;
        poll_fd[list_len].revents = 0;

        int num_ready = poll(poll_fd, list_len + 1, -1);
        if ((num_ready == -1) && (errno == EINTR)) {
            // lock.unlock();
            delete[] poll_fd;
            continue;        //被信号中断，继续等待
        } else if (num_ready == 0) {
            // lock.unlock();
            delete[] poll_fd;
            usleep(1);
            continue;
        } else if (0 > num_ready) {
            for (ssize_t i = list_len - 1; i >= 0; i--) {
                shutdown(socket_fd_list[i], SHUT_RDWR);
                close(socket_fd_list[i]);
            }
            socket_fd_list.clear();
            continue;
        }
        if (poll_fd[list_len].revents > 0) {
            int new_fd = kekxv::socket::accept(fd, nullptr, nullptr);
            if (new_fd < 0) {
                _logger->e(TAG, __LINE__, "Accept error in on_accept()");
            } else {
                add_poll_check(new_fd);
            }
            num_ready--;
        }
        for (ssize_t i = list_len - 1; i >= 0 && num_ready > 0; i--) {
            if (poll_fd[i].revents & POLLIN
                || poll_fd[i].revents & POLLPRI
                || poll_fd[i].revents & POLLRDNORM
                || poll_fd[i].revents & POLLRDBAND
                    ) {
                threadPool->commit([this](int new_fd) -> void {
                    // _logger->d(TAG, __LINE__, "======== client thread run ========");
                    int _fd = -1;
                    if (isWebSocket) {
                        _fd = kWebSocketClient(this, new_fd).run();
                    } else {
                        _fd = kHttpdClient(this, new_fd).run();
                    }
                    if (_fd > 0) {
                        shutdown(_fd, SHUT_RDWR);
                        close(_fd);
                        // self->add_poll_check(_fd);
                    }
                    // _logger->d(TAG, __LINE__, "======== client thread end ========");
                }, poll_fd[i].fd);
                this->socket_fd_list.erase(this->socket_fd_list.begin() + i);
                num_ready--;
            } else if (poll_fd[i].revents & POLLERR
                       || poll_fd[i].revents & POLLHUP
                       || poll_fd[i].revents & POLLNVAL
                    ) {
                shutdown(poll_fd[i].fd, SHUT_RDWR);
                close(poll_fd[i].fd);
                socket_fd_list.erase(socket_fd_list.begin() + i);
                num_ready--;
            }
        }
    } while (isRunning);

    stoped.store(true);
//    _poll_check_run.join();
    _logger->i(TAG, __LINE__, "finished");
    return 0;
}

void kHttpd::add_poll_check(int new_fd) {
    // std::unique_lock<std::mutex> lock{this->m_lock};
    socket_fd_list.push_back(new_fd);
    // lock.unlock();
    // cv_task.notify_one();
}

int
kHttpd::check_host_path(class kHttpdClient *_kHttpdClient, const std::string &Host, const std::string &method,
                        const std::string &url_path) {
    if (url_cb_tasks.find(Host + "_" + method + "_" + url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[Host + "_" + method + "_" + url_path](_kHttpdClient, _kHttpdClient->body_data, url_path,
                                                                  method, 0, nullptr);
    } else if (url_cb_tasks.find(Host + "_" + "_" + url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[Host + "_" + "_" + url_path](_kHttpdClient, _kHttpdClient->body_data, url_path,
                                                         method, 0, nullptr);
    } else if (url_cb_tasks.find(method + "_" +
                                 url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[method + "_" +
                            url_path](_kHttpdClient, _kHttpdClient->body_data, url_path,
                                      method, 0, nullptr);
    } else if (url_cb_tasks.find(url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[url_path](_kHttpdClient, _kHttpdClient->body_data, url_path,
                                      method, 0, nullptr);
    } else if (gen_cb_task != nullptr) {
        return gen_cb_task(_kHttpdClient, _kHttpdClient->body_data, url_path,
                           method,
                           Host, 0, nullptr);
    }
    return -1;
}

int
kHttpd::check_host_path(class kWebSocketClient *_kWebSocketClient, int type, const std::vector<unsigned char> &data) {
    if (url_cb_tasks.find(_kWebSocketClient->header["host"] + "_" + _kWebSocketClient->method + "_" +
                          _kWebSocketClient->url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[_kWebSocketClient->header["host"] + "_" + _kWebSocketClient->method + "_" +
                            _kWebSocketClient->url_path](_kWebSocketClient, data, _kWebSocketClient->url_path,
                                                         _kWebSocketClient->method, type, nullptr);
    } else if (url_cb_tasks.find(_kWebSocketClient->header["host"] + "_" + "_" +
                                 _kWebSocketClient->url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[_kWebSocketClient->header["host"] + "_" + "_" +
                            _kWebSocketClient->url_path](_kWebSocketClient, data, _kWebSocketClient->url_path,
                                                         _kWebSocketClient->method, type, nullptr);
    } else if (url_cb_tasks.find(_kWebSocketClient->method + "_" +
                                 _kWebSocketClient->url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[_kWebSocketClient->method + "_" +
                            _kWebSocketClient->url_path](_kWebSocketClient, data, _kWebSocketClient->url_path,
                                                         _kWebSocketClient->method, type, nullptr);
    } else if (url_cb_tasks.find(_kWebSocketClient->url_path) != url_cb_tasks.end()) {
        return url_cb_tasks[_kWebSocketClient->url_path](_kWebSocketClient, data, _kWebSocketClient->url_path,
                                                         _kWebSocketClient->method, type, nullptr);
    } else if (gen_cb_task != nullptr) {
        return gen_cb_task(_kWebSocketClient, data, _kWebSocketClient->url_path,
                           _kWebSocketClient->method,
                           _kWebSocketClient->header["host"], type, nullptr);
    }
    return _kWebSocketClient->send(data, type) >= 0;
}

void
kHttpd::set_cb(kHttpd::url_cb task, const std::string &url_path, const std::string &method, const std::string &host) {
    string key;
    if (!host.empty()) {
        key += host + "_";
        key += method + "_";
    }
    if (!method.empty()) {
        key += method + "_";
    }
    key += url_path;
    url_cb_tasks[key] = std::move(task);
}

void kHttpd::set_gencb(kHttpd::gen_cb task) {
    gen_cb_task = std::move(task);
}
