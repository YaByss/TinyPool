//threadpool.hh
#pragma once
#include<list>					//
#include<queue>					//
#include<thread>				//std::thread
#include<mutex>					//std::mutex
#include<condition_variable>	//std::condition_variable
#include<future>				//std::future std::packaged_task
#include<functional>			//std::invoke_result_t bind
#include<atomic>				//std::atomic
#include<utility>				//std::forward 
#include<chrono>				//time


#define debug
#ifdef debug
#include<iostream>
#endif


namespace pool
{
	//============================= Declaration =============================

	class threadpool
	{
	private:
		enum
		{
			RUNNING = 0,				//正常工作状态
			SHUTDOWN = 1,				//不接受新任务，处理剩余任务
			STOP = 2,					//不接收新任务，不处理任务
			TIDYING = 3,				//任务数量为空
			TERMINATED = 4				//回收所有线程
		};
		std::mutex								m_mtx_task;			//mutex task queue
		std::mutex								m_mtx_work;			//mutex work list
		std::mutex								m_mtx_set;			//mutex for set other params
		std::condition_variable					m_cond_work;		//worker condition
		std::condition_variable					m_cond_ctl;			//controller condition
	private:
		std::thread	*							m_ctl_pthread;		//ctl thread
		std::atomic<int>						m_state;			//state
		std::atomic<int>						m_reclaimed;		//The thread to be reclaimed
		std::atomic<bool>						m_terminate;		//terminated
		std::atomic<bool>						m_resize;			//resize threads num
		std::atomic<uint32_t>					m_core_threads;		//core threads num，默认 1
		std::atomic<uint32_t>					m_max_threads;		//max threads num，默认 1
		std::atomic<double>						m_time_monitor;		//ctl thread cycle monitoring time (seconds)
	private:
		std::list<std::thread::id>				m_works;			//thread list
		std::queue<std::function<void()>>		m_taskq;			//task queue
		std::atomic<int64_t>					m_work_max_cost;	//max work cost time(ms)
	private:
		threadpool();
		threadpool(const threadpool&) = delete;
		threadpool& operator=(const threadpool&) = delete;
		
		//thread run and control
		void add_thread();
		void run_ctl();
		void run_work();


		//terminated
		void terminated();

	public:
		inline virtual ~threadpool() {
			if(m_state.load()!=TERMINATED)shutdown_now();
#ifdef debug
			std::clog << "Threadpool Destructed." << std::endl;
#endif
		}

		//signelton get instance
		inline static threadpool& get() {
			static threadpool instance;
			return instance;
		}

		//init
		void init(uint32_t core_threads = static_cast<uint32_t>(1), 
			double timeToMonitor = static_cast<double>(3),
			uint32_t max_threads = std::thread::hardware_concurrency());

		// --> shutdown
		inline void shutdown() {
#ifdef debug
			std::clog << "threadpool: shutdown..." << std::endl;
#endif
			std::unique_lock<std::mutex>lock(m_mtx_set);
			m_state.store(SHUTDOWN);
			lock.unlock();
			terminated();
		}

		// --> stop
		inline void shutdown_now(){
#ifdef debug
			std::clog << "threadpool: shutdown now..." << std::endl;
#endif
			std::unique_lock<std::mutex>lock(m_mtx_set);
			m_state.store(STOP);
			lock.unlock();
			terminated();
		}

		//enqueue task template
		template<class F,class... Args>
		auto enqueue(F&& f, Args&& ... args) -> std::future<std::invoke_result_t<F, Args...>>;

		//get num of running threads
		inline size_t get_thread_num()const {
			return m_works.size();
		}

		//set max threads
		void set_max_threads(uint32_t num);

		//set core threads
		void set_core_threads(uint32_t num);

		//set ctl thread cycle monitoring time
		inline void set_monitor_time(double time) {//(seconds)
			std::unique_lock<std::mutex>lock(m_mtx_set);
			m_time_monitor.store(time);
		}

		//retstart thread pool
		void restart();

		//get task num
		inline bool get_task_num()const
		{
			return m_taskq.size();
		}

	};//class thread

	//============================= template =============================

	//添加任务 模板函数
	template<class F, class... Args>
	auto threadpool::enqueue(F&& f, Args&& ... args) -> std::future<std::invoke_result_t<F, Args...>>
	{
		using return_type = std::invoke_result_t<F, Args...>;
		auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::future<return_type> ret = task->get_future();
		//查看状态决定是否添加任务
#ifdef debug
		if(m_state)
		std::clog << "threadpool is not running,can not accept task." << std::endl;
#endif
		if (m_state)
			return ret;
		//将任务加入任务队列并通知 等待线程 执行
		std::unique_lock<std::mutex>lock_task(m_mtx_task);
#ifdef debug
		std::clog << "Add task" << std::endl;
#endif
		m_taskq.emplace([task]() {(*task)(); });
		lock_task.unlock();
		if (m_works.size())//如果有工作线程，则直接通知，否则交给 控制线程唤醒
			m_cond_work.notify_one();
		m_cond_ctl.notify_one();
		return ret;
	}


}//namespace pool