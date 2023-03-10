#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <map>
#include <sys/timeb.h>
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

enum LogLevel {
	LOG_INFO,//信息
	LOG_DEBUG,//调试
	LOG_WARNING,//警告
	LOG_ERROR,//错误
	LOG_FATAL//致命错误
};

class LogInfo {
public:
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt, ...);//const char* fmt,...为可变参数，fmt为可变参数格式，...包含了所有数据
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		void* pData, size_t nSize);
	
	~LogInfo();
	operator Buffer() const{
		return m_buf;
	}
	template<typename T>
	LogInfo& operator<<(const T& data) {//返回值还是自己才能连续使用<<运算符
		std::stringstream stream;
		stream << data;
		//printf("%s(%d):[%s][%s]\n", __FILE__, __LINE__, __FUNCTION__, stream.str().c_str());
		m_buf += stream.str().c_str();
		//printf("%s(%d):[%s][%s]\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_buf);
		return *this;
	}
private:
	bool bAuto;//默认是false 流式日志，则为true
	Buffer m_buf;
};

class CLoggerServer//不应该被复制
{
public://对外接口
	CLoggerServer()
		:m_thread(&CLoggerServer::ThreadFunc, this)
	{
		m_server = NULL;
		char curpath[256] = "";
		getcwd(curpath, sizeof(curpath));
		m_path = curpath;
		m_path += "/log/" + GetTimeStr() + ".log";//日志文件名
		printf("%s(%d):[%s]path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
	}
	~CLoggerServer() {
		Close();
	}
	
	CLoggerServer(const CLoggerServer&) = delete;//不能复制
	CLoggerServer& operator=(const CLoggerServer&) = delete;//不能赋值

	int Start(){
		if (m_server != NULL) return -1;//在start前已经创建，直接报错退出
		if (access("log", W_OK | R_OK) != 0) {//查看对这个文件夹是否有读和写的权限，若不等于0说明文件夹不存在
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);//当前用户读，当前用户写，用户组读，用户组写，其他用户读
		}
		m_file = fopen(m_path, "w+");//w+:打开的文件已存在时，将覆盖原文件
		if (m_file == NULL) return -2;//文件打开失败
		int ret = m_epoll.Create(1);//创建Epoll
		if (ret != 0) return -3;
		m_server = new CSocket();
		if (m_server == NULL) {
			Close();
			return -4;//初始化套接字对象失败
		}
		ret = m_server->Init(CSocketParam("./log/server.sock", (int)SOCK_ISSERVER| SOCK_ISREUSE));
		if (ret != 0) {
			Close();
			//printf("%s(%d):<%s> pid = %d ret:%d errno:%d msg:%s\n", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
			return -5;//创建本地套接字失败
		}
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);//将日志服务器的文件描述符增加到Epoll池中，开启Epoll池对日志服务器的IO管理（输入和错误信号的监听）
		if (ret != 0) {
			Close();
			return -6;
		}
		ret = m_thread.Start();
		if (ret != 0) {
			printf("%s(%d):<%s> pid = %d ret:%d errno:%d msg:%s\n", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
			Close();
			return -7;//线程开始运行失败
		}
		return 0;
	}
	int Close(){
		if (m_server != NULL) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		m_epoll.Close();
		m_thread.Stop();
		return 0;
	}
	//给其他非日志进程的进程和线程使用的
	static void Trace(const LogInfo& info) {//日志数据有三种：内存导出方式、标准输入输出方式、print方式
		int ret = 0;
		static thread_local CSocket client;//1、静态的本地套接字对象（不会随着每次函数调用重新初始化/发生改变）2、属于线程，每个线程调用这个函数都会重新调用构造函数（因而每个线程都对应一个本地套接字对象[线程之间不共享]）
		if (client == -1) {
			ret = client.Init(CSocketParam("./log/server.sock", 0));
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link();
			printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
		}
		ret = client.Send(info);
		printf("%s(%d):[%s]ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
	}
	static Buffer GetTimeStr() {//获取格式化时间
		Buffer result(128);
		timeb tmb;
		ftime(&tmb);//拿到毫秒单位的时间
		tm* pTm = localtime(&tmb.time);
		int nSize = snprintf(result, result.size(),
			"%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec,
			tmb.millitm
		);
		result.resize(nSize);
		return result;
	}
private:
	CThread m_thread;
	CEpoll m_epoll;
	CSocketBase* m_server;
	Buffer m_path;
	FILE* m_file;
	int ThreadFunc() {//线程函数应该私有
		printf("%s(%d):[%s] m_thread.isValid(): %d m_epoll:%d server:%p\n", __FILE__, __LINE__, __FUNCTION__, m_thread.isValid(), (int)m_epoll, m_server);
		EPEvents events;
		std::map<int, CSocketBase*> MapClients;
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL)) {
			ssize_t ret = m_epoll.WaitEvents(events, 1000);
			printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret < 0) break;//终止线程，结束
			if (ret > 0) {//==0继续等待即可（表示没读到任何数据）
				ssize_t i = 0;
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) {
						break;//出问题了就要中止
					}
					else if (events[i].events & EPOLLIN) {
						//printf("%s(%d):[%s]ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
						//printf("%s(%d):[%s]m_server=%p\n", __FILE__, __LINE__, __FUNCTION__, m_server);
						if (events[i].data.ptr == m_server) {//服务器收到输入请求(收到由服务端套接字发送来的信息)只有一种情况：有客户端要来连接,此时服务端要accept
							CSocketBase* pClient = NULL;
							int r = m_server->Link(&pClient);//指针的地址，还是地址（肯定非空）
							//printf("%s(%d):[%s]r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) {//套接字有问题，没接受成功
								continue;//不用处理，因为ret>0，客户端delete掉后
							}
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR);//对针对客户端创建的 客户端处理进程 进行监听（加入Epoll池，监听输入报错）
							//注意，传入的*pClient会进入EpollData的构造函数使得 union联合体m_data存储指针变量后返回整个EpollData对象
							//printf("%s(%d):[%s]r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) {
								delete pClient;
								continue;
							}//成功后保存
							auto it = MapClients.find(*pClient);
							if (it != MapClients.end()) {//若已经存在
								if(it->second)	delete it->second;//删除后替换成新的
							}
							MapClients[*pClient] = pClient;//注意，传入的*pClient会自动转换为类对象下的m_socket
							//printf("%s(%d):[%s]r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
						}
						else {//该线程属于客户端，需要读(收到由客户端套接字发送来的日志信息，需要接收并写入日志)
							printf("%s(%d):[%s]ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient != NULL) {//对应客户端肯定已经创建并且Link(connect)连接过（肯定已经经过上面的if流程），故不需要Init和Link，Epoll.add
								Buffer data(1024 * 1024);
								int r2 = pClient->Recv(data);
								printf("%s(%d):[%s]r2=%d\n", __FILE__, __LINE__, __FUNCTION__, r2);
								if (r2 <= 0) {//接收失败
									printf("%s(%d):[%s]r2=%d\n", __FILE__, __LINE__, __FUNCTION__, r2);
									MapClients[*pClient] = NULL;
									//printf("%s(%d):[%s]r2=%d\n", __FILE__, __LINE__, __FUNCTION__, r2);
									delete pClient;
									//printf("%s(%d):[%s]r2=%d\n", __FILE__, __LINE__, __FUNCTION__, r2);
								}
								else {//接收成功
									//printf("%s(%d):[%s]data=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
									WriteLog(data);//写入日志
								}
								//printf("%s(%d):[%s]r2=%d\n", __FILE__, __LINE__, __FUNCTION__, r2);
							}
						}
					}
				}
				if (i != ret) {//提前结束，整个线程需要break掉
					break;
				}
			}
		}
		for (auto it = MapClients.begin(); it != MapClients.end(); it++) {
			if (it->second) {//!= NULL
				delete it->second;//防止内存泄漏
			}
		}
		MapClients.clear();
		return 0;
	}
	void WriteLog(const Buffer& data) {
		if (m_file != NULL) {
			FILE* pFile = m_file;
			fwrite((char*)data, 1, data.size(), pFile);//一次写一个字节，写data.size()次
			fflush(pFile);//清空缓冲区（即把缓冲区应写入文件的内容执行写入操作）
#ifdef _DEBUG
			printf("%s",(char*)data);
#endif
		}
	}
};
#ifndef TRACE
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO,__VA_ARGS__))//前面的...输入后面的__VA_ARGS__可变参数中
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG,__VA_ARGS__))//前面的...输入后面的__VA_ARGS__可变参数中
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING,__VA_ARGS__))//前面的...输入后面的__VA_ARGS__可变参数中
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR,__VA_ARGS__))//前面的...输入后面的__VA_ARGS__可变参数中
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL,__VA_ARGS__))//前面的...输入后面的__VA_ARGS__可变参数中

//LOGI<<"hello"<<"how are you";
#define LOGI LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_INFO)
#define LOGD LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_DEBUG)
#define LOGW LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_WARNING)
#define LOGE LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_ERROR)
#define LOGF LogInfo(__FILE__,__LINE__,__FUNCTION__,getpid(),pthread_self(),LOG_FATAL)
//内存导出，如 00 01 02 03... ; ...a...
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))//data:数据所在地址，size：尺寸
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))//data:数据所在地址，size：尺寸
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))//data:数据所在地址，size：尺寸
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))//data:数据所在地址，size：尺寸
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))//data:数据所在地址，size：尺寸
#endif