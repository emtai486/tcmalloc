#pragma once
#include "common.h"

class PageCache
{
public:
    static PageCache *GetInstance()
    {
        return &_sInst;
    }
    // 获取⼀个K⻚的span
    Span *NewSpan(size_t k);

private:
    SpanList _spanLists[NPAGES];
    std::mutex pageMtx;
    // 饿汉-单例模式
    PageCache()
    {
    }
    PageCache(const PageCache &) = delete;
    static PageCache _sInst;
};
// // inline 解决了声明但是没有定义的情况
inline PageCache PageCache::_sInst;