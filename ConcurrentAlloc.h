#pragma once
#include "common.h"
#include "ThreadCache.hpp"

//创建和释放TLS
static void* ConcurrentAlloc(size_t size)
{
//通过TLS 每个线程无锁的获取自己专属的TreadCache对象
if(pTLSThreadCache == nullptr)
pTLSThreadCache = new ThreadCache;
return pTLSThreadCache->Allocate(size);
}
static void ConcurrentFree(void* ptr,size_t size)
{
assert(pTLSThreadCache);
pTLSThreadCache->Deallocate(ptr,size);

}