# Introduce  
&emsp;**线程池 - ThreadPool**  

* 使用`<thread>`库，适用于 windows、Linux 等支持该库的平台
* 使用`<future>`库，支持通过 get() 方法来获取任务返回结果
* 通过一个 **管理线程** 来决定线程的增加和减少，可以动态增减线程  
* 因为采用 **管理线程** ，因此会多一个线程开销用于单纯管理线程

## File
* Header file `"threadpool.hh"`
* Implementation file `"threadpool.cpp"`

## Standard
请至少在 ISO C++17 或者之后的标准下编译

## include
```
#include<list>					//list
#include<queue>					//queue
#include<thread>				//std::thread
#include<mutex>					//std::mutex
#include<condition_variable>			//std::condition_variable
#include<future>				//std::future std::packaged_task
#include<functional>				//std::invoke_result_t bind
#include<atomic>				//std::atomic
#include<utility>				//std::forward 
#include<chrono>				//time
```
## Design
* 采用 **单例模式** ,线程池用于管理线程，因此该类具体化唯一  

## Use
* 包含 `include"threadpool.hh"`
* 命名空间，使用`using namespace pool;` 或者 `pool::xxx` 来使用
* 获取线程池对象 `pool::threadpool* tp = &pool::threadpool::get();`
* 插入任务 `tp->enqueue(work_function);`
	* 获取结果 `auto ret = tp->enqueue(work_function).get();`
* 编译头文件和实现文件 `threadpool.hh` `threadpool.cpp`
* 其他具体内容参考 Detail.md 和 Example.md

