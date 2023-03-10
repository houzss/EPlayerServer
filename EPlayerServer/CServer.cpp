#include "CServer.h"
#include "Logger.h"
CServer::CServer()
{
	m_server = NULL;
	m_business = NULL;
}

CServer::~CServer()
{
	Close();
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port, unsigned thread_count)
{
	int ret = 0;
	if (business == NULL) return -1;//没有业务,直接结束
	m_business = business;
	ret = m_process.setEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process);
	if (ret != 0) return -2;
	ret = m_process.CreateSubProcess();
	if (ret != 0) return -3;
	ret = m_tpoll.Start(thread_count);//线程池
	if (ret != 0) return -4;
	ret = m_epoll.Create(thread_count);
	if (ret != 0) return -5;
	m_server = new CSocket();
	if (m_server == NULL) return -6;
	ret = m_server->Init(CSocketParam(ip, port, SOCK_ISSERVER | SOCK_ISIP| SOCK_ISREUSE));//TODO:这里报错Invalid Argument
	if (ret != 0) {
		printf("errno:%d msg:%s\n", errno, strerror(errno));
		return -7;
	}
	ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
	if (ret != 0) return -8;
	for (size_t i = 0; i < m_tpoll.Size(); i++) {
		ret = m_tpoll.AddTask(&CServer::ThreadFunc, this);////thiscall,所以需要附上this
		if (ret != 0) return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != NULL) {
		usleep(10);//10us = 1/100ms
	}
	return 0;
}

int CServer::Close()
{
	if(m_server){
		CSocketBase* sock = m_server;
		m_server = NULL;
		m_epoll.Del(*sock);
		delete sock;
	}
	m_epoll.Close();
	m_process.sendFD(-1);
	m_tpoll.Close();
	//if (m_business != NULL) {
	//	delete m_business;//自己加的,如果后面的业务逻辑发生bug记得看这里
	//}
	return 0;
}

int CServer::ThreadFunc()
{
	TRACEI("epoll: %d, server %p", (int)m_epoll, m_server);
	int ret = 0;
	EPEvents events;
	while ((m_epoll != -1) && (m_server != NULL)) {
		ssize_t size = m_epoll.WaitEvents(events, 500);
		if (size < 0) break;
		if (size > 0) {
			TRACEI("ThreadFunc-size=%d event: %08X", size, events[0].events);
			for (ssize_t i = 0; i < size; i++) {
				if (events[i].events & EPOLLERR) {
					break;//主进程根本不会处理客户端网络的收发,客户端对象会发给子进程去,这里没有用,所以直接结束循环即可
					//if (events[i].data.ptr != m_server) {//客户端出错,需要移除
					//	CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
					//	if (pClient != NULL) {
					//		m_epoll.Del(*pClient);
					//		delete pClient;
					//		continue;
					//	}
					//}//服务器错误不用管
				}
				else if (events[i].events & EPOLLIN) {
					if (m_server != NULL) {
						CSocketBase* pClient = NULL;
						ret = m_server->Link(&pClient);
						if (ret != 0) continue;
						ret = m_process.sendSocket(*pClient,*pClient);
						TRACEI("Send Socket res: %d", ret);
						int s = *pClient;
						delete pClient;
						if (ret != 0) {
							TRACEE("send client %d failed", s);
							continue;
						}
					}
				}
			}
		}
	}
	TRACEI("ThreadFunc-服务器终止");
	return 0;
}
