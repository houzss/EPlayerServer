#pragma once
#include <unistd.h>//linux通用数据类型
#include <pthread.h>
#include <fcntl.h>//权限类型、文件操作类型的宏定义
#include "Function.h"
#include <cstdio>
#include <signal.h>
#include <cerrno>
#include <map>
class CThread
{
public:
	CThread() {
		m_function = NULL;
		m_thread = 0;//unsigned long
		m_bpaused = false;
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	CThread(_FUNCTION_ func, _ARGS_... args) 
		:m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...))
	{
		m_thread = 0;
		m_bpaused = false;
	}
	~CThread() {}
	//禁止复制拷贝
	CThread(const CThread&) = delete;
	CThread operator=(const CThread&) = delete;

	template<typename _FUNCTION_, typename... _ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args)
	{
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_function == NULL) return -1;
		return 0;
	}
	int Start()
	{
		pthread_attr_t attr;
		int ret = 0;
		ret = pthread_attr_init(&attr);
		if (ret != 0) return -1;
		//linux线程执行和windows不同，pthread有两种状态joinable状态和unjoinable状态，如果线程是joinable状态，当线程函数自己返回退出时或pthread_exit时都不会释放线程所占用堆栈和线程描述符（总计8K多）。只有当你调用了pthread_join之后这些资源才会被释放。若是unjoinable状态的线程，这些资源在线程函数退出时或pthread_exit时自动会被释放。
		//unjoinable属性可以在pthread_create时指定，或在线程创建后在线程中pthread_detach自己, 如：pthread_detach(pthread_self())，将状态改为unjoinable状态，确保资源的释放。或者将线程置为 joinable,然后适时调用pthread_join.
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);//当结束时创建的线程状态即为Joinable，以应对要Stop程序时线程仍未结束要join等待其结束的情况
		if (ret != 0) return -2;
		//ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);//默认，可不设置（设置竞争范围在进程内部而不是全局/整个系统所有进程，若和全局进程的其他线程竞争并不靠谱）
		//if (ret != 0) return -3;
		ret = pthread_create(&m_thread,&attr,&CThread::ThreadEntry, this);//这里传入的this为CThread类对象的this指针,并非m_function对应类的指针(这个指针已经被传入函数参数内部了)
		//pthread_create的线程函数必须是静态函数的原因: void *(*__start_routine) (void *) <=> void *ThreadFunc(void *args); 对于普通类成员函数、虚函数，他们实际上都是包含了调用他们的对象的 this 指针，即：经过编译器优化，线程函数变为：void *（类名）+ ThreadFunc(this, void *args);
		//这就导致了该函数的格式是不符合 pthread_create() 对线程函数的要求的。故，如果类成员函数做为线程函数时，必须是静态的。
		//费尽心机先去静态再去非静态的原因：为了使用一些成员属性和一些成员方法（如果一直要使用指针就比较担惊受怕，不太合适）
		//因为是类内公有成员函数通过类名调用私有静态成员函数，可行（但是因为静态成员函数没有this指针，只能传入this作为参数，然后通过thiz=this,thiz->nonStaticFunc()指针调用另一个函数实现从静态到非静态）
		if (ret != 0) return -4;
		m_mapThread[m_thread] = this;
		ret = pthread_attr_destroy(&attr);
		if (ret != 0) return -5;
		return 0;
	}
	int Pause() {
		if (m_thread != 0) return -1;//线程无效
		if (m_bpaused == true) {
			m_bpaused = false;
			return 0;
		}
		m_bpaused = true;//因为赋值操作比较快，所以应先赋值（这样子线程开始等待暂停信号时也能立刻看到这个bool变量已经切换成表示线程被暂停）
		int ret = pthread_kill(m_thread, SIGUSR1);//给线程发送暂停信号
		if (ret != 0) {
			m_bpaused = false;
			return -2;
		}
		return 0;
	}
	int Stop() {
		if (m_thread != 0) {//若线程存在
			pthread_t thread = m_thread;//等待线程让它结束
			m_thread = 0;
			timespec ts;
			ts.tv_sec = 0;//秒
			ts.tv_nsec = 100 * 1000000;//纳秒(将其设置为100毫秒)
			int ret = pthread_timedjoin_np(thread, NULL, &ts);//pthread_join()即是子线程合入主线程，主线程阻塞等待子线程结束，然后回收子线程资源。
			if (ret == ETIMEDOUT) {//超时
				pthread_detach(thread);//将线程改为unjoinable状态，这样就不需要像pthread_join()一样给线程“擦屁股”，关闭线程即可释放线程资源。
				pthread_kill(thread, SIGUSR2);//给线程发送退出信号，因为上一步已将线程改为unjoinable，所以会直接释放资源（堆栈和描述符）
			}
		}
		return 0;
	}
	bool isValid() const { return m_thread != 0; }
private:
	CFunctionBase* m_function;
	pthread_t m_thread;
	bool m_bpaused;//true表示暂停 false表示运行中
	static std::map<pthread_t, CThread*> m_mapThread;//得是静态的，不然在Sigaction无法使用，并且在外部初始化
	static void* ThreadEntry(void* arg)//_stdcall标准调用 静态成员函数属于类 而不属于对象，若是公有成员函数，可以通过类名称直接访问
	{
		CThread* thiz = (CThread*)arg;//静态函数没有this指针，只能取别名
		struct sigaction act = { 0 };//既是函数又是结构体
		sigemptyset(&act.sa_mask);//清空掩码，防止被屏蔽
		act.sa_flags = SA_SIGINFO;//使得回调函数可设置
		act.sa_sigaction = &CThread::Sigaction;
		sigaction(SIGUSR1, &act, NULL);//用于暂停
		sigaction(SIGUSR2, &act, NULL);//用于接收上面Stop（）函数的杀死信号（SIGUSR为用户DIY定义表示含义的信号量，只有1和2）

		thiz->EnterThread();

		if (thiz->m_thread) thiz->m_thread = 0;//不为0则置零
		pthread_t thread = pthread_self();//不是不想用thiz->m_thread，有可能thiz->m_thread为空（被stop函数清零了）
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL;
		pthread_detach(thread);//改为unjoinable，当线程结束运行后自我销毁（立即释放占用资源）
		pthread_exit(NULL);//退出线程
	}
	static void Sigaction(int signo,siginfo_t* info,void* context)//signo：signal_number，info：传进去的断点信息 context：上下文
	{
		if (signo == SIGUSR1) {//线程暂停
			pthread_t thread = pthread_self();
			auto it = m_mapThread.find(thread);
			if (it != m_mapThread.end()) {//map中存在
				if (it->second!=NULL) {
					it->second->m_bpaused = true;
					while (it->second->m_bpaused) {
						if (it->second->m_thread == 0) pthread_exit(NULL);//立刻停止
						usleep(1000);//每休眠1ms查询状态是否ok
					}
				}
			}
		}
		else if (signo == SIGUSR2) {//线程退出
			pthread_exit(NULL);
		}
	}
	void EnterThread()//__thiscall（指针作为隐式参数（对象调用））
	{
		if (m_function != NULL) {
			int ret = (*m_function)();//这是m_function()函数参数已带上原来传入的函数参数
			//1.函数属于类内私有非静态成员函数,那么一定需要this指针找到对应对象对应的成员函数
			//2.若函数需要访问类内私有成员,肯定会传入this指针,即可调用原来类内的私有成员变量/函数)
			if (ret != 0) {
				printf("%s(%d):[%s] ret = %d\n", __FILE__, __LINE__, __FUNCTION__,ret);
			}
		}
	}
};