#pragma once
#include "common.h"
#include "PageCache.hpp"
// 一个进程中只需要有一个
// 单例模式
class CentralCache
{
public:
    // 对外开放返回唯一对象的接口
    static CentralCache *GetInstance()
    {
        return &_sInst;
    }
    // 获取⼀个⾮空的span
    Span *GetOneSpan(SpanList &list, size_t size)
    {
        // 查看当前spanlist中是否还有没有分配的对象span
        Span *it = list.Begin();
        // 遍历spanlist
        while (it != list.End())
        {
            if (it->_freeList != nullptr)
                return it;
            else
                it = it->_next;
        }

        // 先把central cache的桶锁解掉，这样其他线程释放内存，不会阻塞
        list._mtx.unlock();
        // 没有空闲span只能找 page span

        PageCache::GetInstance()->_pageMtx.lock();
        Span *span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
        span->_isUse = true;
        PageCache::GetInstance()->_pageMtx.unlock();

        // 切分不需要加锁，因为其他线程访问不到

        // 计算span的起始地址和大小（byte）
        // 页号*一页的大小
        char *start = (char *)((span->_pageId) << (PAGE_SHIFT));
        size_t bytes = ((span->_n) << (PAGE_SHIFT));
        char *end = (start + bytes);
        // 把页切分成一个个span挂起来
        // 先切一块作为头，方便后续尾插
        span->_freeList = start;
        start += size;
        void *tail = span->_freeList;

        while (start < end)
        {
            NextObj(tail) = start;

            tail = NextObj(tail);
            start += size;
        }
        // 切好之后，挂到桶里面，需要枷锁
        list._mtx.lock();

        list.PushFront(span);

        return span;
    }
    // 从中⼼缓存获取⼀定数量的对象给thread cache,start,end是输出型参数
    size_t FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size)
    {
        // 给哪个桶嘞？算一算
        size_t index = SizeClass().Index(size);
        // 加上桶锁
        _spanLists[index]._mtx.lock();

        // 获取一个非空页
        Span *span = GetOneSpan(_spanLists[index], size);

        assert(span);
        assert(span->_freeList);
        // 从span中获取batchNum个对象
        // 如果不够batchNum个，有多少拿多少
        start = span->_freeList;
        end = start;
        size_t i = 0;
        size_t actualNum = 1;
        while (i < batchNum - 1 && NextObj(end) != nullptr)
        {
            end = NextObj(end);
            ++i;
            ++actualNum;
        }
        // 页指向剩下的块
        span->_freeList = NextObj(end);
        // 切走的最后一块指向空
        NextObj(end) = nullptr;
        // 记录使用的span数量
        span->_useCount += actualNum;
        // 解锁
        _spanLists[index]._mtx.unlock();

        return actualNum;
    }
    void ReleaseListToSpans(void *start, size_t byte_size)
    {
        // 算桶
        size_t index = SizeClass().Index(byte_size);
        // 加锁
        _spanLists[index]._mtx.lock();
        while (start)
        {
            void *next = NextObj(start);

            Span *span = PageCache::GetInstance()->MapObjectToSpan(start);
            NextObj(start) = span->_freeList;
            span->_freeList = start;
            span->_useCount--;

            // 说明span的切分出去的所有小块内存都回来了
            // 这个span就可以再回收给page cache，pagecache可以再尝试去做前后页的合并
            if (span->_useCount == 0)
            {
                _spanLists[index].Erase(span);
                span->_freeList = nullptr;
                span->_next = nullptr;
                span->_prev = nullptr;

                // 释放span给page cache时，使用page cache的锁就可以了
                // 这时把桶锁解掉
                _spanLists[index]._mtx.unlock();

                PageCache::GetInstance()->_pageMtx.lock();
                PageCache::GetInstance()->ReleaseSpanToPageCache(span);
                PageCache::GetInstance()->_pageMtx.unlock();

                _spanLists[index]._mtx.lock();
            }

            start = next;
        }
        // 解锁
        _spanLists[index]._mtx.unlock();
    }

private:
    SpanList _spanLists[NFREELISTS];

private:
    // 构造函数私有
    CentralCache() {};
    // 拷贝构造禁用
    CentralCache(const CentralCache &) = delete;
    // 自己私有的创建一个类
    static CentralCache _sInst;
};

// // inline 解决了声明但是没有定义的情况
inline CentralCache CentralCache::_sInst;