//threadpool.cpp
#include"threadpool.hh"

using namespace pool;
using namespace std::chrono_literals;

//============================= class threapool =============================

//private

//Construction
threadpool::threadpool() {
	//init
	m_terminate.store(false);
	m_core_threads.store(static_cast<uint32_t>(1));
	m_max_threads.store(std::thread::hardware_concurrency());
	m_time_monitor.store(static_cast<double>(3));
	m_reclaimed.store(0);
	m_resize.store(false);
	m_work_max_cost.store(0LL);
	//添加一个默认的 监控线程
	m_ctl_pthread = new std::thread(&threadpool::run_ctl, this);
#ifdef debug
	std::clog << "Threadpool Constructed." << std::endl;
#endif
	m_state.store(RUNNING);
}

//添加线程
//需要有 m_mtx_work 的函数才可以执行
void threadpool::add_thread()
{
	std::unique_lock<std::mutex>lock_work(m_mtx_work);
	std::thread th(&threadpool::run_work, this);
	m_works.emplace_back(th.get_id());
	th.detach();
#ifdef debug
	std::clog << "Add one thread." << std::endl;
#endif
}

//控制线程循环函数
//控制线程：默认一个线程用于监督任务决定是否创建、回收线程资源
//无任务情况下该线程进入 wait
//该线程为线程池初始化时开启的第一个线程，该线程 id 不加入 works 列表
void threadpool::run_ctl()
{
#ifdef debug
	std::clog << "Controller thread constructed." << std::endl;
#endif
	//初始化控制线程时，创建足够数量的 常驻工作线程 core_thread
	for (size_t i = 0; i < m_core_threads.load(); i++)
		add_thread();
	int tasknum = 0;
	while (1)
	{
		std::unique_lock<std::mutex>lock_set(m_mtx_set);
		//唤醒条件：
		//每隔 固定循环时间 m_time_monitor.load() 秒 唤醒
		//m_terminate 唤醒，由 terminate() 唤醒，准备执行退出
		//m_resize 唤醒，由 resize() 唤醒，调整线程数量
		// m_taskq.size() > tasknum; 当任务列表任务增加时，被 enqueue 唤醒，决定是否增加线程
		m_cond_ctl.wait_for(lock_set, m_time_monitor.load() * 1000ms, [&] {return m_terminate || m_resize || m_taskq.size() > tasknum; });
		
		//resize 唤醒
		if (m_resize.load())
		{
#ifdef debug
			std::clog << "ctl thread resizing..." << std::endl;
			std::clog << "Now work threads :" << m_works.size()<<"\n"
				<< "Core threads :" << m_core_threads.load()<<'\n'
				<< "Max threads :" << m_max_threads.load() << std::endl;
#endif
			if (m_works.size() < m_core_threads.load())
				while(m_core_threads.load()>m_works.size())
					add_thread();
			else if (m_works.size()>m_core_threads.load() and m_taskq.empty())
			{//如果当前无任务，则唤醒线程进行减少
				m_reclaimed.store(static_cast<int>(m_works.size() - m_core_threads.load()));
				m_cond_work.notify_all();
				while (m_reclaimed.load());//等待调整线程数量
			}
			m_resize.store(false);
#ifdef debug
			std::clog << "ctl thread resize down." << std::endl;
			std::clog << "Now threads :" << m_works.size() << "\n"
				<< "Core threads :" << m_core_threads.load() << '\n'
				<< "Max threads :" << m_max_threads.load() << std::endl;
#endif
			continue;
		}


		//在有任务没有工作线程的情况下，创建线程通知
		if (!m_works.size() and !m_taskq.empty()) {
#ifdef debug
			std::clog << "There is no work thread now,creating..." << std::endl;
#endif
			add_thread();
			m_cond_work.notify_one();
			continue;
		}

		if (!m_terminate)
		{//enqueue 唤醒
#ifdef debug
			std::clog << "ctl thread monitoring.." << std::endl;
#endif
			if (m_works.size() < m_max_threads.load() and !m_taskq.empty() and m_work_max_cost!=0)
			{//任务堆积情况，当前线程 < 最大线程数，存在任务，任务计时已更新，任务数量* 
				//计算应当增加的线程数,并增加线程
				while (m_works.size() < m_max_threads.load() and
					m_taskq.size() / m_works.size() * m_work_max_cost > m_taskq.size() / (m_works.size() + 1) * m_work_max_cost + 1000)
				{
					add_thread();
#ifdef debug
					std::clog << "Work Busy,add thread..." << std::endl;
#endif
				}
			}
			else if (m_works.size() > m_core_threads and m_taskq.size() < m_works.size())
			{//任务数量 < 线程数量，回收多余的临时线程
#ifdef debug
				std::clog << "Work Free,reclaiming thread..." << std::endl;
#endif
				m_reclaimed.store(m_reclaimed.load() + 1);
				m_cond_work.notify_one();
				while (m_reclaimed.load());//等待回收结束
			}
		}
		else {//terminate 唤醒 m_terminate = true
#ifdef debug
			std::clog << "Terminate rouse ctl thread." << std::endl;
#endif
			m_reclaimed.store(0);
			if (m_state.load() == SHUTDOWN){//等待任务执行
#ifdef debug
				std::clog << "ctl thread: SHUTDOWN..." << std::endl;
#endif
				m_state.store(TIDYING);
				while (!m_taskq.empty());//等待任务清空
				m_state.store(STOP);
			}
			if(m_state.load()==STOP){//直接回收全部线程
				m_cond_work.notify_all();
				while (!m_works.empty());//等待线程全部回收
				m_state.store(TERMINATED);
#ifdef debug
				std::clog << "state -> TERMINATED\nctl thread TID-"<<std::this_thread::get_id()<<" exit." << std::endl;
#endif
				return;
			}
			//m_terminated 状态下不应当运行到此处
			abort();
		}
		tasknum = m_taskq.size();
	}//while
	return;
}


//线程主要循环函数
void threadpool::run_work()
{
	while (1)
	{
		std::unique_lock<std::mutex>lock_task(m_mtx_task);
		m_cond_work.wait(lock_task,
			[&] {return m_terminate || !m_taskq.empty() || m_reclaimed; });

		//停止接受任务，立即退出，由 shutdown_now() 触发
		if (m_terminate and (m_state == STOP))
		{
#ifdef debug
			std::clog << "work thread TID-"<<std::this_thread::get_id()<<": terminated STOP" << std::endl;
#endif
			std::unique_lock<std::mutex>lock_work(m_mtx_work);
			m_works.remove(std::this_thread::get_id());
			return;
		}

		//判断 m_taskq.empty()
		if (!m_taskq.empty()) {
			auto TimePoint = std::chrono::high_resolution_clock::now();
			std::function<void()>task;
			task = std::move(m_taskq.front());
			m_taskq.pop();
			//完成取出任务操作后释放锁
			lock_task.unlock();
			//执行任务
			task();
			//记录最大执行任务所花费时间
			auto duration = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count()
				- std::chrono::time_point_cast<std::chrono::milliseconds>(TimePoint).time_since_epoch().count();
			if (duration > m_work_max_cost.load())
				m_work_max_cost.store(duration);
		}

		//查看当前预计回收线程数，判断是否退出
		if (m_reclaimed.load())
		{
			std::unique_lock<std::mutex>lock_work(m_mtx_work);
			if (m_reclaimed.load())
			{
				m_reclaimed.store(m_reclaimed.load() - 1);
				m_works.remove(std::this_thread::get_id());
#ifdef debug
				std::clog << "work thread TID-" << std::this_thread::get_id() << " :quit." << std::endl;
#endif
				lock_work.unlock();
				return;
			}
		}
	}//while
}


//结束所有线程，由 shutdown  shutdown_now 和 析构函数 调用
void threadpool::terminated()
{
#ifdef debug
	std::clog << "terminating..." << std::endl;
#endif
	m_terminate.store(true);
	m_cond_ctl.notify_one();
	m_ctl_pthread->join();
	delete m_ctl_pthread;
}

// public

//init
//设置 超时时长>1 核心常驻线程数>1  最大线程数>core_threads 
void threadpool::init(uint32_t core_threads, double timeToMonitor, uint32_t max_threads) {
#ifdef debug
	std::clog << "threadpool:init()" << std::endl;
#endif
	m_time_monitor.store(timeToMonitor);
	set_max_threads(max_threads);
	set_core_threads(core_threads);
}

//设置最大线程数
void threadpool::set_max_threads(uint32_t num)
{
#ifdef debug
	std::clog << "Set max threads to " << num << std::endl;
#endif
	m_max_threads.store(num);
	if (num < m_core_threads)
		set_core_threads(num);
}

//设置 核心常驻线程数 最小值为 1 
void threadpool::set_core_threads(uint32_t num)
{
#ifdef debug
	std::clog << "Set core threads to " << num << std::endl;
#endif
	std::unique_lock<std::mutex>lock(m_mtx_set);
	//核心线程数 大于 最大线程数，同步修改最大线程数
	if (num > m_max_threads)
		m_max_threads.store(num);
	//通知 ctl 线程修改
	m_core_threads.store(num);
	m_resize.store(true);
	m_cond_ctl.notify_one();
}

//线程池进入 TERMINATED 后 重启线程池
void threadpool::restart()
{//只有在 TERMINATED 后重启
#ifdef debug
	std::clog << "Restarting..." << std::endl;
	if (m_state != TERMINATED)
		std::clog << "threadpool is running, restarting failed." << std::endl;
#endif
	if (m_state != TERMINATED)return;
	std::unique_lock<std::mutex>lock(m_mtx_set);
	while (m_taskq.size())//清空任务列表
		m_taskq.pop();
	m_terminate.store(false);
	m_work_max_cost.store(0LL);
	m_ctl_pthread = new std::thread(&threadpool::run_ctl, this);
	m_state.store(RUNNING);
#ifdef debug
	std::clog << "threadpool: restarted." << std::endl;
#endif
}