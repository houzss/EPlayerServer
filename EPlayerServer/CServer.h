#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPoll.h"
#include "Process.h"

template<typename _FUNCTION_, typename... _ARGS_>
class CConnectedFunction : public CFunctionBase {
public:
	CConnectedFunction(_FUNCTION_ func, _ARGS_... args)//这里的func一定是外面传来的函数所在地址,所以外面的func传入都是带上&func(取地址),因为不知道这个函数对应的对象,传入复制的类成员函数没有道理(不知道对应哪个对象,不能用)
		:m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)//forward表示原样转发，()...表示参数展开 std::forward<T>(u)有两个参数：T 与 u。当T为左值引用类型时，u将被转换为T类型的左值，否则u将被转换为T类型右值。如此定义std::forward是为了在使用右值引用参数的函数模板中解决参数的完美转发问题。
		//bind绑定类成员函数时，第一个参数表示对象的成员函数的指针(必须显式地指定&,因为编译器不会将对象的成员函数隐式转换成函数指针)，第二个参数表示对象的地址(使用对象成员函数的指针时，必须要知道该指针属于哪个对象，因此第二个参数为对象的地址 &base)。
	{}//上面m_binder前面的冒号代表初始化
	virtual ~CConnectedFunction() {}
	virtual int operator()(CSocketBase* pClient) {
		return m_binder(pClient);
	}
	//std::bind用于给一个可调用对象绑定参数。可调用对象包括函数对象（仿函数）、函数指针、函数引用、成员函数指针和数据成员指针。
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;//相当于反
};

template<typename _FUNCTION_, typename... _ARGS_>
class CReceivedFunction : public CFunctionBase {
public:
	CReceivedFunction(_FUNCTION_ func, _ARGS_... args)//这里的func一定是外面传来的函数所在地址,所以外面的func传入都是带上&func(取地址),因为不知道这个函数对应的对象,传入复制的类成员函数没有道理(不知道对应哪个对象,不能用)
		:m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)//forward表示原样转发，()...表示参数展开 std::forward<T>(u)有两个参数：T 与 u。当T为左值引用类型时，u将被转换为T类型的左值，否则u将被转换为T类型右值。如此定义std::forward是为了在使用右值引用参数的函数模板中解决参数的完美转发问题。
		//bind绑定类成员函数时，第一个参数表示对象的成员函数的指针(必须显式地指定&,因为编译器不会将对象的成员函数隐式转换成函数指针)，第二个参数表示对象的地址(使用对象成员函数的指针时，必须要知道该指针属于哪个对象，因此第二个参数为对象的地址 &base)。
	{}//上面m_binder前面的冒号代表初始化
	virtual ~CReceivedFunction() {}
	virtual int operator()(CSocketBase* pClient, const Buffer& data) {
		return m_binder(pClient, data);
	}
	//std::bind用于给一个可调用对象绑定参数。可调用对象包括函数对象（仿函数）、函数指针、函数引用、成员函数指针和数据成员指针。
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;//相当于反传
};


class CBusiness {//用于处理业务的类(抽象类)
public:
	CBusiness() : 
		m_connectedcallback(NULL), 
		m_recvcallback(NULL){}
	virtual int BusinessProcess(CProcess* proc) = 0;//纯虚函数
	template<typename _FUNCTION_, typename... _ARGS_>
	int setConnectedCallback(_FUNCTION_ func, _ARGS_... args)
	{
		//m_connectedcallback = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		m_connectedcallback = new CConnectedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_connectedcallback == NULL) return -1;//设置失败
		return 0;//设置成功
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	int setRecvCallback(_FUNCTION_ func, _ARGS_... args)
	{
		//m_recvcallback = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		m_recvcallback = new CReceivedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_recvcallback == NULL) return -1;//设置失败
		return 0;//设置成功
	}
protected:
	CFunctionBase* m_connectedcallback;//连接成功回调函数
	CFunctionBase* m_recvcallback;//接收成功回调函数
};

class CServer//1.开子进程(客户端处理进程)  
{
public:
	CServer();
	~CServer();
	CServer(const CServer&) = delete;//包含了进程\线程等,不能被复制拷贝
	CServer& operator=(const CServer&) = delete;
	int Init(CBusiness* business, const Buffer& ip = "0.0.0.0", short port = 9999, unsigned thread_count = 2);
	int Run();
	int Close();

	
private:
	CThreadPoll m_tpoll;//线程池
	CSocketBase* m_server;
	CEpoll m_epoll;//epoll池(主要用来多线程接入客户端)
	CProcess m_process;//用于开进程
	CBusiness* m_business;//业务模块, 需要手动delete
	int ThreadFunc();//运行关键逻辑
};

