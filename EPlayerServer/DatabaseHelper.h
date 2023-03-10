#pragma once
#include "Public.h"
#include <map>
#include <list>
#include <memory>
#include <vector>
class _Table_;
using PTable = std::shared_ptr<_Table_>;//C++ 智能指针底层是采用引用计数的方式实现的。简单的理解，智能指针在申请堆内存空间的同时，会为其配备一个整形值（初始值为 1），每当有新对象使用此堆内存时，该整形值 +1；反之，每当使用此堆内存的对象被释放时，该整形值减 1。当堆空间对应的整形值为 0 时，即表明不再有对象使用它，该堆空间就会被释放掉。
using KeyValue = std::map<Buffer, Buffer>;//别名
using Result = std::list<PTable>;

class _Field_;
using PField = std::shared_ptr<_Field_>;
using FieldArray = std::vector<PField>;
using FieldMap = std::map<Buffer, PField>;
//shared_ptr:防止内存资源管理不当，具有一定的垃圾回收能力

class CDatabaseClient
{
public:
	CDatabaseClient() {}
	virtual ~CDatabaseClient(){}

	CDatabaseClient(const CDatabaseClient&) = delete;
	CDatabaseClient& operator=(const CDatabaseClient&) = delete;

	//纯虚函数，使得该类成为抽象类，且派生类必须实现这个函数才能有创建对象
	//连接
	virtual int Connect(const KeyValue& args) = 0;//用键值对方式传入参数，以适配多种数据库（防止各种数据库要求参数不一致导致需要多次重载的问题/麻烦）
	//执行
	virtual int Exec(const Buffer& sql) = 0;
	//带结果的执行
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0;//查询和查询结果(查出来的应该是对象，这才符合ORM的要求)
	//事务开启
	virtual int StartTransaction() = 0;
	//事务提交
	virtual int CommitTransaction() = 0;
	//事务回滚
	virtual int RollbackTransaction() = 0;
	//关闭连接
	virtual int Close() = 0;
	//判断是否处于连接状态
	virtual bool isConnected() = 0;
};

//表的基类实现
class _Table_ 
{
public:
	_Table_(){}
	virtual ~_Table_() {}
	//返回用于创建该表的SQL语句
	virtual Buffer Create() = 0;//因为需要传回字符串，而不是函数内部产生的局部字符串（会在函数执行完毕后自动执行析构，导致找不到问题）
	
	virtual Buffer Drop() = 0;
	//增
	virtual Buffer Insert(const _Table_& values) = 0;
	//删
	virtual Buffer Delete(const _Table_& values, const Buffer& condition = "") = 0;
	//改
	virtual Buffer Modify(const _Table_& values, const Buffer& condition = "") = 0;//TODO:参数优化
	//查
	virtual Buffer Query(const Buffer& condition = "") = 0;
	//创建一个基于表的对象(该表的副本)，用于查询使用
	virtual PTable Copy() const= 0;
	//清空所有列的使用状态
	virtual void ClearFieldCondition() = 0;
	//获取表的全名
	virtual operator const Buffer() const = 0;//注意返回Database+'.'+Name

	//表所属的DB名称
	Buffer Database;
	//表名称
	Buffer Name;
	//列的定义（有序）
	FieldArray FieldDefine;
	//列的映射表（用于键值匹配，如查询是否存在某列）
	FieldMap Fields;
};

enum {
	SQL_INSERT = 1,//插入列
	SQL_MODIFY = 2,//修改列
	SQL_CONDITION = 4,//查询条件列
};

enum {
	NONE = 0,
	NOT_NULL = 1,//非空
	DEFAULT = 2,//默认值
	UNIQUE = 4,//唯一
	PRIMARY_KEY = 8,//主键
	CHECK = 16,//约束
	AUTOINCREMENT = 32//自动增长
};

using SqlType = enum {
	TYPE_NULL = 0,
	TYPE_BOOL = 1,
	TYPE_INT = 2,
	TYPE_DATETIME = 4,
	TYPE_REAL = 8,//小数类型
	TYPE_VARCHAR = 16,
	TYPE_TEXT = 32,
	TYPE_BLOB = 64
};

class _Field_
{
public:
	_Field_() {}
	virtual ~_Field_() {}

	_Field_(const _Field_& field) {
		Name = field.Name;
		Type = field.Type;
		Size = field.Size;
		Attr = field.Attr;
		Default = field.Default;
		Check = field.Check;
	}
	virtual _Field_& operator=(const _Field_& field) {//为什么返回类型是引用而不是对象本身（抽象类不能生成对象，这个引用对象只能返回派生类对象的引用）
		if (this != &field) {
			Name = field.Name;
			Type = field.Type;
			Size = field.Size;
			Attr = field.Attr;
			Default = field.Default;
			Check = field.Check;
		}
		return *this;
	}
	//根据字符串转成对应的值
	virtual Buffer Create() = 0;
	//生成等于号的表达式（主要用于where语句使用）
	virtual void LoadFromStr(const Buffer& str) = 0;
	//将列转为Sql语句的字符串形式
	virtual Buffer toEqualExp() const = 0;
	//获取列的全名（在不同数据库全名形式不一样）
	virtual Buffer toSqlStr() const = 0;
	//操作符重定义
	virtual operator const Buffer() const = 0;

	//属性
	Buffer Name;
	Buffer Type;
	Buffer Size;
	unsigned Attr;//属性：唯一性 主键
	Buffer Default;//默认值（不同数据库情况有差异，mysql不允许有默认值）
	Buffer Check;//约束条件（不同数据库情况也有差异）
	//操作条件
	unsigned Condition;//用于做什么）
	union {
		bool Bool;
		int Int;
		double Double;
		Buffer* Str;
	}Value;//存储列的查询结果
	int nType;//表明Value联合体存储的类型
};

//#name表示将name转为字符串
#define DECLARE_TABLE_CLASS(name, base) class name:public base{ \
public: \
virtual PTable Copy() const{return PTable(new name(*this));} \
name():base(){Name=#name;

#define DECLARE_FIELD(baseclass,ntype,name,type,size, attr,default_,check) \
{ \
PField field(new baseclass(ntype, name, type, size, attr, default_, check)); \
FieldDefine.push_back(field); \
Fields[name] = field;}

#define DECLARE_TABLE_CLASS_END() }};