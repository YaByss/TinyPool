//test.cpp

#include"threadpool.hh"
#include<iostream>
#include<chrono>
using namespace std::chrono_literals;
int g = 0;
std::mutex mtx;

void f()
{
	std::unique_lock<std::mutex>lock(mtx);
	g++;
	lock.unlock();
	std::this_thread::sleep_for(300ms);
}
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
		tp->enqueue(f);
		//auto a = tp->enqueue(f2, i);
		//std::cout<<i<<"^2 = " << a.get() << std::endl;
		std::this_thread::sleep_for(1ms);
	}
}

const int task_num = 1000;
int main()
{
	pool::threadpool* tp = &pool::threadpool::get();
	
	//addtask
	//getchar();
	std::cout << "####################### Add Task Test" << std::endl;
	addtask(task_num);
	std::cout << "Add task OK" << std::endl;
	std::cout << "Wait.." << std::endl;
	while (g != task_num);
	std::cout << "g = " << g <<" Test OK" << std::endl;
	
	///set_monitor_time 
	//getchar();
	//std::cout << "####################### set monitor time Test" << std::endl;
	//std::cout << "press AnyKey to set 1 second" << std::endl;
	//getchar();
	//tp->set_monitor_time(1);
	//std::cout << "press AnyKey to set 5 second" << std::endl;
	//getchar();
	//tp->set_monitor_time(5);


	////get_thread_num
	//getchar();
	//std::cout << "####################### Get thread num Test" << std::endl;
	//std::cout << tp->get_thread_num() << std::endl;
	//std::cout << "Test OK" << std::endl;

	////set core thread
	//getchar();
	//std::cout << "####################### Set core thread Test" << std::endl;
	//tp->set_core_threads(3);
	//while (tp->get_thread_num() != 3);
	//tp->set_core_threads(5);
	//while (tp->get_thread_num() != 5);
	//tp->set_core_threads(0);
	//while (tp->get_thread_num() != 0);
	//std::cout << "Test OK" << std::endl;
	

	////shutdown and restart
	//getchar();
	//std::cout << "####################### Shutdown and restart Test" << std::endl;
	//tp->set_core_threads(0);
	//tp->shutdown();

	//g = 0;
	//addtask(5);
	//tp->restart();
	//std::cout << g << std::endl;
	//g = 0;
	//addtask(10);
	//while (g != 10);
	//std::cout << g << std::endl;
	//std::cout << "Test OK" << std::endl;

	////shutdown_now
	//getchar();
	//std::cout << "####################### Shutdown_now Test" << std::endl;
	//g = 0;
	//addtask(task_num);
	//std::clog << "Add ok" << std::endl;
	//tp->shutdown_now();
	//std::cout << "need to do:" << tp->get_task_num();
	//std::cout << g << std::endl;
	//std::cout << "Test OK" << std::endl;

	getchar();
}
