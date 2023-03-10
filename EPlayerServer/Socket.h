#ifndef SOCKETD
#define SOCKETD
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>//本地套接字
#include <netinet/in.h>//网络套接字参数、宏
#include <arpa/inet.h>//网络API接口
#include <cerrno>
#include <fcntl.h>
#include "Public.h"

enum SockAttr {//取值一定是2^n次方,以表示这些变量可以在一个整数间同时存在，比如111B=7D表示UDP，非阻塞和服务器
	SOCK_ISSERVER = 1,//是否是服务器（1表示是，0表示为客户端）
	SOCK_ISNONBLOCK = 2,//是否非阻塞（1表示非阻塞，0表示阻塞）
	SOCK_ISUDP = 4,//是否是UDP 1表示UDP 0表示TCP
	SOCK_ISIP = 8,//是否是网络套接字 1表示网络套接字（IP协议），0表示本地套接字
	SOCK_ISREUSE = 16,//是否重用了地址
};

class CSocketParam
{
public:
	CSocketParam(){
		bzero(&addr_in, sizeof(addr_in));//清空为0
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0;//默认是客户端、阻塞、TCP、本地套接字
	}
	CSocketParam(sockaddr_in* addrin, int attr) {
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
	}
	CSocketParam(const Buffer& ip, short port, int attr) {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;//AF:Address Family PF:Protocol Family（协议组）所以在windows中AF_INET与PF_INET完全一样 代表TCP/IP协议族
		addr_in.sin_port = htons(port);//host to net (short)主机字节序转为网络字节序
		//htonl则代表按unsigned long表示
		addr_in.sin_addr.s_addr = inet_addr(ip);	
	}
	CSocketParam(const Buffer& path, int attr) {
		ip = path;
		addr_un.sun_family = AF_UNIX;
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}
	~CSocketParam(){}
	CSocketParam(const CSocketParam& param) {
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}
	CSocketParam& operator=(const CSocketParam& param) {
		if (this != &param) {
			ip = param.ip;
			port = param.port;
			attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}
		return *this;
	}
	sockaddr* addrin() { return (sockaddr*)&addr_in; }//调用系统API时使用
	sockaddr* addrun() { return (sockaddr*)&addr_un; }

	//地址
	sockaddr_in addr_in;//公共成员，因为不是私有的，也可以命名为m_addr_in
	sockaddr_un addr_un;//本地套接字地址
	//其它参数
	Buffer ip;//ip
	short port;//端口
	int attr;//状态，参考SockAttr
};
class CSocketBase
{
public:
	CSocketBase() {
		m_socket = -1;
		m_status = 0;//初始化未完成
	}
	virtual ~CSocketBase() {//虚析构函数可以用来传递析构操作
		Close();
	}
	//虚析构函数可以保证在delete父类指针的时候可以顺着虚析构把子类对象给delete掉。如果析构函数非虚函数，就不具有传递性（子类可能就不能完整删除）。
	
	//初始化 服务器：套接字创建、bind绑定、listen监听； 客户端：套接字创建。
	virtual int Init(const CSocketParam& param) = 0;//纯虚函数，强迫继承类（如客户端）必须实现该函数（若不实现，则子类属于抽象类无法实例化：无法用子类声明对象）
	//子类只有把所有抽象函数实现了，才能实例化
	//链接 服务器：accept； 客户端：connect。 （对于UDP，这里可以忽略，因为udp的link包含在send和recv中） ，虽然UDP可以不做，但是这边可以赋0，保证了无论使用什么协议流程都是统一的。保证了写业务的时候不用保证上层是怎么做的
	virtual int Link(CSocketBase** pClient = NULL) = 0;//纯虚函数  服务器填参数（用于accept某个客户端，需要引入一个二级指针指向对象），客户端不用填参数（connect不用参数，使用默认参数为空）
	//抽象类，无法生成实例对象，所以不能使用&
	//** 二级指针，一级指针值在内部改变后无法传出（即在函数调用期间这个参数是复制来的，而不是直接使用外部的指针）
	//*& 表示对指针类型的引用，这样作为形参的指针地址会进入函数，函数直接使用外部指针变量，而不是赋值传入参数生成新的指针（*），这里因为引用变量不能赋值null，所以只能用二级指针
	//发送数据
	virtual int Send(const Buffer& data) = 0;//纯虚函数
	//接收数据
	virtual int Recv(Buffer& data) = 0;//纯虚函数
	//关闭连接
	virtual int Close() {
		m_status = 3;//已经关闭
		if (m_socket != -1) {//描述符未关闭时
			int fd = m_socket;
			m_socket = -1;
			if ((m_param.attr & SOCK_ISSERVER) && //服务器
				((m_param.attr & SOCK_ISIP) == 0))//使用本地套接字(非网络套接字)
				unlink(m_param.ip);//只有使用本地套接字的服务器才应该释放，客户端还没完全结束就释放会导致问题:当ip为本地套接字时（属于路径）本地sock文件会找不到
			close(fd);
		}
		return 0;
	}
	virtual operator int() { return m_socket; }
	virtual operator int() const{ return m_socket; }
	virtual operator const sockaddr_in* () const { return &m_param.addr_in; }
	virtual operator sockaddr_in* () { return &m_param.addr_in; }
protected://不能声明为private，因为继承的子类也需要使用（保护成员在派生类（即子类）中是可访问的）
	//套接字描述符，默认值-1
	int m_socket;
	//状态（因为有时候我们不能根据套接字描述符确定状态，若在接收和发送前未确定是否连接或初始化就进行操作，会产生一系列问题）: 0（默认值）初始化未完成 1初始化已完成 2连接完成 3已关闭
	int m_status;
	//初始化参数
	CSocketParam m_param;
};

//网络和本地套接字二合一
class CSocket :public CSocketBase//公有继承，若是私有继承，那么子类的子类（孙类）无法访问父类的所有成员
{
public:
	CSocket() :CSocketBase() {}
	CSocket(int sock) :CSocketBase(){
		m_socket = sock;
	}
	virtual ~CSocket() {//虚析构函数可以用来传递析构操作
		Close();
	}
	//虚析构函数可以保证在delete父类指针的时候可以顺着虚析构把子类对象给delete掉。如果析构函数非虚函数，就不具有传递性（子类可能就不能完整删除）。

	//初始化 服务器：套接字创建、bind绑定、listen监听； 客户端：套接字创建。
	virtual int Init(const CSocketParam& param) {
		if (m_status != 0) return -1;//若已经初始化或套接字已关闭（即将关闭），就不应该二次初始化
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;//SOCK_DGRAM报文形式（UDP）
		if (m_socket == -1) {//未初始化时就初始化
			if (param.attr & SOCK_ISIP)
				m_socket = socket(PF_INET, type, 0);//PF_INET网络IP协议
			else
				m_socket = socket(PF_LOCAL, type, 0);//PF_LOCAL本地协议，SOCK_STREAM TCP
		}
		else//套接字已创建非空（accept来的套接字）。这里因为m_socket!=-1故直接为m_status设为2
			m_status = 2;//比如CEPlayerServer.h(virtual int BusinessProcess(CProcess* proc)第50行中，客户端处理进程的子线程收到传来的socket后要创建客户端的业务处理类,创建了一个套接字后只做了Init没有Link直接传给
		if (m_socket == -1) return -2;
		int ret = 0;
		if (m_param.attr & SOCK_ISREUSE) {//重用
			int option = 1;
			ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
			if (ret == -1) return -7;
		}
		if (m_param.attr & SOCK_ISSERVER) {//是服务器，需要bind和listen
			if (param.attr & SOCK_ISIP)//网络
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in));//TODO:这里出问题了
			else//本地
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			if (ret == -1) return -3;//因为0属于服务器也对应阻塞状态，不可能出现EAGAIN
			ret = listen(m_socket, 32);
			if (ret == -1) return -4;
		}//客户端不用干啥，所以不用else

		if (m_param.attr & SOCK_ISNONBLOCK) {//若需要设置为非阻塞方式
			int option = fcntl(m_socket, F_GETFL);//获取套接字
			if (option == -1) return -5;//获取失败
			option |= O_NONBLOCK;//在状态中设置是否阻塞在二进制某位上体现，所以这里要用|按位或运算，防止其他设置也受到改变
			ret = fcntl(m_socket, F_SETFL, option);
			if(ret == -1) return -6;
		}
		
		if (m_status == 0)
			m_status = 1;//初始化完成
		return 0;
	}
	
	//链接 服务器：accept； 客户端：connect。 （对于UDP，这里可以忽略，因为udp的link包含在send和recv中） ，虽然UDP可以不做，但是这边可以赋0，保证了无论使用什么协议流程都是统一的。保证了写业务的时候不用保证上层是怎么做的
	virtual int Link(CSocketBase** pClient = NULL) {
		if ((m_status <= 0) || (m_socket == -1)) return -1;//初始化未完成
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) {//服务端
			if (pClient == NULL) return -2;//二级指针地址不应为空，否则不能创建用于和客户端连接的socket通信
			CSocketParam param;//用于接收
			socklen_t len = 0;
			int fd = -1;
			if (m_param.attr & SOCK_ISIP) {//网络
				param.attr |= SOCK_ISIP;
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len);//返回一个用于通信的新的socket的“描述符”，
			}
			else {
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len);//返回一个用于通信的新的socket的“描述符”，
			}
			if (fd == -1) return -3;
			*pClient = new CSocket(fd);//这个新的socket用于客户端与服务端之间的通信。
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param);
			if (ret != 0) {//初始化失败
				delete (*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else {//客户端
			if (m_param.attr & SOCK_ISIP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			if (ret != 0) return -6;
		}
		m_status = 2;//连接完成
		return 0;
	}
	//发送数据
	virtual int Send(const Buffer& data) {
		if ((m_status < 2) || (m_socket == -1)) return -1;
		ssize_t index = 0;
		while (index < (ssize_t)data.size()) {//没发完继续发
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index);
			if (len == 0) return -2;//关闭了
			if (len < 0) return -3;
			index += len;
		}
		//printf("%s(%d):[%s]data.size()=%d index=%d\n", __FILE__, __LINE__, __FUNCTION__, data.size(), index);
		return 0;
	}
	//接收数据 大于0，表示接收成功；小于0，表示失败；等于0，表示没收到数据，但没有错误。
	virtual int Recv(Buffer& data) {
		if ((m_status < 2) || (m_socket == -1)) return -1;
		data.resize(1024 * 1024);//因为一开始传入的数据为空，size为0，所以需要重置大小
		ssize_t len = read(m_socket, (void*)data, data.size());
		if (len > 0) {//接收成功
			data.resize(len);
			return (int)len;//收到数据
		}
		data.clear();
		if (len < 0) {
			if (errno == EINTR || (errno == EAGAIN)) {//非阻塞式
				data.clear();
				return 0;//未收到数据
			}
			return -2;//发送错误
		}
		return -3;//包括了len==0(EOF)，套接字被关闭  //TODO:这里有问题，导致了段错误
	}
	//关闭连接
	virtual int Close() {
		return CSocketBase::Close();
	}
};
#endif