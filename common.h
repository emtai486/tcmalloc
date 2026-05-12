// 公共头文件，包含一些公共头
#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include <assert.h>
using std::cout;
using std::endl;
// 小于等于MAX_BYTES，就找thread cache申请
// 大于MAX_BYTES，就直接找page cache或者系统堆申请

static const int MAX_BYTES = 256 * 1024;
// thread cache 和central cache自由链表哈希桶的表大小
static const size_t NFREELISTS = 208;

// obj需要指向的地址
void *&NextObj(void *ptr)
{
    return *(void **)ptr;
}
// 自由链表
class FreeList
{
public:
    void Push(void *obj)
    {
        assert(obj);
        // 头插
        obj = NextObj(obj);
        _freeList = obj;
    }
    void *Pop()
    {
        assert(_freeList);
        // 头删
        void *obj = _freeList;
        _freeList = NextObj(obj);
        return _freeList;
    }
    //判断链表是否为空
    bool Empty()
    {
        return _freeList==nullptr;
    }

private:
    void *_freeList = nullptr;
};
// 管理对齐和映射等关系
class Sizeclass
{
public:
    // 整体控制在最多10%左右的内碎片浪费
    // [1,128]                 8byte对齐    freelist[0,16)
    // [128+1,1024]            16byte对齐    freelist[16,72)
    // [1024+1,81024]         128byte对齐   freelist[72,128)
    // [8*1024+1,641024]       1024byte对齐   freelist[128,184)
    // [64*1024+1,256*1024]     8*1024byte对齐 freelist[184,208)

    // 通过对齐数，计算对齐后的大小
    static inline size_t _RoundUp(size_t bytes, size_t alignNum)
    {
        // return ((bytes/alignNum+1)*alignNum);
        // 使用位运算，
        return (((bytes) + alignNum - 1) & ~(alignNum - 1));
    }

    // 对齐大小计算
    static inline size_t RoundUp(size_t bytes)
    {
        if (bytes <= 128)
        {
            return _RoundUp(bytes, 8);
        }
        else if (bytes <= 1024)
        {
            return _RoundUp(bytes, 16);
        }
        else if (bytes <= 81024)
        {
            return _RoundUp(bytes, 128);
        }
        else if (bytes <= 64 * 1024)
        {
            return _RoundUp(bytes, 1024);
        }
        else if (bytes <= 2561024)
        {
            return _RoundUp(bytes, 8 * 1024);
        }
        else
        {
            assert(false);
        }
        return -1;
    }
    // 传入大小和对齐的数是2的多少次方，计算哪个桶
    static inline size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }

    // 计算映射的哪一个自由链表桶
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);

        // 每个区间有多少个链
        static int group_array[4] = {16, 56, 56, 56};
        if (bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if (bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if (bytes <= 81024)
        {
            return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (bytes <= 64 * 1024)
        {
            return _Index(bytes - 8 * 1024, 10) + group_array[2] +
                   group_array[1] + group_array[0];
        }
        else if (bytes <= 256 * 1024)
        {
            return _Index(bytes - 64 * 1024, 13) + group_array[3] +
                   group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
        }

        return -1;
    }
};