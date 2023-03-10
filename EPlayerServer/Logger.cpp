#include "Logger.h"

LogInfo::LogInfo(const char* file, int line, const char* func, 
	pid_t pid, pthread_t tid, int level, 
	const char* fmt, ...)
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ", file, line, sLevel[level], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
	else return;//反之出错直接退出

	va_list ap;//可变参数解析
	va_start(ap, fmt);
	count = vasprintf(&buf, fmt, ap);//vasprintf 函数将变长参数的内容输出到 buf 中，若成功则返回输出内容的长度，若失败则返回 -1.
	if (count > 0) {
		m_buf += buf;
		free(buf);//防止内存泄漏
	}
	m_buf += "\n";
	va_end(ap);
}

LogInfo::LogInfo(const char* file, int line, const char* func, 
	pid_t pid, pthread_t tid, int level)
{//自己主动发的 流式日志
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	bAuto = true;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ", file, line, sLevel[level], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
}

LogInfo::LogInfo(const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level, 
	void* pData, size_t nSize)
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n", file, line, sLevel[level], (char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
	else return;//反之出错直接推出

	Buffer out;
	size_t i = 0;
	char* Data = (char*)pData;
	for (; i < nSize; i++) {
		char buf[16] = "";
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf;
		if (0 == ((i + 1) % 16)) {//每16个字符一行
			m_buf += "\t; ";
			char buf[17] = "";
			memcpy(buf, Data + i - 15, 16);
			for (int j = 0; j < 16; j++)
				if ((buf[j] < 32) && (buf[j]>=0)) buf[j] = '.';//中文显示不出来（一个中文占3个16位）
			m_buf += buf;
			//for (size_t j = i - 15; j <= i; j++) {//输出乱码
			//	if (((Data[j] & 0xFF) > 31) && ((Data[j] & 0xFF) < 0x7F)) {//31为控制位,在ASCII码可显示范围内
			//		m_buf += Data[i];
			//	}
			//	else {
			//		m_buf += '.';
			//	}
			//}
			m_buf += "\n";
		}
	}
	//处理尾巴
	size_t k = i % 16;
	if (k != 0) {//有尾巴但非16倍数，最后未换行
		for (size_t j = 0; j < 16 - k; j++) m_buf += "   ";
		m_buf += "\t; ";
		for (size_t j = i - k; j <= i; j++) {
			if (((Data[j] & 0xFF) > 31) && ((Data[j] & 0xFF) < 0x7F)) {//31为控制位,在ASCII码可显示范围内
				m_buf += Data[i];
			}
			else {
				m_buf += '.';
			}
		}
		m_buf += "\n";
	}

}

LogInfo::~LogInfo()
{
	if (bAuto) {//为真则表示需要自己发送
		m_buf += "\n";
		CLoggerServer::Trace(*this);//把自己发上去
	}
}