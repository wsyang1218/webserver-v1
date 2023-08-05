#include <getopt.h>
#include <string>
#include "net/EventLoop.h"
#include "Server.h"
#include "base/Logging.h"

using namespace std;

int main(int argc, char *argv[]) {
    int threadNum = 4;
    int port = 10000;
    string logPath = "./WebServer.log";

    // parse args
    int opt;
    const char *str = "t:l:p";
    while ((opt = getopt(argc, argv, str)) != -1)  {
        switch (opt)
        {
        case 't': {
            threadNum = atoi(optarg);
            break;
        }
        case 'l': {
            logPath = optarg;
            if (logPath.size() < 2 || optarg[0] != '/') {
            printf("logPath should start with \"/\"\n");
            abort();
            }
            break;
        }
        case 'p': {
            port = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
    Logger::setLogFileName(logPath);
    // STL库再多线程上的应用
    #ifndef _PTHREADS
        LOG << "_PTHREADS is not defined!";
    #endif
    EventLoop mainLoop;
    Server myHTTPServer(&mainLoop, threadNum, port);
    myHTTPServer.start();
    mainLoop.loop();
    return 0;
}