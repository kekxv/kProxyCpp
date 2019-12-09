//
// Created by caesar on 2019/11/26.
//

#include "kProxy.h"
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>
#else

#include <arpa/inet.h>
#include <fcntl.h>

#endif

#include <kClient.h>
#include <socket.h>

using namespace std;
using namespace kekxv;

#ifdef __FILENAME__
const char *kProxy::TAG = __FILENAME__;
#else
const char *kProxy::TAG = "kProxy";
#endif

logger *kProxy::_logger = logger::instance();;

struct client_info {
    kProxy *self;
    struct event_base *base;
    struct event *read_ev;
};

void kProxy::Init() {
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

#ifdef _WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
}

kProxy::kProxy(int max_thread) {
    this->threadPool = new thread_pool(max_thread);
}

kProxy::~kProxy() {
    delete this->threadPool;
    this->threadPool = nullptr;
}

int kProxy::listen(int listen_count, short port, const char *ip) {
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
                _logger->d(TAG, __LINE__, "new_fd accepted is %d", new_fd);
                add_poll_check(new_fd);
                _logger->d(TAG, __LINE__, "accept() finished for fd=%d", new_fd);
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
                    int _fd = kClient(this, new_fd).run();
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

void kProxy::add_poll_check(int new_fd) {
    // std::unique_lock<std::mutex> lock{this->m_lock};
    socket_fd_list.push_back(new_fd);
    // lock.unlock();
    // cv_task.notify_one();
}

void kProxy::poll_check_run(kProxy *self) {
    do {
        std::unique_lock<std::mutex> lock{self->m_lock};
        self->cv_task.wait(lock,
                           [self] {
                               return self->stoped.load() || !self->socket_fd_list.empty();
                           }
        ); // wait 直到有 task
//        std::unique_lock<std::mutex> lock{self->m_lock};
        if (self->socket_fd_list.empty()) {
            lock.unlock();
//            usleep(3);
            continue;
        }
        ssize_t list_len = self->socket_fd_list.size();
        auto *poll_fd = new struct pollfd[list_len];
        for (ssize_t i = 0; i < list_len; i++) {
            poll_fd[i].fd = self->socket_fd_list[i];
            poll_fd[i].events = POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND | \
                     POLLERR | POLLHUP | POLLNVAL;
//            poll_fd[i].events = POLLSTANDARD;
            poll_fd[i].revents = 0;
        }
        int num_ready = poll(poll_fd, list_len, 0);
        if ((num_ready == -1) && (errno == EINTR)) {
            lock.unlock();
            delete[] poll_fd;
            continue;        //被信号中断，继续等待
        } else if (num_ready == 0) {
            lock.unlock();
            delete[] poll_fd;
            usleep(1);
            continue;
        } else if (0 > num_ready) {
            for (ssize_t i = list_len - 1; i >= 0; i--) {
                shutdown(self->socket_fd_list[i], SHUT_RDWR);
                close(self->socket_fd_list[i]);
            }
            self->socket_fd_list.clear();
            continue;
        }
        for (ssize_t i = list_len - 1; i >= 0 && num_ready > 0; i--) {
            if (poll_fd[i].revents & POLLIN
                || poll_fd[i].revents & POLLPRI
                || poll_fd[i].revents & POLLRDNORM
                || poll_fd[i].revents & POLLRDBAND
                    ) {
                self->threadPool->commit([self](int new_fd) -> void {
//                    _logger->d(TAG, __LINE__, "======== client thread run ========");
                    int _fd = kClient(self, new_fd).run();
                    if (_fd > 0) {
                        shutdown(_fd, SHUT_RDWR);
                        close(_fd);
                        // self->add_poll_check(_fd);
                    }
//                    _logger->d(TAG, __LINE__, "======== client thread end ========");
                }, poll_fd[i].fd);
                self->socket_fd_list.erase(self->socket_fd_list.begin() + i);
                num_ready--;
            } else if (poll_fd[i].revents & POLLERR
                       || poll_fd[i].revents & POLLHUP
                       || poll_fd[i].revents & POLLNVAL
                    ) {
                shutdown(poll_fd[i].fd, SHUT_RDWR);
                close(poll_fd[i].fd);
                self->socket_fd_list.erase(self->socket_fd_list.begin() + i);
                num_ready--;
            }
        }

    } while (!self->stoped);
}

