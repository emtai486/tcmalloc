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
    Span *NewSpan(size_t k)
    {
        assert(k > 0 && k < NPAGES);
        if (!_spanLists[k].Empty()) // 当前桶不为空
        {
            return _spanLists[k].PopFront();
        }
        // 当前同为空，检查后面的有没有span，有的话切出来
        for (int i = k + 1; i < NPAGES; ++i)
        {
            if (!_spanLists[i].Empty())
            {
                // 拿出这个大span
                Span *nSpan = _spanLists[i].PopFront();
                Span *kSpan = new Span;
                // nspan头部切k页出来
                kSpan->_pageID = nSpan->_pageID;
                kSpan->_n = k;

                nSpan->_pageID += k;
                nSpan->_n -= k;
                // 把剩下的挂到新位置
                _spanLists[nSpan->_n].PushFront(nSpan);

                return kSpan;
            }
        }
        // 到这里说明没有大span,就要向堆去要了
        Span *bigSpan = new Span;
        void *ptr = SystemAlloc(NPAGES - 1);
        // 算页号，页数
        bigSpan->_pageID = (PAGE_ID)ptr << PAGE_SHIFT;
        bigSpan->_n = NPAGES - 1;

        //把这个申请到的span插入到对应的桶的位置，再次调用这个函数

        _spanLists[bigSpan->_n].PushFront(bigSpan);
        return NewSpan(k);
    }
    public:
     std::mutex pageMtx;

private:
    SpanList _spanLists[NPAGES];
   
    // 饿汉-单例模式
    PageCache()
    {
    }
    PageCache(const PageCache &) = delete;
    static PageCache _sInst;
};
// // inline 解决了声明但是没有定义的情况
inline PageCache PageCache::_sInst;