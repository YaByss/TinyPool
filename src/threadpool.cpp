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
	//���һ��Ĭ�ϵ� ����߳�
	m_ctl_pthread = new std::thread(&threadpool::run_ctl, this);
#ifdef debug
	std::clog << "Threadpool Constructed." << std::endl;
#endif
	m_state.store(RUNNING);
}

//����߳�
//��Ҫ�� m_mtx_work �ĺ����ſ���ִ��
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

//�����߳�ѭ������
//�����̣߳�Ĭ��һ���߳����ڼල��������Ƿ񴴽��������߳���Դ
//����������¸��߳̽��� wait
//���߳�Ϊ�̳߳س�ʼ��ʱ�����ĵ�һ���̣߳����߳� id ������ works �б�
void threadpool::run_ctl()
{
#ifdef debug
	std::clog << "Controller thread constructed." << std::endl;
#endif
	//��ʼ�������߳�ʱ�������㹻������ ��פ�����߳� core_thread
	for (size_t i = 0; i < m_core_threads.load(); i++)
		add_thread();
	int tasknum = 0;
	while (1)
	{
		std::unique_lock<std::mutex>lock_set(m_mtx_set);
		//����������
		//ÿ�� �̶�ѭ��ʱ�� m_time_monitor.load() �� ����
		//m_terminate ���ѣ��� terminate() ���ѣ�׼��ִ���˳�
		//m_resize ���ѣ��� resize() ���ѣ������߳�����
		// m_taskq.size() > tasknum; �������б���������ʱ���� enqueue ���ѣ������Ƿ������߳�
		m_cond_ctl.wait_for(lock_set, m_time_monitor.load() * 1000ms, [&] {return m_terminate || m_resize || m_taskq.size() > tasknum; });
		
		//resize ����
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
			{//�����ǰ�����������߳̽��м���
				m_reclaimed.store(static_cast<int>(m_works.size() - m_core_threads.load()));
				m_cond_work.notify_all();
				while (m_reclaimed.load());//�ȴ������߳�����
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


		//��������û�й����̵߳�����£������߳�֪ͨ
		if (!m_works.size() and !m_taskq.empty()) {
#ifdef debug
			std::clog << "There is no work thread now,creating..." << std::endl;
#endif
			add_thread();
			m_cond_work.notify_one();
			continue;
		}

		if (!m_terminate)
		{//enqueue ����
#ifdef debug
			std::clog << "ctl thread monitoring.." << std::endl;
#endif
			if (m_works.size() < m_max_threads.load() and !m_taskq.empty() and m_work_max_cost!=0)
			{//����ѻ��������ǰ�߳� < ����߳������������������ʱ�Ѹ��£���������* 
				//����Ӧ�����ӵ��߳���,�������߳�
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
			{//�������� < �߳����������ն������ʱ�߳�
#ifdef debug
				std::clog << "Work Free,reclaiming thread..." << std::endl;
#endif
				m_reclaimed.store(m_reclaimed.load() + 1);
				m_cond_work.notify_one();
				while (m_reclaimed.load());//�ȴ����ս���
			}
		}
		else {//terminate ���� m_terminate = true
#ifdef debug
			std::clog << "Terminate rouse ctl thread." << std::endl;
#endif
			m_reclaimed.store(0);
			if (m_state.load() == SHUTDOWN){//�ȴ�����ִ��
#ifdef debug
				std::clog << "ctl thread: SHUTDOWN..." << std::endl;
#endif
				m_state.store(TIDYING);
				while (!m_taskq.empty());//�ȴ��������
				m_state.store(STOP);
			}
			if(m_state.load()==STOP){//ֱ�ӻ���ȫ���߳�
				m_cond_work.notify_all();
				while (!m_works.empty());//�ȴ��߳�ȫ������
				m_state.store(TERMINATED);
#ifdef debug
				std::clog << "state -> TERMINATED\nctl thread TID-"<<std::this_thread::get_id()<<" exit." << std::endl;
#endif
				return;
			}
			//m_terminated ״̬�²�Ӧ�����е��˴�
			abort();
		}
		tasknum = m_taskq.size();
	}//while
	return;
}


//�߳���Ҫѭ������
void threadpool::run_work()
{
	while (1)
	{
		std::unique_lock<std::mutex>lock_task(m_mtx_task);
		m_cond_work.wait(lock_task,
			[&] {return m_terminate || !m_taskq.empty() || m_reclaimed; });

		//ֹͣ�������������˳����� shutdown_now() ����
		if (m_terminate and (m_state == STOP))
		{
#ifdef debug
			std::clog << "work thread TID-"<<std::this_thread::get_id()<<": terminated STOP" << std::endl;
#endif
			std::unique_lock<std::mutex>lock_work(m_mtx_work);
			m_works.remove(std::this_thread::get_id());
			return;
		}

		//�ж� m_taskq.empty()
		if (!m_taskq.empty()) {
			auto TimePoint = std::chrono::high_resolution_clock::now();
			std::function<void()>task;
			task = std::move(m_taskq.front());
			m_taskq.pop();
			//���ȡ������������ͷ���
			lock_task.unlock();
			//ִ������
			task();
			//��¼���ִ������������ʱ��
			auto duration = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count()
				- std::chrono::time_point_cast<std::chrono::milliseconds>(TimePoint).time_since_epoch().count();
			if (duration > m_work_max_cost.load())
				m_work_max_cost.store(duration);
		}

		//�鿴��ǰԤ�ƻ����߳������ж��Ƿ��˳�
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


//���������̣߳��� shutdown  shutdown_now �� �������� ����
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
//���� ��ʱʱ��>1 ���ĳ�פ�߳���>1  ����߳���>core_threads 
void threadpool::init(uint32_t core_threads, double timeToMonitor, uint32_t max_threads) {
#ifdef debug
	std::clog << "threadpool:init()" << std::endl;
#endif
	m_time_monitor.store(timeToMonitor);
	set_max_threads(max_threads);
	set_core_threads(core_threads);
}

//��������߳���
void threadpool::set_max_threads(uint32_t num)
{
#ifdef debug
	std::clog << "Set max threads to " << num << std::endl;
#endif
	m_max_threads.store(num);
	if (num < m_core_threads)
		set_core_threads(num);
}

//���� ���ĳ�פ�߳��� ��СֵΪ 1 
void threadpool::set_core_threads(uint32_t num)
{
#ifdef debug
	std::clog << "Set core threads to " << num << std::endl;
#endif
	std::unique_lock<std::mutex>lock(m_mtx_set);
	//�����߳��� ���� ����߳�����ͬ���޸�����߳���
	if (num > m_max_threads)
		m_max_threads.store(num);
	//֪ͨ ctl �߳��޸�
	m_core_threads.store(num);
	m_resize.store(true);
	m_cond_ctl.notify_one();
}

//�̳߳ؽ��� TERMINATED �� �����̳߳�
void threadpool::restart()
{//ֻ���� TERMINATED ������
#ifdef debug
	std::clog << "Restarting..." << std::endl;
	if (m_state != TERMINATED)
		std::clog << "threadpool is running, restarting failed." << std::endl;
#endif
	if (m_state != TERMINATED)return;
	std::unique_lock<std::mutex>lock(m_mtx_set);
	while (m_taskq.size())//��������б�
		m_taskq.pop();
	m_terminate.store(false);
	m_work_max_cost.store(0LL);
	m_ctl_pthread = new std::thread(&threadpool::run_ctl, this);
	m_state.store(RUNNING);
#ifdef debug
	std::clog << "threadpool: restarted." << std::endl;
#endif
}