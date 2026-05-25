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
        assert(k > 0);
        
        // 大于128 page的直接向堆申请
        if (k > NPAGES - 1)
        {
            void *ptr = SystemAlloc(k);
            Span *span = new Span;
            // Span* span = _spanPool.New();

            span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
            span->_n = k;
            // 页号缓存一下
            _idSpanMap[span->_pageId] = span;
            return span;
        }

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
                kSpan->_pageId = nSpan->_pageId;
                kSpan->_n = k;

                nSpan->_pageId += k;
                nSpan->_n -= k;
                // 把剩下的挂到新位置
                _spanLists[nSpan->_n].PushFront(nSpan);

                // 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
                for (PAGE_ID i = 0; i < kSpan->_n; ++i)
                {
                    _idSpanMap[kSpan->_pageId + i] = kSpan;
                }

                return kSpan;
            }
        }
        // 到这里说明没有大span,就要向堆去要了
        Span *bigSpan = new Span;
        void *ptr = SystemAlloc(NPAGES - 1);
        // 算页号，页数
        bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
        bigSpan->_n = NPAGES - 1;

        // 把这个申请到的span插入到对应的桶的位置，再次调用这个函数

        _spanLists[bigSpan->_n].PushFront(bigSpan);
        return NewSpan(k);
    }
    // 给地址，计算出_pageId,返回Span
    Span *MapObjectToSpan(void *obj)
    {
        PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
        auto ret = _idSpanMap.find(id);
        // 找到了
        if (ret != _idSpanMap.end())
        {
            return ret->second;
        }
        // 没找到
        else
        {
            assert(false);
            return nullptr;
        }
    }
    void ReleaseSpanToPageCache(Span *span)
    {
        // 大于128 page的直接还给堆
        if (span->_n > NPAGES - 1)
        {
            void *ptr = (void *)(span->_pageId << PAGE_SHIFT);
            SystemFree(ptr);
            // delete span;
           // _spanPool.Delete(span);

            return;
        }
        // 对span前后的页，尝试进行合并，缓解内存碎片问题
        // 向前合并
        while (1)
        {
            PAGE_ID prevId = span->_pageId - 1;
            auto ret = _idSpanMap.find(prevId);
            // 前面的页号没有，不合并了
            if (ret == _idSpanMap.end())
            {
                break;
            }

            // 前面相邻页的span在使用，不合并了
            Span *prevSpan = ret->second;
            if (prevSpan->_isUse == true)
            {
                break;
            }

            // 合并出超过128页的span没办法管理，不合并了
            if (prevSpan->_n + span->_n > NPAGES - 1)
            {
                break;
            }

            span->_pageId = prevSpan->_pageId;
            span->_n += prevSpan->_n;

            _spanLists[prevSpan->_n].Erase(prevSpan);
            delete prevSpan;
        }

        // 向后合并
        while (1)
        {
            PAGE_ID nextId = span->_pageId + span->_n;
            auto ret = _idSpanMap.find(nextId);
            if (ret == _idSpanMap.end())
            {
                break;
            }

            Span *nextSpan = ret->second;
            if (nextSpan->_isUse == true)
            {
                break;
            }

            if (nextSpan->_n + span->_n > NPAGES - 1)
            {
                break;
            }

            span->_n += nextSpan->_n;

            _spanLists[nextSpan->_n].Erase(nextSpan);
            delete nextSpan;
        }
        // 从桶中拿出，标记为未用
        _spanLists[span->_n].PushFront(span);
        span->_isUse = false;
        _idSpanMap[span->_pageId] = span;
        _idSpanMap[span->_pageId + span->_n - 1] = span;
    }

public:
    std::mutex _pageMtx;

private:
    SpanList _spanLists[NPAGES];
    std::unordered_map<PAGE_ID, Span *> _idSpanMap;
    // 饿汉-单例模式
    PageCache()
    {
    }
    PageCache(const PageCache &) = delete;
    static PageCache _sInst;
};
// // inline 解决了声明但是没有定义的情况
inline PageCache PageCache::_sInst;