#pragma once
#include "Public.h"
#include "http_parser.h"
#include <map>

class CHttpParser
{//HTTP解析类
public:
	CHttpParser();
	~CHttpParser();
	//复制、赋值构造函数
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
	//接口
	size_t Parser(const Buffer& data);
	unsigned Method() const { return m_parser.method; }//DELETE GET HEAD POST PUT...（参考http_parser.h HTTP_METHOD_MAP宏）
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; }
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const {return m_parser.http_errno; }
protected:
	//静态方法
	static int OnMessageBegin(http_parser* parser);
	static int OnUrl(http_parser* parser, const char* at, size_t length);
	static int OnStatus(http_parser* parser, const char* at, size_t length);
	static int OnHeaderField(http_parser* parser, const char* at, size_t length);
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
	static int OnHeadersComplete(http_parser* parser);
	static int OnBody(http_parser* parser, const char* at, size_t length);
	static int OnMessageComplete(http_parser* parser);
	//成员方法
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
private:
	http_parser m_parser;
	http_parser_settings m_settings;
	std::map<Buffer, Buffer> m_HeaderValues;
	Buffer m_status;
	Buffer m_url;
	Buffer m_body;
	bool m_complete;
	Buffer m_lastField;
};

class UrlParser 
{
public:
	UrlParser(const Buffer& url);
	~UrlParser(){}
	int Parser();
	Buffer operator[](const Buffer& name);
	Buffer Protocl() const { return m_protocol; }
	Buffer Host() const { return m_host; }
	int Port() const { return m_port; }//默认返回80
	void SetUrl(const Buffer& url);//允许修改Url
	const Buffer Uri()const { return m_uri; }//返回uri
private:
	Buffer m_url;
	Buffer m_protocol;
	Buffer m_host;
	Buffer m_uri;
	int m_port;
	std::map<Buffer, Buffer> m_KeyValues;
};