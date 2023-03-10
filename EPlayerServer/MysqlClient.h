#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include <mysql/mysql.h>

class CMysqlClient :public CDatabaseClient
{
public:
	CMysqlClient() {
		bzero(&m_db, sizeof(m_db));
		m_bInit = false;
	}
	virtual ~CMysqlClient() {
		Close();
	}

	CMysqlClient(const CMysqlClient&) = delete;
	CMysqlClient& operator=(const CMysqlClient&) = delete;

	//纯虚函数，使得该类成为抽象类，且派生类必须实现这个函数才能有创建对象
	//连接
	virtual int Connect(const KeyValue& args);//用键值对方式传入参数，以适配多种数据库（防止各种数据库要求参数不一致导致需要多次重载的问题/麻烦）
	//执行
	virtual int Exec(const Buffer& sql);
	//带结果的执行
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table);//查询和查询结果(查出来的应该是对象，这才符合ORM的要求)
	//事务开启
	virtual int StartTransaction();
	//事务提交
	virtual int CommitTransaction();
	//事务回滚
	virtual int RollbackTransaction();
	//关闭连接
	virtual int Close();
	//判断是否处于连接状态 true表示连接中，false表示未连接
	virtual bool isConnected();
private:
	MYSQL m_db;
	bool m_bInit;//默认false 表示未初始化 初始化完成置true，表示已连接

	//class ExecParam {
	//public:
	//	ExecParam(CMysqlClient* obj, Result& result, const _Table_& table)
	//		:obj(obj), result(result), table(table) {}
	//	CMysqlClient* obj;
	//	Result& result;
	//	const _Table_& table;
	//};
};

class _mysql_table_ :public _Table_
{
public:
	_mysql_table_() :_Table_() {}
	virtual ~_mysql_table_() {}

	_mysql_table_(const _mysql_table_& table);
	_mysql_table_& operator=(const _mysql_table_& table);

	//返回用于创建该表的SQL语句
	virtual Buffer Create();//因为需要传回字符串，而不是函数内部产生的局部字符串（会在函数执行完毕后自动执行析构，导致找不到问题）
	virtual Buffer Drop();
	//增
	virtual Buffer Insert(const _Table_& values);
	//删
	virtual Buffer Delete(const _Table_& values, const Buffer& condition = "");
	//改
	virtual Buffer Modify(const _Table_& values, const Buffer& condition = "");//value是用来干嘛的？
	//查
	virtual Buffer Query(const Buffer& condition = "");//带查询条件的
	//创建一个基于表的对象(该表的副本)，用于查询使用
	virtual PTable Copy() const;
	virtual void ClearFieldCondition();
	//获取表的全名
	virtual operator const Buffer() const;
};


class _mysql_field_ :public _Field_
{
public:

	_mysql_field_();
	_mysql_field_(
		int ntype,
		const Buffer& name,
		const Buffer& type,
		const Buffer& size,
		unsigned attr,
		const Buffer& default_,
		const Buffer& check
	);
	_mysql_field_(const _mysql_field_& field);
	virtual ~_mysql_field_();
	//根据字符串转成对应的值
	virtual Buffer Create();
	//列赋值
	virtual void LoadFromStr(const Buffer& str);
	//将列转为等式字符串形式（列名=列值）
	virtual Buffer toEqualExp() const;
	//将列转为Sql语句的字符串形式(列值)
	virtual Buffer toSqlStr() const;
	//操作符重定义,获取列名
	virtual operator const Buffer() const;
private:
	Buffer Str2Hex(const Buffer& data) const;
};