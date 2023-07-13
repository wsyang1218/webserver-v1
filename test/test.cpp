#include "../code/threadpool.h"
#include <cstdio>
#include <unistd.h>

class request {
public:
    request() {}
    ~request() {}
    void process() {
        printf("processing...\n");
    }
private:
};

void testThreadPoll() {
    printf("start testing threapool module.\n");
    threadpool<request> pool(6); //调用默认构造函数
    for (int i = 0; i < 18; ++i) {
        request *r = new request();
        bool ret = pool.append(r);
        // printf("%d\n", i);
    }
}

void testHttpConn() {
    
}

int main() {
    testThreadPoll();
}