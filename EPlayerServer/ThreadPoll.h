#pragma once
#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"
#include <vector>

class CThreadPoll {
public:
	CThreadPoll() {
		m_server = NULL;//套接字指针置空
		timespec tp = { 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf = NULL;
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000 , tp.tv_nsec % 1000000);
		if (buf != NULL) {
			m_path = buf;
			free(buf);
		}//有问题的话，在start接口里判断m_path解决问题
		usleep(1);//防止其他线程池所用本地套接字文件重合
	};
	~CThreadPoll() {
		Close();
	}
	CThreadPoll(const CThreadPoll&) = delete;//禁止复制
	CThreadPoll& operator=(const CThreadPoll&) = delete;//禁止赋值

	int Start(unsigned count) {
		if (m_server != NULL) return -1;//已经初始化了
		if (m_path.size() == 0) return -2;//构造函数失败
		m_server = new CSocket();
		if (m_server == NULL) return -3;
		int ret = m_server->Init(CSocketParam(m_path, SOCK_ISSERVER));
		if (ret != 0) return -4;
		ret = m_epoll.Create(count);
		if (ret != 0) return -5;
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
		if (ret != 0) return -6;
		m_threads.resize(count);
		for (unsigned i = 0; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPoll::TaskDispatch, this);
			if (m_threads[i] == NULL) return -7;
			ret = m_threads[i]->Start();//启动线程
			if (ret != 0) return -8;//启动失败
		}
		return 0;
	}
	void Close() {//线程池结束和初始化失败时错误处理
		m_epoll.Close();
		if (m_server) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		for (auto thread : m_threads) {
			if (thread != NULL) delete thread;
		}
		m_threads.clear();
		unlink(m_path);
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_... args) {
		static thread_local CSocket client;//thread_local为C++11后的新特性，对于每个线程对象不一样（给每个线程分配不同的客户端）
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSocketParam(m_path, 0));
			if (ret != 0) return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);//模板函数已构造
		if (base == NULL) return -3;
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));//将指针地址赋值给data（而不是指针指向的对象所在地址）
		ret = client.Send(data);
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
	size_t Size() const { return m_threads.size(); };
private:
	int TaskDispatch() {
		while (m_epoll != -1) {
			EPEvents events;
			int ret = 0;
			ssize_t esize = m_epoll.WaitEvents(events);
			if (esize > 0) {
				for (ssize_t i = 0; i < esize; i++) {
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) {//服务器套接字收到输入，有客户端来连接
							ret = m_server->Link(&pClient);
							if (ret != 0) continue;
							m_epoll.Add(*pClient, EpollData((void*)pClient));
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else {//客户端套接字收到输入（收到数据）
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient != NULL) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data);
								if (ret <= 0) {//读取失败
									m_epoll.Del(*pClient);
									delete pClient;
									continue;
								}
								memcpy(&base,(char*)data, sizeof(base));
								if (base != NULL) {
									(*base)();//进行任务函数的调用
									delete base;//任务执行结束，释放
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}
	
	CEpoll m_epoll;
	std::vector<CThread*> m_threads;//为什么是线程指针数组而不是线程对象数组?答：要放到容器里的对象一定要有默认构造函数和复制构造函数，线程没有复制构造函数，因此不能直接放入容器内，因此只能放入指针。
	CSocketBase* m_server;
	Buffer m_path;//本地链接（会创建一个尾文件，大小一直是0），本地套接字的内核对象在文件中的一个显现
};