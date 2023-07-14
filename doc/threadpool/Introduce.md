# Introduce  
&emsp;**�̳߳� - ThreadPool**  

* ʹ��`<thread>`�⣬������ windows��Linux ��֧�ָÿ��ƽ̨
* ʹ��`<future>`�⣬֧��ͨ�� get() ��������ȡ���񷵻ؽ��
* ͨ��һ�� **�����߳�** �������̵߳����Ӻͼ��٣����Զ�̬�����߳�  
* ��Ϊ���� **�����߳�** ����˻��һ���߳̿������ڵ��������߳�

## File
* Header file `"threadpool.hh"`
* Implementation file `"threadpool.cpp"`

## Standard
�������� ISO C++17 ����֮��ı�׼�±���

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
* ���� **����ģʽ** ,�̳߳����ڹ����̣߳���˸�����廯Ψһ  

## Use
* ���� `include"threadpool.hh"`
* �����ռ䣬ʹ��`using namespace pool;` ���� `pool::xxx` ��ʹ��
* ��ȡ�̳߳ض��� `pool::threadpool* tp = &pool::threadpool::get();`
* �������� `tp->enqueue(work_function);`
	* ��ȡ��� `auto ret = tp->enqueue(work_function).get();`
* ����ͷ�ļ���ʵ���ļ� `threadpool.hh` `threadpool.cpp`
* �����������ݲο� Detail.md �� Example.md

