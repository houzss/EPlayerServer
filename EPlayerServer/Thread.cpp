//用于初始化Thread.h中的static std::map<pthread_t, CThread*> m_mapThread;
#include "Thread.h"
std::map<pthread_t, CThread*> CThread::m_mapThread;//初始化