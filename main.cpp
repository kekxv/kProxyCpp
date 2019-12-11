#include <iostream>
#include <kHttpd.h>

int main(int argc, char **argv) {
    std::cout << "Hello, World!" << std::endl;
    kHttpd::Init();

    kHttpd kProxy("./", 20);
    kProxy.listen();
    return 0;
}