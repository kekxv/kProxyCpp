#include <iostream>
#include <kProxy.h>

int main() {
    std::cout << "Hello, World!" << std::endl;
    kProxy::Init();

    kProxy kProxy(100);
    kProxy.listen();
    return 0;
}