#ifndef CProcessD
#define CProcessD
#include "Function.h"
#include <memory.h>//memset
#include <sys/socket.h>//socket
#include <sys/stat.h>
#include <fcntl.h>//func control
#include <stdlib.h>
#include <cerrno>
#include <signal.h>

class CProcess
{
public:
    CProcess() {//记录了整体的函数和参数，在设置的时候才能确认。在构造的时候不能确认,因此会报找不到参数列表的错误
        //CFunction* m_func = nullptr;
        //使用父类，父类不是模板的，可以在构造的时候确认，不会报错。这样就把模板限制在函数SetEntryFunction中了，不至于CProcess变成模板类
        m_func = NULL;
        memset(pipes, 0, sizeof(pipes));
    }
    ~CProcess() {
        if (m_func != NULL) {
            delete m_func;
            m_func = NULL;
        }
    }
    template<typename _FUNCTION_, typename... _ARGS_>//函数类型，可变参数类型
    int setEntryFunction(_FUNCTION_ func, _ARGS_... args) {//设置入口函数
        m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
        return 0;
    }
    int CreateSubProcess() {//创建子进程
        if (m_func == NULL) return -1;
        //要在fork之前建立套接字，否则fork完无法建立通讯
        int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);//AF_LOCAL:linux独有的本地套接字,会创建出一个链接，SOCK_STREAM：TCP协议
        if (ret == -1) return -2;
        pid_t pid = fork();//返回pid>0主进程，==0子进程，==-1创建失败
        if (pid == -1) return -3;
        if (pid == 0) {//子进程
            close(pipes[1]);//子进程关闭写管道，只需要读（父进程传入，子进程接收）
            pipes[1] = 0;
            //若想从子进程传出，需要重新声明并创建一个管道（不能两个进程都往一个地方写，会一直处于满状态）
            ret = (*m_func)();
            exit(0);//创建完之后直接退出，无需返回（防止继续执行主进程代码）
        }
        //主进程
        close(pipes[0]);//关闭读管道
        pipes[0] = 0;
        m_pid = pid;
        return 0;
    }
    ////需要测试文件描述符传递
    int sendFD(int fd) {//主进程完成
        //fd:File Deskriptor
        struct msghdr msg;
        iovec iov[2];
        char buf[2][10] = { "houzss","NB" };
        bzero(&msg, sizeof(msg));
        iov[0].iov_base = buf[0];//消息不重要，重要的是文件描述符fd
        iov[0].iov_len = sizeof(buf[0]);//6个字符+1个结束符'\0'
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);//6个字符+1个结束符'\0'
        msg.msg_iov = iov;//传输数据（指针）
        msg.msg_iovlen = 2;//传输数据个数
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        //下面的数据才是需要传递的（真正数据）。
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        //cmsghdr* cmsg = new cmsghdr;
        //bzero(cmsg, sizeof(cmsghdr));
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));//宏，文件描述符长度
        cmsg->cmsg_level = SOL_SOCKET;//版本太新，不用管
        cmsg->cmsg_type = SCM_RIGHTS;//权限
        *(int*)CMSG_DATA(cmsg) = fd;//control
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = sendmsg(pipes[1], &msg, 0);//写管道（管道只能从1写）
        //char buf1[100] = "test for straight write";
        //ssize_t ret = write(pipes[1], buf1, sizeof(buf1));
        //if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
        if (ret == -1) {//
            return -2;
        }
        free(cmsg);
        return 0;
    }
    int recvFD(int& fd)//子进程
        //用引用以直接修改fd本身而不是复制来的变量 
    {
        msghdr msg;
        iovec iov[2];
        char buf[][10] = { "","" };
        bzero(&msg, sizeof(msg));
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;//两者有一即可
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));//真正要接收的内容
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;//权限
        msg.msg_control = cmsg;
        msg.msg_controllen = CMSG_LEN(sizeof(int));
        ssize_t ret = recvmsg(pipes[0], &msg, 0);//读管道（管道只能从0读）
        //char buf1[100] = "";
        //ssize_t ret = read(pipes[0], buf1, sizeof(buf1));
        //if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
        //printf("%s(%d):<%s> buf = %s\n", __FILE__, __LINE__, __FUNCTION__, buf1);//创建日志子进程前打印一次
        if (ret == -1) {
            free(cmsg);
            return -2;
        }
        fd = *(int*)CMSG_DATA(cmsg);//成功了则从cmsg中读数据（文件描述符）
        free(cmsg);//防止内存泄漏
        return 0;
    }

    int sendSocket(int fd, const sockaddr_in* addrin) {//增加了对地址的发送(网络套接字适配版)
        //fd:File Deskriptor
        struct msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));
        memcpy(buf, addrin, sizeof(sockaddr_in));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        //这里直接用addrin传入iov也可以
        //iov.iov_base = (void*)addrin;//消息不重要，重要的是文件描述符fd
        //iov.iov_len = sizeof(sockaddr_in);
        msg.msg_iov = &iov;//传输数据（指针）
        msg.msg_iovlen = 1;//传输数据个数
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        //下面的数据才是需要传递的（真正数据）。
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        //cmsghdr* cmsg = new cmsghdr;
        //bzero(cmsg, sizeof(cmsghdr));
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));//宏，文件描述符长度
        cmsg->cmsg_level = SOL_SOCKET;//版本太新，不用管
        cmsg->cmsg_type = SCM_RIGHTS;//权限
        *(int*)CMSG_DATA(cmsg) = fd;//control
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        ssize_t ret = sendmsg(pipes[1], &msg, 0);//写管道（管道只能从1写）
        //char buf1[100] = "test for straight write";
        //ssize_t ret = write(pipes[1], buf1, sizeof(buf1));
        //if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
        if (ret == -1) {//
            printf("******errno %d msg %s\n", errno, strerror(errno));
            return -2;
        }
        free(cmsg);
        return 0;
    }
    int recvSocket(int& fd, sockaddr_in* addrin)//子进程
        //用引用以直接修改fd本身而不是复制来的变量 
    {
        msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;//两者有一即可
        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));//真正要接收的内容
        if (cmsg == NULL) return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;//权限
        msg.msg_control = cmsg;
        msg.msg_controllen = CMSG_LEN(sizeof(int));
        ssize_t ret = recvmsg(pipes[0], &msg, 0);//读管道（管道只能从0读）
        //char buf1[100] = "";
        //ssize_t ret = read(pipes[0], buf1, sizeof(buf1));
        //if (ret != 0) printf("errno:%d msg:%s\n", errno, strerror(errno));
        //printf("%s(%d):<%s> buf = %s\n", __FILE__, __LINE__, __FUNCTION__, buf1);//创建日志子进程前打印一次
        if (ret == -1) {
            free(cmsg);
            return -2;
        }
        memcpy(addrin, buf, sizeof(sockaddr_in));
        fd = *(int*)CMSG_DATA(cmsg);//成功了则从cmsg中读数据（文件描述符）
        if(msg.msg_iov[0].iov_base!=addrin)//这里是我自己加的
            addrin = (sockaddr_in*)msg.msg_iov[0].iov_base;
        free(cmsg);//防止内存泄漏
        return 0;
    }

    static int SwitchDeamon()//转换到守护 Linux Daemon（守护进程）是运行在后台的一种特殊进程。它独立于控制终端并且周期性地执行某种任务或等待处理某些发生的事件。它不需要用户输入就能运行而且提供某种服务，不是对整个系统就是对某个用户程序提供服务。
    {
        printf("%s(%d):<%s> pid = %d\n", __FILE__, __LINE__, __FUNCTION__, getpid());//宏：__FILE__始终指向代码文件名，__LINE__始终指向行号，__FUNCTION__始终指向代码名
        pid_t ret = fork();//创建子进程
        if (ret == -1) {
            return -1;
        }
        else if (ret > 0) {//主进程结束
            exit(0);
        }
        //子进程内容如下
        //有必要先介绍一下Linux中的进程与控制终端，登录会话和进程组之间的关系：进程属于一个进程组，进程组号（GID）就是进程组长的进程号（PID）。登录会话可以包含多个进程组。这些进程组共享一个控制终端。这个控制终端通常是创建进程的登录终端。
        //控制终端，登录会话和进程组通常是从父进程继承下来的。我们的目的就是要摆脱它们，使之不受它们的影响。
        ret = setsid();//创建新会话（无终端控制），并让子进程成为新的会话组长和新的进程组长，并与原来的登录会话和进程组脱离
        if (ret == -1) return -2;//失败则返回
        ret = fork();//创建孙进程
        if (ret == -1) return -3;
        else if (ret > 0) exit(0);//子进程结束，孙进程成为孤儿进程（既无父进程，又没有会话组长不受控制）

        //孙进程内容如下（进入守护状态）
        for (int i = 0; i < 3; i++) {
            close(i);//关掉不需要的文件描述符 [0,1,2]==>标准输入、标准输出、标准出错。
        }
        umask(0);//重设文件掩码:守护进程从父进程继承来的文件创建方式掩码可能会拒绝设置某些许可权限，文件权限掩码是指屏蔽掉文件权限中的对应位。
        signal(SIGCHLD, SIG_IGN);//屏蔽子进程消息信号(可以把某个信号的处置设定为SIG_IGN来忽略它)
        //子进程的运行状态发生变化就会发送SIGCHILD信号；这里的意思是，子进程比较依恋父母，自己发生变化就要给父母说一下。
        return 0;
    }
private:
    CFunctionBase* m_func;
    pid_t m_pid;//记录进程pid号
    int pipes[2];//创建管道（后期会改成socket用于收发）
};
#endif
