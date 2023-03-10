#ifndef EpollD
#define EpollD
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <errno.h>
#include <sys/signal.h>
#include <memory.h>
#define EVENT_SIZE 128
class EpollData
{
public:
	//构造函数
	EpollData() { m_data.u64 = 0; }
	EpollData(void* ptr) { m_data.ptr = ptr; }
	explicit EpollData(int fd) { m_data.fd = fd; }//explicit显式调用，必须严格控制参数为整数，防止出现意料之外的问题(如防止构造函数被用来隐式类型转换)
	explicit EpollData(uint32_t u32) { m_data.u32 = u32; }
	explicit EpollData(uint64_t u64) { m_data.u64 = u64; }
	//允许它进行复制构造
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; }//u64占用位数最大，只要u64复制了其他都会被复制
	//运算符重载
	EpollData& operator=(const EpollData& data) {
		if (this != &data)
			m_data.u64 = data.m_data.u64;
		return *this;
	}
	EpollData& operator=(void* data) {
		m_data.ptr = data;
		return *this;
	}
	EpollData& operator=(int data) {
		m_data.fd = data;
		return *this;
	}
	EpollData& operator=(uint32_t data) {
		m_data.u32 = data;
		return *this;
	}
	EpollData& operator=(uint64_t data) {
		m_data.u64 = data;
		return *this;
	}
	operator epoll_data_t() { return m_data; }//强制类型转换就起效了
	operator epoll_data_t()const { return m_data; }//当整个EpollData构造的常量对象也可以发挥作用
	operator epoll_data_t* () { return &m_data; }
	operator const epoll_data_t* ()const { return &m_data; }//返回指针
private:
	epoll_data_t m_data;//联合体union，只能存储联合体中的一个数据
};
using EPEvents = std::vector<epoll_event>;//C++11:using可用作别名
class CEpoll
{
public:
	CEpoll() {
		m_epoll = -1;
	}
	~CEpoll() {
		Close();
	}
	CEpoll(const CEpoll&) = delete;
	CEpoll& operator = (const CEpoll&) = delete;
	operator int()const { return m_epoll; }//重载运算符，提取返回epoll描述符 前面的const表示该函数不会修改成员变量

	int Create(unsigned count) {
		if (m_epoll != -1) return -1;//已创建过
		m_epoll = epoll_create(count);//epollcreate 负责创建一个池子，一个监控和管理句柄 fd 的池子；
		if (m_epoll == -1) return -2;
		return 0;
	}
	ssize_t WaitEvents(EPEvents& events, int timeout = 10) {
		if (m_epoll == -1) return -1;//未初始化
		EPEvents evs(EVENT_SIZE);
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout);//等待事件 epollwait 就是负责打盹的，让出 CPU 调度，但是只要有“事”，立马会从这里唤醒；
		if (ret == -1) {
			if ((errno == EINTR) || (errno == EAGAIN)) {//EINTR中断消息，EAGAIN可能在非阻塞情况下出现
				return 0;//表明没读到任何数据
			}
			return -2;
		}
		if (ret > (int)events.size()) {//有更多消息发生了
			events.resize(ret);
		}
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret);//若事件发生了则传输参数
		return ret;
	}
	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) {
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);//epollctl 负责管理这个池子里的 fd 增、删、改；
		if (ret == -1) return -2;
		return 0;
	}
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0)) {
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);//epollctl 负责管理这个池子里的 fd 增、删、改；
		if (ret == -1) return -2;
		return 0;
	}
	int Del(int fd) {
		if (m_epoll == -1) return -1;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);//不需要事件，留空即可
		if (ret == -1) return -2;
		return 0;
	}
	void Close() {
		if (m_epoll != -1) {
			int fd = m_epoll;
			m_epoll = -1;
			//前两行为快速操作（指令级别赋值，在多线程防止多个线程同时对m_epoll进行关闭/赋值/修改等操作）防御性编程
			close(fd);
		}
	}
private:
	int m_epoll;
};

#endif // !EpollD
