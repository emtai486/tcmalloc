#pragma once
#include "common.h"

class ThreadCache
{
public:
    // 申请和释放内存对象
    void *Allocate(size_t size)
    {
        assert(size <= MAX_BYTES);
        // 对齐
        size_t alignSize = Sizeclass().RoundUp(size);
        // 计算桶位置
        size_t index = Sizeclass().Index(size);
        // 当前位置的桶不为空，给空间
        if (!_freeList[index].Empty())
        {
            return _freeList[index].Pop();
        }
        // 为空，向下层controlCache要
        else
        {
            // 从中心缓存获取对象
     FetchFromCentralCache ( index,  size);

        }
        return nullptr;
    }
    void Deallocate(void *ptr, size_t size)
    {
        assert(ptr);
        assert(size<MAX_BYTES);
        //找出对应映射的自由链表桶，插入进去
        size_t index=Sizeclass().Index(size);
        _freeList[index].Push(ptr);
       
    }
     // 从中心缓存获取对象
     void* FetchFromCentralCache (size_t index, size_t size)
     {
        return nullptr;
     }

private:
    // 自由链表，哈希桶
    FreeList _freeList[NFREELISTS];
};
 // TLS thread local storage
 //一个全局变量，指向当前线程的TreadCache 是每个线程独立拥有的，各自访问的是各自的，起到了不用加锁，也能保护临界资源的作用
 //static __declspec(thread) ThreadCache* pTLSThreadCache = nullptr ;
 thread_local ThreadCache* pTLSThreadCache = nullptr;
