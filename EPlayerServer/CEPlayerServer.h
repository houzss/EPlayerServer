#pragma once
#include "Logger.h"
#include "CServer.h"
#include "OpenSSLHelper.h"
#include "MysqlClient.h"
#include "HttpParser.h"
#include "jsoncpp/json.h"
#include <map>

DECLARE_TABLE_CLASS(user_mysql, _mysql_table_)
//列所属ClassName，枚举定义类型，列名，数据库存储类型，长度，主键/非空等属性关键字，默认值，约束(CHECK)
DECLARE_FIELD(_mysql_field_, TYPE_INT, "user_id", "INTEGER", "", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_VARCHAR, "user_qq", "VARCHAR", "(15)", NOT_NULL, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_VARCHAR, "user_phone", "VARCHAR", "(12)", NOT_NULL | DEFAULT, "18888888888", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_name", "TEXT", "", NOT_NULL, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_nick", "TEXT", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_wechat", "TEXT", "", DEFAULT, "NULL", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_wechat_id", "TEXT", "", DEFAULT, "NULL", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_address", "TEXT", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_province", "TEXT", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_country", "TEXT", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_INT, "user_age", "INTEGER", "", DEFAULT | CHECK, "18", "")
DECLARE_FIELD(_mysql_field_, TYPE_BOOL, "user_male", "BOOL", "", DEFAULT, "1", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_flags", "TEXT", "", DEFAULT, "0", "")
DECLARE_FIELD(_mysql_field_, TYPE_REAL, "user_experience", "REAL", "", DEFAULT, "0.0", "")
DECLARE_FIELD(_mysql_field_, TYPE_INT, "user_level", "INTEGER", "", DEFAULT | CHECK, "0", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_class_priority", "TEXT", "", DEFAULT, "0", "")//自己加的默认
DECLARE_FIELD(_mysql_field_, TYPE_REAL, "user_time_per_viewer", "REAL", "", DEFAULT, "0.0", "")//自己加的默认
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_career", "TEXT", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_password", "TEXT", "", NOT_NULL, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_DATETIME, "user_birthday", "DATETIME", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_describe", "TEXT", "", NONE, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_education", "TEXT", "", NONE, "", "")
//DECLARE_FIELD(_mysql_field_, TYPE_DATETIME, "user_register_time", "DATETIME", "", DEFAULT, "LOCALTIME()", "")//TODO:这里不可以
DECLARE_TABLE_CLASS_END()


#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("%s(%d):<%s> pid = %d, ret = %d, errno= %d, msg= [%s]", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));return err;}
#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("%s(%d):<%s> pid = %d, ret = %d, errno= %d, msg= [%s]", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));continue;}
class CEPlayerServer :public CBusiness
{
public:
	CEPlayerServer(unsigned count) : 
		CBusiness(), 
		m_count(count) 
	{
		
	}
	~CEPlayerServer() {
		if (m_db) {
			CDatabaseClient* db = m_db;
			m_db = NULL;
			//db->Close();//TODO：这里会报段错误，可能调用纯虚函数了
			delete db;
		}
		m_epoll.Close();
		m_tpoll.Close();
		for (auto it : m_MapClients) {
			if (it.second) {
				delete it.second;
			}
		}
		m_MapClients.clear();
	}
	virtual int BusinessProcess(CProcess* proc) {
		using namespace std::placeholders;//使得后面占位符可以直接使用_1,..._29,否则应该是std::placeholders::_1,...,std::placeholders::_29
		int ret = 0;
		m_db = new CMysqlClient();
		if (m_db == NULL) {
			TRACEE("no more memory for new MysqlClient");
			return -1;
		}
		KeyValue args;
		args["host"] = "10.1.125.83";
		args["user"] = "root";
		args["passwd"] = "lab505";
		args["port"] = 3306;
		args["db"] = "houzss";
		ret = m_db->Connect(args);
		ERR_RETURN(ret, -2);
		//连接成功后还得保证这个表存在
		user_mysql user;//不存在则创建，存在因为语句为Create if no exists所以不会重复创建
		ret = m_db->Exec(user.Create());//存在则会报错-2，虽然没有错误但执行了SQL语句返回非0
		ERR_RETURN(ret, -3);
		ret = setConnectedCallback(&CEPlayerServer::Connected, this, _1);//std::placeholders占位符,共29个
		ERR_RETURN(ret, -4);
		ret = setRecvCallback(&CEPlayerServer::Received, this, _1, _2);
		ERR_RETURN(ret, -5);
		int sock = 0;
		ret = m_epoll.Create(m_count);
		ERR_RETURN(ret, -6);
		ret = m_tpoll.Start(m_count);
		ERR_RETURN(ret, -7);
		for (unsigned i = 0; i < m_count; i++) {
			ret = m_tpoll.AddTask(&CEPlayerServer::ThreadFunc, this);//因为属于成员函数,所以要将this指针作为参数传入
			ERR_RETURN(ret, -8);
		}
		sockaddr_in addrin;
		while (m_epoll != -1) {
			ret = proc->recvSocket(sock, &addrin);
			TRACEI("RecvSocket ret=%d", ret);
			if (ret < 0 || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock);//这里收到已创建好的socket
			if (pClient == NULL) continue;
			ret = pClient->Init(CSocketParam(&addrin, SOCK_ISIP));//进入Init后看看socket是否等于-1,若非-1(表示socket已建立)直接m_status状态设为2;
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, EpollData((void*)pClient));
			if (m_connectedcallback) {
				//(*m_connectedcallback)();
				(*m_connectedcallback)(pClient);
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}

private:
	unsigned m_count;
	CEpoll m_epoll;
	CThreadPoll m_tpoll;
	std::map<int, CSocketBase*> m_MapClients;
	CDatabaseClient* m_db;

	int HttpParser(const Buffer& data) {
		TRACEI("HttpParser-data:%s", (char*)data);
		CHttpParser parser;
		size_t psize = parser.Parser(data);
		if (psize == 0 || parser.Errno() != 0) {
			TRACEE("size:%llu, errno: %u", psize, parser.Errno());
			return -1;
		}
		if (parser.Method() == HTTP_GET) {
			//get处理
			TRACEI("*****url:%s", "https://101.1.125.60" + parser.Url());
			UrlParser url("https://101.1.125.60"+parser.Url());
			int ret = url.Parser();
			if (ret != 0) {
				TRACEE("ret = %d, url[%s],  errno: %u", ret, "https://101.1.125.60" + parser.Url(), parser.Errno());
				return -2;
			}
			Buffer uri = url.Uri();
			TRACEI("*****uri:%s", (char*)uri);
			if (uri == "login") {
				//处理登录
				Buffer time = url["time"];
				Buffer salt = url["salt"];
				Buffer user = url["user"];
				Buffer sign = url["sign"];
				TRACEI("time: %s salt: %s user: %s sign: %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
				//数据库查询
				user_mysql db_user;
				Result result;
				Buffer sql = db_user.Query("user_name=\"" + user + "\"");
				ret = m_db->Exec(sql, result, db_user);
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
				if (ret != 0) {
					TRACEE("Query error sql = %s, ret = %d", (char*)sql, ret);
					return -3;
				}
				printf("%s(%d):[%s]result.size=%d\n", __FILE__, __LINE__, __FUNCTION__, result.size());
				if (result.size() == 0) {
					TRACEE("No result sql = %s, ret = %d", (char*)sql, ret);
					return -4;
				}
				if (result.size() != 1) {//TODO:这里查出来结果会超过1（但程序里实际上只有一条结果），
					TRACEE("User Num more than one sql = %s, ret = %d", (char*)sql, ret);
					return -5;
				}
				auto user1 = result.front();
				Buffer pwd = *(user1->Fields["user_password"]->Value.Str);
				//printf("password = %s\n", (char*)pwd);
				TRACEI("password = %s",(char*)pwd);
				//Buffer pwd = *(user1->Fields.at("password").get());这里是取得列名"password"
				//登录请求验证
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";//等价于私有字符串
				Buffer md5str = time + MD5_KEY + pwd + salt;
				Buffer md5 = COpenSSLHelper::MD5(md5str);
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign) {//一致说明匹配成功，返回0
					return 0;
				}
				else {
					return -6;//返回密码错误
				}
				return -7;//否则报错
			}
		}
		else if (parser.Method() == HTTP_POST) {
			//post处理

		}
		return -7;
	}
	
	Buffer MakeResponse(int ret)
	{
		Json::Value root;//既可以是数组类型，也可以是键值对类型（如下所示），还可以是结构体（内部再套Json::Value）类型
		if (ret != 0) {//有错误
			root["status"] = ret;
			root["message"] = "login failed.";//登陆失败，可能是用户名或者密码/或者加密错误
		}
		else {
			root["status"] = 0;
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();
		Buffer result = "HTTP/1.1 200 OK\r\n";
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server: Houzss/0.1\r\nContent-Type:text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		//memset(temp, '\0', sizeof(temp));//因为之前用过了，如果不清零我担心会读出异常数据?但实质上会在最后一个字符后加一个"\0",在printf/cout/string()不会读出，因此这句话可以不写
		snprintf(temp, sizeof(temp), "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";//两个换行表明整个结束
		result += Date + Server + Length + Stub + json;
		TRACEI("response:", (char*)result);
		return result;
	}

	int ThreadFunc() {
		int ret = 0;
		EPEvents events;
		while (m_epoll != -1) {
			ssize_t size = m_epoll.WaitEvents(events);
			if (size < 0) break;
			if (size > 0) {
				for (ssize_t i = 0; i < size; i++) {
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
						if (pClient) {
							Buffer data;
							ret = pClient->Recv(data);
							TRACEI("recv data size %d", ret);
							if (ret <= 0) {
								TRACEW("%s(%d):<%s> pid = %d, ret = %d, errno= %d, msg= [%s]", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno)); 
								m_epoll.Del(*pClient);//有问题则从epoll池中删除，防止一直报错
								continue;
							}
							//WARN_CONTINUE(ret);
							if (m_recvcallback) {
								(*m_recvcallback)(pClient, data);//需要传入参数,还需要修改
							}
						}
					}
				}
			}
		}
		return 0;
	}
	int Connected(CSocketBase* pClient) {
		//TODO:客户端连接处理，简单打印一下客户端信息
		sockaddr_in* paddr = *pClient;
		TRACEI("client connected addr: %s, port: %d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}
	int Received(CSocketBase* pClient, const Buffer& data) {
		TRACEI("接收到数据");
		//TODO:主要业务，包括
		//HTTP解析
		int ret = 0;
		Buffer response = "";
		ret = HttpParser(data);//TODO:他把HTTPParser和密码验证放一块了
		TRACEI("HttpParser ret=%d", ret);
		if (ret != 0) {
			if (ret == -6) {
				TRACEE("Userlogin validation failed %d", ret);
				response = MakeResponse(ret);
				ret = pClient->Send(response);
				if (ret != 0) {
					TRACEE("Received: Client send response failed:%d [%s]", ret, (char*)response);
				}
				else {
					TRACEI("Received: Client send response success!");
				}
				return 0;//这里只是匹配错误，所以应该直接return 0;
			}
			TRACEE("http parser failed!%d", ret);
			return -1;
		}
		////验证结果的反馈
		//if (ret != 0) {//验证失败
		//	TRACEE("User login validation failed:%d", ret);
		//}
		response = MakeResponse(ret);
		ret = pClient->Send(response);
		if (ret != 0) {
			TRACEE("Received: Client send response failed:%d [%s]", ret, (char*) response);
		}
		else {
			TRACEI("Received: Client send response success!");
		}
		return 0;
	}
};