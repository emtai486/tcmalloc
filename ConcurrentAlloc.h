#pragma once
#include "common.h"
#include "ThreadCache.hpp"
#include "ObjectPool.hpp"
// 创建和释放TLS
static void *ConcurrentAlloc(size_t size)
{
	// 通过TLS 每个线程无锁的获取自己专属的TreadCache对象

	if (size > MAX_BYTES) // 大于256kb走的逻辑
	{
		// 对齐，计算实际页数
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span *span = PageCache::GetInstance()->NewSpan(kpage);
		// span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void *ptr = (void *)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else // 正常大小走的逻辑
	{
		// 用定长内存池优化
		static ObjectPool<ThreadCache> tcPool;
		// pTLSThreadCache = new ThreadCache;
		pTLSThreadCache = tcPool.New();
		// cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

		return pTLSThreadCache->Allocate(size);
	}
}
static void ConcurrentFree(void *ptr, size_t size)
{
	if (size > MAX_BYTES) // 大于128页的释放
	{
		Span *span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}