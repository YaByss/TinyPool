# Example
&emsp;Ê¾Àý  

```
#include"threadpool.hh"
#include<iostream>
#include<chrono>
using namespace std::chrono_literals;
int g = 0;

int f2(int a)
{
	return a * a;
	std::this_thread::sleep_for(300ms);
}
void addtask(int num)
{
	pool::threadpool* tp = &pool::threadpool::get();
	for (int i = 1; i <= num; i++)
	{
		auto a = tp->enqueue(f2, i);
		std::cout<<i<<"^2 = " << a.get() << std::endl;
		std::this_thread::sleep_for(1ms);
	}
}

const int task_num = 1000;
int main()
{
	pool::threadpool* tp = &pool::threadpool::get();

	std::cout << "####################### Add Task Test" << std::endl;
	addtask(task_num);
	std::cout << "Add task Finished." << std::endl;
	getchar();
}
```
