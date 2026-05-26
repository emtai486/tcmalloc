#pragma once
#include "common.h"

template <class T>
class ObjectPool
{
public:
    T *New()
    {
        T *obj = nullptr;
        // 优先从自由链表中拿内存
        if (_freeList)
        {
            // 先保存下一个结点的地址
            void *next = *(void **)_freeList;
            // obj取走自由链表中第一块内存
            obj = (T *)_freeList;
            _freeList = next;
        }
        else
        {
            // 当剩余内存小于本次申请需要的内存大小的时候
            if (_remainBytes < sizeof(T))
            {
                // 更新剩余字节数
                _remainBytes = 128 * 1024;
                //>>13,/8/1024,计算出页数
                _memory = (char *)SystemAlloc(_remainBytes >> 13);
                if (_memory == nullptr) // 失败了，抛异常
                {
                    throw std::bad_alloc();
                }
            }
            // obj指向新开辟空间
            obj = (T *)_memory;
            // 保证每次开辟的空间，至少有一个地址的大小，保证自由链表的每一个结点可以放得下
            size_t objSize = sizeof(T) < sizeof(void *) ? sizeof(void *) : sizeof(T);
            _memory += objSize;
            _remainBytes -= objSize;
        }
        // 定位new,显示调用T的构造函数初始化
        new (obj) T;
        return obj;
    }
    void Delete(T *obj)
    {
        // 显示调用析构函数清理对象
        obj->~T();
        // 头插
        _freeList = obj;
        // 对一个二级指针解引用，同时适配32/64位平台
        // 把这块内存前4/8字节用来存放地址
        *(void **)obj = nullptr;
    }

private:
    char *_memory = nullptr;   // 指向大块内存的指针
    size_t _remainBytes = 0;   // 大块内存的剩余字节数
    void *_freeList = nullptr; // 自由链表，管理自由内存的头指针
};

// /////////////////////////////////////////////////////////////
// /////////////////////下面是测试代码///////////////////////////
// ////////////////////////////////////////////////////////////
// struct TreeNode
// {
//     int _val;
//     TreeNode *_left;
//     TreeNode *_right;

//     TreeNode()
//         : _val(0), _left(nullptr), _right(nullptr)
//     {
//     }
// };

// void TestObjectPool()
// {
//     // 申请释放的轮次
//     const size_t Rounds = 3;

//     // 每轮申请释放多少次
//     const size_t N = 100000;

//     size_t begin1 = clock();
//     std::vector<TreeNode *> v1;
//     v1.reserve(N);

//     for (size_t j = 0; j < Rounds; ++j)
//     {
//         for (int i = 0; i < N; ++i)
//         {
//             v1.push_back(new TreeNode);
//         }
//         for (int i = 0; i < N; ++i)
//         {
//             delete v1[i];
//         }
//         v1.clear();
//     }

//     size_t end1 = clock();

//     ObjectPool<TreeNode> TNPool;
//     size_t begin2 = clock();
//     std::vector<TreeNode *> v2;
//     v2.reserve(N);

//     for (size_t j = 0; j < Rounds; ++j)
//     {
//         for (int i = 0; i < N; ++i)
//         {
//             v2.push_back(TNPool.New());
//         }
//         for (int i = 0; i < 100000; ++i)
//         {
//             TNPool.Delete(v2[i]);
//         }
//         v2.clear();
//     }
//     size_t end2 = clock();

//     cout << "new cost time:" << end1 - begin1 << endl;
//     cout << "object pool cost time:" << end2 - begin2 << endl;
// }
