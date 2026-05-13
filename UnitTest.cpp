#include "common.h"
#include "ConcurrentAlloc.h"

static void Alloc1()
{
    for (size_t i = 0; i < 5; i++)
        void *ptr = ConcurrentAlloc(7);
}

static void Alloc2()
{
    for (size_t i = 0; i < 5; i++)
        void *ptr = ConcurrentAlloc(5);
}

void TestTLS()
{
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);
    t1.join();
    t2.join();
}
int main()
{
    TestTLS();
    return 0;
}