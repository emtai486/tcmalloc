// 公共头文件，包含一些公共头
#pragma once

#include <thread>
#include <mutex>

#include <iostream>
#include <vector>
#include <algorithm>

#include <time.h>
#include <assert.h>

using std::cout;
using std::endl;

// 小于等于MAX_BYTES，就找thread cache申请
// 大于MAX_BYTES，就直接找page cache或者系统堆申请
static const int MAX_BYTES = 256 * 1024;
// thread cache 和central cache自由链表哈希桶的表大小
static const size_t NFREELISTS = 208;
// page cache 管理span list哈希表⼤⼩
static const size_t NPAGES = 129;

// 32位平台下只有32，64位平台下，有32也有64，所有先判断有没有64
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
// linux
#endif

// obj需要指向的地址
static void *&NextObj(void *ptr)
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
    // 对一串操作
    void PopRange(void *&start, void *&end, size_t n)
    {
        NextObj(end) = _freeList;
        _freeList = start;
    }

    void *Pop()
    {
        assert(_freeList);
        // 头删
        void *obj = _freeList;
        _freeList = NextObj(obj);
        return obj;
    }
    // 判断链表是否为空
    bool Empty()
    {
        return _freeList == nullptr;
    }
    size_t &MaxSize()
    {
        return _maxSize;
    }

private:
    void *_freeList = nullptr;
    size_t _maxSize = 1;
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
    // ⼀次从中⼼缓存获取多少个
    static size_t NumMoveSize(size_t size)
    {
        if (size == 0)
            return 0;

        // [2, 512]，⼀次批量移动多少个对象的(慢启动)上限值
        // ⼩对象⼀次批量上限⾼
        // ⼩对象⼀次批量上限低
        int num = MAX_BYTES / size;
        if (num < 2)
            num = 2;

        if (num > 512)
            num = 512;

        return num;
    }
};

// 管理多个连续大块内存页的跨度结构
struct Span
{
    PAGE_ID _pageID = 0; // 起始页号
    size_t _n = 0;       // 页数

    // 带头双向链表结构
    Span *_prev = nullptr;
    Span *_next = nullptr;

    size_t _useCount = 0;      // 切好的小块内存的使用计数
    void *_freeList = nullptr; // 切好的小块内存的自由链表
};

// 带头双向链表管理页
class SpanList
{
public:
    SpanList()
    {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }
    // 任意插
    void Insert(Span *pos, Span *newSpan)
    {
        assert(pos);
        assert(newSpan);

        Span *prev = pos->_prev;

        // prev new pos
        newSpan->_prev = prev;
        newSpan->_next = pos;
        pos->_prev = newSpan;
        prev->_next = newSpan;
    }
    // 任意删
    void Erase(Span *pos)
    {
        assert(pos);
        assert(pos != _head);

        // prev Xpos next
        Span *next = pos->_next;
        Span *prev = pos->_prev;

        prev->_next = next;
        next->_prev = prev;
    }

    Span* Begin()
    {
        return _head->_next;
    }
    Span* End()
    {
        return _head;
    }

private:
    Span *_head;
public:
    std::mutex _mtx; // 桶锁
};