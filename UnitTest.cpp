#include "common.h"
#include "ConcurrentAlloc.h"
#include "CentralCache.hpp"
static void Alloc1()
{
    for (size_t i = 0; i < 5; i++)
        void* ptr = ConcurrentAlloc(7);
}

static void Alloc2()
{
    
        void* p1 = ConcurrentAlloc(5);
        void* p2 = ConcurrentAlloc(9);
        void* p3 = ConcurrentAlloc(13);

        ConcurrentFree(p1, 5);
        ConcurrentFree(p2, 9);
        ConcurrentFree(p3, 13);

}
void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);
	void* p6 = ConcurrentAlloc(8);
	void* p7 = ConcurrentAlloc(8);


	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;

	ConcurrentFree(p1, 6);
	ConcurrentFree(p2, 8);
	ConcurrentFree(p3, 1);
	ConcurrentFree(p4, 7);
	ConcurrentFree(p5, 8);
	ConcurrentFree(p6, 8);
	ConcurrentFree(p7, 8);
}

void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e, 6);
	}
}

void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(16);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e, 16);
	}
}

void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);

	t1.join();
	t2.join();
}
int main()
{
	MultiThreadAlloc2();
    return 0;
}