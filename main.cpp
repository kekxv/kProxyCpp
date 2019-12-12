#include <iostream>
#include <kHttpd.h>
#include <kWebSocketClient.h>

int main(int argc, char **argv) {
    std::cout << "Hello, World!" << std::endl;
    kHttpd::Init();

    kHttpd kProxy("./", 20);
    kProxy.isWebSocket = true;
    kProxy.set_gencb([](void *kClient, const std::vector<unsigned char> &data, const std::string &url_path,
                        const std::string &method,
                        const std::string &host, int type, void *arg) -> int {
        // std::cout << "回调调用" << std::endl;
        return ((kWebSocketClient *) kClient)->send(data, type) >= 0;
    });
    kProxy.set_cb([](void *kClient, const std::vector<unsigned char> &data, const std::string &url_path,
                     const std::string &method, int type, void *arg) -> int {
        //        std::cout << "回调调用:" << url_path << " " << method << " " << ((kWebSocketClient *) kClient)->header["host"]
        //                  << std::endl;

        return ((kWebSocketClient *) kClient)->send(
                "回调调用:" + url_path + " " + method + " " + ((kWebSocketClient *) kClient)->header["host"]) >= 0;
        return ((kWebSocketClient *) kClient)->send(data, type) >= 0;
    }, "/ws");
    kProxy.listen();
    return 0;
}