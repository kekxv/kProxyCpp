#include <iostream>
#include <kHttpd.h>

int main(int argc, char **argv) {
    std::cout << "Hello, World!" << std::endl;
    kHttpd::Init();

    kHttpd kProxy("./", 20);
    kProxy.isWebSocket = true;
    kProxy.listen();
    return 0;
}