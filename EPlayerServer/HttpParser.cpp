#include "HttpParser.h"
#include <string.h>
CHttpParser::CHttpParser()
{
	m_complete = false;
	memset(&m_parser, 0, sizeof(m_parser));
	m_parser.data = this;//data存储连接或套接字对象（可以存入HttpParser对象），由于回调函数是静态函数或标准成员函数，只有将this指针作为参数传入才可以使用类内成员函数
	http_parser_init(&m_parser, HTTP_REQUEST);//服务端只会收到来自客户端的请求(如果是客户端会收到服务端的响应)
	memset(&m_settings, 0, sizeof(m_settings));
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin;
	m_settings.on_url = &CHttpParser::OnUrl;
	m_settings.on_status = &CHttpParser::OnStatus;
	m_settings.on_header_field = &CHttpParser::OnHeaderField;
	m_settings.on_header_value = &CHttpParser::OnHeaderValue;
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;
	m_settings.on_body = &CHttpParser::OnBody;
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete;
	/*m_settings.on_chunk_header = NULL;
	m_settings.on_chunk_complete = NULL;*///因为前面memset置空了，所以这里不用填
}

CHttpParser::~CHttpParser()
{}

CHttpParser::CHttpParser(const CHttpParser & http)
{
	memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
	m_parser.data = this;
	memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
	m_status = http.m_status;
	m_url = http.m_url;
	m_body = http.m_body;
	m_complete = http.m_complete;
	m_lastField = http.m_lastField;
}

CHttpParser& CHttpParser::operator=(const CHttpParser& http)
{
	if (this != &http) {
		memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
		m_parser.data = this;
		memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
		m_status = http.m_status;//Buffer对象来自string继承类，等号赋值并非浅赋值地址
		m_url = http.m_url;
		m_body = http.m_body;
		m_complete = http.m_complete;
		m_lastField = http.m_lastField;
	}
	return *this;
}

size_t CHttpParser::Parser(const Buffer& data)
{
	m_complete = false;
	size_t ret = http_parser_execute( 
		&m_parser, &m_settings, data, data.size());//解析了多少
	if (m_complete == false) {
		m_parser.http_errno = 0x7f;//127报错
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageBegin();//((CHttpParser*)parser->data)为传回的指针对象，即使用静态函数调用对象的成员方法
}

int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnUrl(at,length);
}

int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnStatus(at,length);
}

int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderField(at,length);
}

int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderValue(at,length);
}

int CHttpParser::OnHeadersComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnHeadersComplete();
}

int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnBody(at,length);
}

int CHttpParser::OnMessageComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageComplete();
}

int CHttpParser::OnMessageBegin()
{
	return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
	m_url = Buffer(at, length);
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length);
	return 0;
}

int CHttpParser::OnMessageComplete()
{
	m_complete = true;
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
	m_protocol = "";
	m_host = "";
	m_uri = "";
	m_port = 80;
	m_KeyValues.clear();
}

int UrlParser::Parser()
{
	//分三步解析：协议、域名和端口、URI
	//解析协议部分
	const char* pos = m_url;//"https://cn.bing.com:80/"
	const char* target = strstr(pos, "://");
	if (target == NULL) return -1;
	m_protocol = Buffer(pos, target);//"https"

	//解析域名和端口
	pos = target+3;//pos位置右移，查找第一个/  pos="cn.bing.com:80/"
	target = strchr(pos, '/');
	if (target == NULL) {
		if ((m_protocol.size() + 3) >= m_url.size())	return -2;//URL不全
		m_host = pos;//后面没有反斜杠，直接协议://后面全都是host
		return 0;
	}
	Buffer value = Buffer(pos, target);//value ="cn.bing.com:80"
	if (value.size() == 0) return -3;//host域名非空
	target = strchr(value, ':');
	if (target != NULL) {
		m_host = Buffer(value, target);//m_host = "cn.bing.com"
		m_port = atoi(Buffer(target + 1, (char*)value + value.size()));//m_port = aoit((char*)"80")
	}
	else {//找不到端口号
		m_host = value;
	}
	//解析URI
	pos = strchr(pos, '/');// pos="/search?EID=MBSC&form=BGGCDF&pc=U710&q=URL+URI"
	target = strchr(pos, '?');//"https://cn.bing.com/search?EID=MBSC&form=BGGCDF&pc=U710&q=URL+URI"，找到?之后的部分
	if (target == NULL) {//没找到，表示剩下的全是m_uri
		m_uri = pos+1;//省略"/"
		return 0;
	}
	else {
		m_uri = Buffer(pos+1, target);//uri="search"
		//解析key和value
		pos = target + 1;//"key1=value1&..."
		const char* t = NULL;//指向常数对象的指针，不能对指针指向对象值进行修改，但可以改变指向对象
		do {
			target = strchr(pos, '&');
			if (target == NULL) {//"a=b"或""
				t = strchr(pos, '=');
				if (t == NULL) return -4;//?后没跟东西
				m_KeyValues[Buffer(pos, t)] = Buffer(t + 1);
				break;//退出do-while循环
			}
			else {//"a=b&c=d"
				Buffer kv(pos, target);//kv表示 "a=b"
				t = strchr(kv, '=');
				if (t == NULL) return -5;
				m_KeyValues[Buffer(kv, t)] = Buffer(t + 1, (char*)kv + kv.size());
				pos = target + 1;
			}
		} while (pos != NULL);
	}

	return 0;
}

Buffer UrlParser::operator[](const Buffer& name)
{
	auto it = m_KeyValues.find(name);
	if (it == m_KeyValues.end())
		return Buffer();
	return it->second;
}

void UrlParser::SetUrl(const Buffer& url)
{
	m_url = url;
	m_protocol = "";
	m_host = "";
	m_uri = "";
	m_port = 80;
	m_KeyValues.clear();
}
