# Detail 
&emsp;�̳߳�ʵ�ֵľ���ϸ��

## HeadFile - "threadpool.hh"

### Namespace
&emsp;�ļ����������ݾ����������ռ� `pool`   

### Struct and Class
* `class threadpool;`

#### class threadpool   
```
class threadpool
{
private:
	enum
	{//����ö�٣��� 0-4 ��������ǰ�̳߳ص�״̬ 
		RUNNING = 0,	//��������״̬
		SHUTDOWN = 1,	//�����������񣬴���ʣ������
		STOP = 2,	//�����������񣬲���������
		TIDYING = 3,	//��������Ϊ��
		TERMINATED = 4	//���������߳�
	};
	std::mutex m_mtx_task;			//�����߳�ʹ����ȡ����Ļ�����
	std::mutex m_mtx_work;			//�����̶߳��еĻ�����
	std::mutex m_mtx_set;			//�������õĻ�����
	std::condition_variable m_cond_work;	//�����߳�ʹ�õ���������
	std::condition_variable m_cond_ctl;	//�����߳�ʹ�õ���������
private:
	std::thread* m_ctl_pthread;		//ָ�� �����߳� ��ָ��
	std::atomic<int> m_state;		//�洢�̳߳ص�ǰ״̬
	std::atomic<int> m_reclaimed;		//����ָ����ǰ�ɻ��յĿ����߳�
	std::atomic<bool> m_terminate;		//�սῪ�أ��� terminate() ����
	std::atomic<bool> m_resize;		//�����߳������Ŀ��أ��� set_core_threads() ����
	std::atomic<uint32_t> m_core_threads;	//��פ�ĺ��Ĺ����߳�������Ĭ�� 1
	std::atomic<uint32_t> m_max_threads;	//����߳�������Ĭ��Ϊ��ǰӲ��֧���߳���
	std::atomic<double> m_time_monitor;	//�����߳�ѭ������ʱ��������λ ��
private:
	std::list<std::thread::id> m_works;		//�����̵߳�����
	std::queue<std::function<void()>> m_taskq;	//�������
	std::atomic<int64_t> m_work_max_cost;		//ÿִ��һ�ι��������ʱ�仨�ѣ���λ ����
private:
	//���캯����ɾ���� ���ƹ��캯�� �� ��ֵ���������ֹ�������󱻿���
	threadpool();
	threadpool(const threadpool&) = delete;
	threadpool& operator=(const threadpool&) = delete;	
	void add_thread();	//������̵߳� �ڲ����� 
	void run_ctl();		//�����߳�ִ�е� ѭ������  
	void run_work();	//�����߳�ִ�е� ѭ������ 
	void terminated();	//�սắ������ shutdown() �� shutdown_now() ����
public:
	//��������
	inline virtual ~threadpool() { 
		if(m_state.load()!=TERMINATED)shutdown_now();
	}

	//����ģʽ��ȡ���廯�Ĺ�����Ա����  
	inline static threadpool& get() {
		static threadpool instance;
		return instance;
	}

	//��ʼ��������Ա����������ִ�й���ǰ���� ���Ĺ����߳������������̼߳�ؼ��������߳���
	void init(uint32_t core_threads = static_cast<uint32_t>(1), 
		double timeToMonitor = static_cast<double>(3),
		uint32_t max_threads = std::thread::hardware_concurrency());

	//�̳߳عرպ�����ִ�к�ܾ��������񣬲����ڴ����������ִ���˳�
	inline void shutdown() {
		std::unique_lock<std::mutex>lock(m_mtx_set);
		m_state.store(SHUTDOWN);
		lock.unlock();
		terminated();
	}

	//�̳߳عرպ�����ִ�к�ܾ��������񣬲��ҷ����������������˳�
	inline void shutdown_now(){
		std::unique_lock<std::mutex>lock(m_mtx_set);
		m_state.store(STOP);
		lock.unlock();
		terminated();
	}

	//������� ģ�庯��
	template<class F,class... Args>
	auto enqueue(F&& f, Args&& ... args) -> std::future<std::invoke_result_t<F, Args...>>;

	//��ȡ��ǰ���е� �����߳� ������ע������Ψһ�Ĺ����߳�
	inline size_t get_thread_num()const {
		return m_works.size();
	}

	//��������߳�����
	void set_max_threads(uint32_t num);

	//���ú��Ĺ����߳�����
	void set_core_threads(uint32_t num);

	//���ù����߳���ѯ���
	inline void set_monitor_time(double time) {//(seconds)
		std::unique_lock<std::mutex>lock(m_mtx_set);
		m_time_monitor.store(time);
	}

	//�̳߳�����������������Ҫ���̳߳���ȫ�˳� state==TERMINATED �²���Ч
	void restart();

	//��ȡ��ǰ�����б�δ��ȡ�ߵĹ�������
	inline bool get_task_num()const{
		return m_taskq.size();
	}

};
```
## ImplementFile - "threadpool.cpp"
&emsp;����ο� `threadpool.cpp`
 
