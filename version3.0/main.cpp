#include "server.h"
using namespace std;

int main() {
    Server server = Server(12345);
    server.init_log("test.log", false);
    server.create_threadpool();
    server.set_events();
    server.start();
    return 0;
}