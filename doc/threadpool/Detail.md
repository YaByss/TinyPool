# Detail 
&emsp;线程池实现的具体细节

## HeadFile - "threadpool.hh"

### Namespace
&emsp;文件内所有内容均属于命名空间 `pool`   

### Struct and Class
* `class threadpool;`

#### class threadpool   
```
class threadpool
{
private:
	enum
	{//匿名枚举，用 0-4 来描述当前线程池的状态 
		RUNNING = 0,	//正常工作状态
		SHUTDOWN = 1,	//不接受新任务，处理剩余任务
		STOP = 2,	//不接收新任务，不处理任务
		TIDYING = 3,	//任务数量为空
		TERMINATED = 4	//回收所有线程
	};
	std::mutex m_mtx_task;			//工作线程使用拿取任务的互斥锁
	std::mutex m_mtx_work;			//管理线程队列的互斥锁
	std::mutex m_mtx_set;			//管理设置的互斥锁
	std::condition_variable m_cond_work;	//工作线程使用的条件变量
	std::condition_variable m_cond_ctl;	//管理线程使用的条件变量
private:
	std::thread* m_ctl_pthread;		//指向 管理线程 的指针
	std::atomic<int> m_state;		//存储线程池当前状态
	std::atomic<int> m_reclaimed;		//用于指出当前可回收的空闲线程
	std::atomic<bool> m_terminate;		//终结开关，由 terminate() 开启
	std::atomic<bool> m_resize;		//调整线程数量的开关，由 set_core_threads() 开启
	std::atomic<uint32_t> m_core_threads;	//常驻的核心工作线程数量，默认 1
	std::atomic<uint32_t> m_max_threads;	//最大线程数量，默认为当前硬件支持线程数
	std::atomic<double> m_time_monitor;	//管理线程循环监测的时间间隔，单位 秒
private:
	std::list<std::thread::id> m_works;		//工作线程的链表
	std::queue<std::function<void()>> m_taskq;	//任务队列
	std::atomic<int64_t> m_work_max_cost;		//每执行一次工作的最大时间花费，单位 毫秒
private:
	//构造函数，删除了 复制构造函数 和 赋值运算符，防止单例对象被拷贝
	threadpool();
	threadpool(const threadpool&) = delete;
	threadpool& operator=(const threadpool&) = delete;	
	void add_thread();	//添加新线程的 内部函数 
	void run_ctl();		//管理线程执行的 循环函数  
	void run_work();	//工作线程执行的 循环函数 
	void terminated();	//终结函数，由 shutdown() 和 shutdown_now() 调用
public:
	//析构函数
	inline virtual ~threadpool() { 
		if(m_state.load()!=TERMINATED)shutdown_now();
	}

	//单例模式获取具体化的公开成员函数  
	inline static threadpool& get() {
		static threadpool instance;
		return instance;
	}

	//初始化公开成员函数，用于执行工作前调整 核心工作线程数量、管理线程监控间隔、最大线程数
	void init(uint32_t core_threads = static_cast<uint32_t>(1), 
		double timeToMonitor = static_cast<double>(3),
		uint32_t max_threads = std::thread::hardware_concurrency());

	//线程池关闭函数，执行后拒绝接受任务，并且在处理完任务后执行退出
	inline void shutdown() {
		std::unique_lock<std::mutex>lock(m_mtx_set);
		m_state.store(SHUTDOWN);
		lock.unlock();
		terminated();
	}

	//线程池关闭函数，执行后拒绝接受任务，并且放弃处理任务立即退出
	inline void shutdown_now(){
		std::unique_lock<std::mutex>lock(m_mtx_set);
		m_state.store(STOP);
		lock.unlock();
		terminated();
	}

	//任务入队 模板函数
	template<class F,class... Args>
	auto enqueue(F&& f, Args&& ... args) -> std::future<std::invoke_result_t<F, Args...>>;

	//获取当前运行的 工作线程 数量，注不包括唯一的管理线程
	inline size_t get_thread_num()const {
		return m_works.size();
	}

	//设置最大线程数量
	void set_max_threads(uint32_t num);

	//设置核心工作线程数量
	void set_core_threads(uint32_t num);

	//设置管理线程轮询间隔
	inline void set_monitor_time(double time) {//(seconds)
		std::unique_lock<std::mutex>lock(m_mtx_set);
		m_time_monitor.store(time);
	}

	//线程池重新启动函数，需要在线程池完全退出 state==TERMINATED 下才生效
	void restart();

	//获取当前工作列表未被取走的工作数量
	inline bool get_task_num()const{
		return m_taskq.size();
	}

};
```
## ImplementFile - "threadpool.cpp"
&emsp;具体参考 `threadpool.cpp`
 
