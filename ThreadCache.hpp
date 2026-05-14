#pragma once

#include "common.h"
#include "CentralCache.hpp"

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
        // 为空，向下层centralCache要
        else
        {
            // 从中心缓存获取对象
            return FetchFromCentralCache(index, alignSize);
        }
    }
    void Deallocate(void *ptr, size_t size)
    {
        assert(ptr);
        assert(size < MAX_BYTES);
        // 找出对应映射的自由链表桶，插入进去
        size_t index = Sizeclass().Index(size);
        _freeList[index].Push(ptr);
    }
    // 从中心缓存获取对象
    void *FetchFromCentralCache(size_t index, size_t size)
    {
        // 满开始的调节算法
        //  1、最开始不会一次向central cache一次批量要太多，因为要太多了可能用不完
        // 2、如果你不要这个size大小内存需求，那么batchNum就会不断增长，直到上限
        // 3、size越大，一次向central cache要的batchNum就越小
        // 4、size越小，一次向central cache要的batchNum就越大
        size_t batchNum = std::min(_freeList[index].MaxSize(), Sizeclass().NumMoveSize(size));
        if (batchNum == _freeList[index].MaxSize())
        {
            _freeList[index].MaxSize() += 1;
        }
        void *start = nullptr;
        void *end = nullptr;
        // 实际给的数量
        size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);

        assert(actualNum > 1);
        if (actualNum == 1)
        {
            assert(start == end);
            return start;
        }
        else
        {
            _freeList[index].PopRange(NextObj(start), end, size);
            return start;
        }
    }

private:
    // 自由链表，哈希桶
    FreeList _freeList[NFREELISTS];
};
// TLS thread local storage
// 一个全局变量，指向当前线程的TreadCache 是每个线程独立拥有的，各自访问的是各自的，起到了不用加锁，也能保护临界资源的作用
// static __declspec(thread) ThreadCache* pTLSThreadCache = nullptr ;
thread_local ThreadCache *pTLSThreadCache = nullptr;
