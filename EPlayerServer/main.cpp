#include <cstdio>
//#include "Process.h"
#include "CEPlayerServer.h"
#include "HttpParser.h"
#include "Sqlite3Client.h"
#include "MysqlClient.h"
#include "OpenSSLHelper.h"
int CreateLogServer(CProcess* proc) {//参数类封装，如果传入指针，用于接收的处理函数需要提前知道传入指针类型，还得通过强制类型转换（要增加新数据还得增加一个无属性指针）
                                    //而我们传入参数类就可以不管函数需要哪几类数据，供其使用即可
    CLoggerServer server;
    int ret = server.Start();//子进程启动日志服务器（本地套接字）
    if (ret != 0) {
        printf("%s(%d):<%s> pid = %d ret:%d errno:%d msg:%s\n", __FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));
    }
    int fd = 0;
    while (true) {//循环等待主进程信号
        ret = proc->recvFD(fd);//收到从管道传来的文件描述符，存储在fd变量中
        printf("%s(%d):<%s> ret = %d , fd = %d\n", __FILE__, __LINE__, __FUNCTION__, ret, fd);
        if (fd <= 0) break;
    }
    ret = server.Close();
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//当在日志端看到main函数20行时，对应日志服务器结束
    return 0;
}
int CreateClientServer(CProcess* proc) 
{
    //printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//宏：__FILE__始终指向代码文件名，__LINE__始终指向行号，__FUNCTION__始终指向代码名
    int fd = -1;
    int ret = proc->recvFD(fd);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //printf("%s(%d):<%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd);
    sleep(1);//休眠1秒
    char buf[20] = "";
    lseek(fd, 0, SEEK_SET);//恢复到初始状态,但注意：这里获取到的fd值和主进程创建的fd不是相同的
    read(fd, buf, sizeof(buf));
    //printf("%s(%d):<%s> buf = %s\n", __FILE__, __LINE__, __FUNCTION__, buf);
    close(fd);
    return 0;
}

int LogTest() 
{//定义返回值为int是因为可变参数-线程函数需要一个返回值
    char buffer[] = "hello houzss! 居居铭";
    usleep(1000 * 100);//休眠100毫秒
    TRACEI("Here is log %d %c %f %g %s 哈哈 嘻嘻 王猪猪",10,'a',1.0f,2.0,buffer);
    DUMPD((void*)buffer, (size_t)sizeof(buffer));
    LOGE << 100 << " " << 's' << " " << 0.12345f << " " << 1.23456789 << " " << buffer << " " << "房祖名";
    return 0;
}
int log_client_test() 
{
    //CProcess::SwitchDeamon();//fork出来的子进程会共享代码（即获取到main（）函数的代码），如果不杀掉这个子进程就会继续执行main（）下的代码
    CProcess proclog, procclients;//需要测试进程分离（派生子进程是否成功）
    printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//创建日志子进程前打印一次
    proclog.setEntryFunction(CreateLogServer, &proclog);//传入Cprocess对象，供CreateLogServer使用
    int ret = proclog.CreateSubProcess();
    if (ret != 0) {
        printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//创建失败打印一句
        return -1;
    }
    LogTest();//主进程执行该函数
    printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//创建成功打印一次
    CThread thread(LogTest);
    thread.Start();
    procclients.setEntryFunction(CreateClientServer, &procclients);
    ret = procclients.CreateSubProcess();
    if (ret != 0) {
        printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//创建失败打印一次
        return -2;
    }
    printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//结尾打印一次
    //usleep(100 * 000);
    //文件描述符应该在fork之后创建，如果在fork之前创建好，那么fork完成后子进程会复制一份主进程刚创建的文件描述符，对于这个描述符不能确定是从管道接收到的还是一开始fork复制来的
    int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND);//首选项READ WRTITE对应读写，不存在则创建，存在则追加
    printf("%s(%d):<%s> fd = %d\n", __FILE__, __LINE__, __FUNCTION__, fd);//结尾打印一次
    if (fd == -1) return -3;
    ret = procclients.sendFD(fd);//发送过去
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//结尾打印一次
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
    write(fd, "test by houzss", 14);//再写，14字节数
    close(fd);

    CThreadPoll poll;
    ret = poll.Start(4);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//结尾打印一次
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = poll.AddTask(LogTest);//非thiscall,所以不用附上this
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//结尾打印一次
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = poll.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//结尾打印一次
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = poll.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//结尾打印一次
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
    ret = poll.AddTask(LogTest);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);//结尾打印一次
    if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));

    (void)getchar();
    poll.Close();

    proclog.sendFD(-1);//通知日志服务器可以关闭了(虽然会产生报错但是还是传了-1进去)
    (void)getchar();
    return 0;
}


int business_test() 
{
    int ret = 0;
    CProcess proclog;
    ret = proclog.setEntryFunction(CreateLogServer, &proclog);//传入Cprocess对象，供CreateLogServer使用
    if (ret != 0) printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -1);
    ret = proclog.CreateSubProcess();
    if (ret != 0) printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -2);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    CEPlayerServer business(2);
    CServer server;
    ret = server.Init(&business);
    if (ret != 0) printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -3);
    ret = server.Run();
    if (ret != 0) printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -4);

    return 0;
}

int http_test() 
{
    //HttpParser test
    //完整测试用例
    Buffer str = "GET /favicon.ico HTTP/1.1\r\n"
        "Host: 0.0.0.0=5000\r\n"
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*; q = 0.8\r\n"
        "Accept-Language: en-us,en;q=0.5\r\n"
        "Accept-Encoding: gzip,deflate\r\n"
        "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
        "Keep-Alive: 300\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    CHttpParser parser;
    size_t psize = parser.Parser(str);
    if (parser.Errno() != 0) {
        printf("errno: %d\n", parser.Errno());
        return -1;
    }
    if (psize != str.size()) {
        printf("size error:%lld Expectation:%lld\n", psize, str.size());
        return -2;
    }
    printf("Method:%d, URL: %s\n", parser.Method(), (char*)parser.Url());
    
    //不完整测试用例
    str = "GET /favicon.ico HTTP/1.1\r\n"
        "Host: 0.0.0.0=5000\r\n"
        "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    psize = parser.Parser(str);
    printf("errno: %d, size %lld\n", parser.Errno(), psize);
    if (parser.Errno() != 0x7f) {//char*类型的-1（开源库规定）
        return -3;
    }
    if (psize != 0) {
        return -4;
    }

    //UrlParser test
    UrlParser url1("https://www.baidu.com/s?ie=utf8&oe=utf8&wd=httplib&tn=98010089_dg&ch=3"); 
    int ret = url1.Parser();
    if (ret != 0) {
        printf("urlparser1 failed: %d",ret);
        return -5;
    }
    printf("ie = %s expectation:utf8\n", (char*)url1["ie"]);
    printf("oe = %s expectation:utf8\n", (char*)url1["oe"]);
    printf("wd = %s expectation:httplib\n", (char*)url1["wd"]);
    printf("tn = %s expectation:98010089_dg\n", (char*)url1["tn"]);
    printf("ch = %s expectation:3\n", (char*)url1["ch"]);
    
    UrlParser url2("http://127.0.0.1:19811/?time=144000&salt=9527&user=test&sign=1234567890abcdef");
    ret = url2.Parser();
    if (ret != 0) {
        printf("urlparser2 failed: %d", ret);
        return -6;
    }
    printf("time = %s expectation:144000\n", (char*)url2["time"]);
    printf("salt = %s expectation:9527\n", (char*)url2["salt"]);
    printf("user = %s expectation:test\n", (char*)url2["user"]);
    printf("sign = %s expectation:1234567890abcdef\n", (char*)url2["sign"]);
    printf("Host:%s, Port:%d\n", (char*)url2.Host(), url2.Port());
    return 0;
}

//class user_test :public _sqlite3_table_
//{
//public:
//	virtual PTable Copy() const {
//		return PTable(new user_test(*this));
//	}
//	user_test() :_sqlite3_table_() {
//		Name = "user_test";
//		{
//			PField field(new _sqlite3_field_(TYPE_INT, "user_id", "INT", "", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "", ""));
//			FieldDefine.push_back(field);
//			Fields["user_id"] = field;
//		}
//		{
//			PField field(new _sqlite3_field_(TYPE_VARCHAR, "user_qq", "VARCHAR", "(15)", NOT_NULL, "", ""));
//			FieldDefine.push_back(field);
//			Fields["user_qq"] = field;
//		}
//	}
//};
//需要传入表名和表类名
DECLARE_TABLE_CLASS(user_test, _sqlite3_table_)
//除了必需参数外，需要额外传入列类名
DECLARE_FIELD(_sqlite3_field_,TYPE_INT, "user_id", "INTEGER", "", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "", "")
//对于字符串类型，Size需要传入括号（看看以后能不能改进自适应）
DECLARE_FIELD(_sqlite3_field_, TYPE_VARCHAR, "user_qq", "VARCHAR", "(15)", NOT_NULL, "", "")
DECLARE_FIELD(_sqlite3_field_, TYPE_VARCHAR, "user_phone", "VARCHAR", "(12)", NOT_NULL|DEFAULT, "18888888888", "")
DECLARE_FIELD(_sqlite3_field_, TYPE_TEXT, "user_name", "TEXT", "", 0, "", "")
DECLARE_TABLE_CLASS_END()
int sqlite3_test()
{
    int ret = 0;
    user_test test, value;
    printf("Create:%s\n", (char*)test.Create());
    printf("Drop:%s\n", (char*)test.Drop());
    printf("Delete:%s\n", (char*)test.Delete(test));//删
    value.Fields["user_qq"]->LoadFromStr("1817619619");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    printf("Insert:%s\n", (char*)test.Insert(value));//增
    value.Fields["user_qq"]->LoadFromStr("123456789");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    printf("MODIFY:%s\n", (char*)test.Modify(value));//改
    printf("QUERY:%s\n", (char*)test.Query());//查
    printf("Drop:%s\n", (char*)test.Drop());//删
    getchar();

    CDatabaseClient* pClient = new CSqlite3Client();
    KeyValue args;
    args["host"] = "test.db";
    ret = pClient->Connect(args);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret,-1);
    ret = pClient->Exec(test.Create());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -2);
    ret = pClient->Exec(test.Delete(value));//删
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -3);
    value.Fields["user_qq"]->LoadFromStr("1817619619");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    ret = pClient->Exec(test.Insert(value));//增
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -4);
    value.Fields["user_qq"]->LoadFromStr("123456789");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    ret = pClient->Exec(test.Modify(value));//改
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -5);
    Result result;
    ret = pClient->Exec(test.Query(),result,test);//查
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -6);
    ret = pClient->Exec(test.Drop());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -7);
    ret = pClient->Close();
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    ERR_RETURN(ret, -8);
    return 0;
}

DECLARE_TABLE_CLASS(user_test_mysql, _mysql_table_)
DECLARE_FIELD(_mysql_field_, TYPE_INT, "user_id", "INTEGER", "", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_VARCHAR, "user_qq", "VARCHAR", "(15)", NOT_NULL, "", "")
DECLARE_FIELD(_mysql_field_, TYPE_VARCHAR, "user_phone", "VARCHAR", "(12)", NOT_NULL | DEFAULT, "18888888888", "")
DECLARE_FIELD(_mysql_field_, TYPE_TEXT, "user_name", "TEXT", "", 0, "", "")
DECLARE_TABLE_CLASS_END()
int mysql_test() {
    int ret = 0;
    user_test_mysql test, value;
    printf("Create:%s\n", (char*)test.Create());
    printf("Drop:%s\n", (char*)test.Drop());
    printf("Delete:%s\n", (char*)test.Delete(test));//删
    value.Fields["user_qq"]->LoadFromStr("1817619619");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    printf("Insert:%s\n", (char*)test.Insert(value));//增
    value.Fields["user_qq"]->LoadFromStr("123456789");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    printf("MODIFY:%s\n", (char*)test.Modify(value));//改
    printf("QUERY:%s\n", (char*)test.Query());//查
    printf("Drop:%s\n", (char*)test.Drop());//删
    //getchar();

    CDatabaseClient* pClient = new CMysqlClient();
    KeyValue args;
    args["host"] = "10.1.125.83";
    args["user"] = "root";
    args["passwd"] = "lab505";
    args["port"] = 3306;
    args["db"] = "houzss";
    ret = pClient->Connect(args);
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -1);
    ret = pClient->Exec(test.Create());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -2);
    ret = pClient->Exec(test.Delete(value));//删
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -3);
    value.Fields["user_qq"]->LoadFromStr("1817619619");
    value.Fields["user_qq"]->Condition = SQL_INSERT;
    ret = pClient->Exec(test.Insert(value));//增
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -4);
    value.Fields["user_qq"]->LoadFromStr("123456789");
    value.Fields["user_qq"]->Condition = SQL_MODIFY;
    ret = pClient->Exec(test.Modify(value));//改
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -5);
    Result result;
    ret = pClient->Exec(test.Query(), result, test);//查
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -6);
    ret = pClient->Exec(test.Drop());
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -7);
    ret = pClient->Close();
    printf("%s(%d):<%s> ret = %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //ERR_RETURN(ret, -8);
    return 0;
}


int openssl_test() {
    Buffer data = "abcdef";
    data = COpenSSLHelper::MD5(data);
    printf("expectation: E80B5017098950FC58AAD83C8C14978E \n real result:%s\n", (char*)data);
    return 0;
}
int main()
{
    int ret = 0;
    ret = business_test();
	//int ret = http_test();
    //ret = sqlite3_test();
    //ret = mysql_test();
    //ret = openssl_test();
    printf("main:ret = %d\n", ret);
    return ret;
}