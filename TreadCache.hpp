#pragma once
#include "common.h"

class TreadCache
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
        }
    }
    void Deallocate(void *ptr, size_t size)
    {
    }

private:
    // 自由链表，哈希桶
    FreeList _freeList[NFREELISTS];
};