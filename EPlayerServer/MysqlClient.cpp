#include "MysqlClient.h"
#include "Logger.h"

int CMysqlClient::Connect(const KeyValue& args)
{
	if (m_bInit == true) return -1;//已经初始化，直接返回-1（禁止重复初始化）
	MYSQL* ret = mysql_init(&m_db);
	if (ret == NULL) return -2;
	
	ret = mysql_real_connect(&m_db,
		args.at("host"), args.at("user"),
		args.at("passwd"), args.at("db"),
		atoi(args.at("port")), NULL, 0);
	//mysql_select_db(&m_db, );
	if ((ret == NULL) && (mysql_errno(&m_db) != 0)) {//错误，需要关闭连接且清零
		printf("func: %s Mysql connect error %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
		TRACEE("errno:%d msg:%s", mysql_errno(&m_db), mysql_error(&m_db));
		mysql_close(&m_db);
		bzero(&m_db, sizeof(m_db));
		return -3;
	}
	m_bInit = true;
	return 0;
}

int CMysqlClient::Exec(const Buffer& sql)
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, sql, sql.size());
	if (ret != 0) {
		printf(" %s(%d):<%s> Mysql exec error%d %s\n", __FILE__, __LINE__, __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
		TRACEE("errno:%d msg:%s", mysql_errno(&m_db), mysql_error(&m_db));
		return -2;
	}
	return 0;
}

int CMysqlClient::Exec(const Buffer& sql, Result& result, const _Table_& table)
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, sql, sql.size());
	if (ret != 0) {
		printf(" %s(%d):<%s> Mysql exec error%d %s\n", __FILE__, __LINE__, __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
		TRACEE("errno:%d msg:%s", mysql_errno(&m_db), mysql_error(&m_db));
		return -2;
	}
	MYSQL_RES* mysql_result = mysql_store_result(&m_db);
	MYSQL_ROW row;
	unsigned num_fields = mysql_num_fields(mysql_result);
	while ((row = mysql_fetch_row(mysql_result)) != NULL) {
		PTable pt = table.Copy();//复制表结构
		for (unsigned i = 0; i < num_fields; i++) {
			if (row[i] != NULL) {//char* row[i]可能为空
				pt->FieldDefine[i]->LoadFromStr(row[i]);//传入数据
			}
		}
		result.push_back(pt);//存储结果
	}
	return 0;
}

int CMysqlClient::StartTransaction()
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, "BEGIN", 6);//sql = 'BEGIN"\0"'
	if (ret != 0) {
		printf(" %s(%d):<%s> Mysql BEGIN error%d %s\n", __FILE__, __LINE__, __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
		TRACEE("errno:%d msg:%s", mysql_errno(&m_db), mysql_error(&m_db));
		return -2;
	}
	return 0;
}

int CMysqlClient::CommitTransaction()
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, "COMMIT", 7);
	if (ret != 0) {
		printf(" %s(%d):<%s> Mysql COMMIT error%d %s\n", __FILE__, __LINE__, __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
		TRACEE("errno:%d msg:%s", mysql_errno(&m_db), mysql_error(&m_db));
		return -2;
	}
	return 0;
}

int CMysqlClient::RollbackTransaction()
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, "ROLLBACK", 9);
	if (ret != 0) {
		printf(" %s(%d):<%s> Mysql ROLLBACK error%d %s\n", __FILE__, __LINE__, __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));
		TRACEE("errno:%d msg:%s", mysql_errno(&m_db), mysql_error(&m_db));
		return -2;
	}
	return 0;
}

int CMysqlClient::Close()
{
	if (m_bInit) {
		m_bInit = false;
		mysql_close(&m_db);
		bzero(&m_db, sizeof(m_db));
	}
	return 0;
}

bool CMysqlClient::isConnected()
{
	return m_bInit;
}

_mysql_table_::_mysql_table_(const _mysql_table_& table)
{
	Database = table.Database;
	Name = table.Name;
	for (size_t i = 0; i < table.FieldDefine.size(); i++)
	{
		PField field = PField(new _mysql_field_(*
			(_mysql_field_*)table.FieldDefine[i].get()));
		FieldDefine.push_back(field);
		Fields[field->Name] = field;
	}
}

_mysql_table_& _mysql_table_::operator=(const _mysql_table_& table)
{
	if (this != &table) {
		Database = table.Database;
		Name = table.Name;
		for (size_t i = 0; i < table.FieldDefine.size(); i++)
		{
			PField field = PField(new _mysql_field_(*
				(_mysql_field_*)table.FieldDefine[i].get()));
			FieldDefine.push_back(field);
			Fields[field->Name] = field;
		}
	}
	return *this;
}

Buffer _mysql_table_::Create()
{//CREATE TABLE IF NOT EXISTS 表全名 (列定义,..., 
 //PRIMARY KEY 主键类名, UNIQUE INDEX `列名_UNIQUE` (列名 ASC/DESC) VISIBLE) 
 //(ENGINE=InnoDB DEFAULT CHARSET=utf8; ENGINE 设置存储引擎，CHARSET 设置编码，可选) 中间的default不变
 // 关于引擎：MYISAM 节约空间，速度较快；INNODB 安全性高，事务的处理，多表多用户操作
 //和sqlite3区别在于列定义不一样，主键和唯一属性都在后面
	Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + " (\r\n";
	for (unsigned i = 0; i < FieldDefine.size(); i++) {
		if (i > 0) sql += ",\r\n";
		sql += FieldDefine[i]->Create();
		if (FieldDefine[i]->Attr & PRIMARY_KEY) {
			sql += ",\r\n PRIMARY KEY (`" + FieldDefine[i]->Name + "`)";
		}
		if (FieldDefine[i]->Attr & UNIQUE) {
			sql += ",\r\n UNIQUE INDEX `" + FieldDefine[i]->Name + "_UNIQUE` (";
			sql += (Buffer)*FieldDefine[i] + "ASC) VISIBLE ";//从实现里看的出来，这里的(Buffer)*FieldDefine[i]实际上就是FieldDefine[i]->Name加上双引号，为什么要不一样？
		}
	}
	sql += ");";
	//printf("sql = %s\n", (char*)sql);
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Drop()
{
	Buffer sql = "DROP TABLE IF EXISTS " + (Buffer)*this + ";";//TODO:注意这里我加了一个IF EXISTS
	printf("sql = %s\n", (char*)sql);
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Insert(const _Table_& values)
{//INSERT INTO `表全名` (列1，列2，...,列n)  
 // VALUES (值1,...,值n) 
	bool isFirst = true;//首列
	Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
	Buffer ColNamesql = "", ColValsql = "";
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {//O(2N)改成O(N)
		if (values.FieldDefine[i]->Condition & SQL_INSERT) {
			if (!isFirst) {
				ColNamesql += ',';
				ColValsql += ',';
			}
			else isFirst = false;
			ColNamesql += (Buffer)*values.FieldDefine[i];
			ColValsql += values.FieldDefine[i]->toSqlStr();
		}
	}
	sql += ColNamesql + " ) VALUES ( " + ColValsql + " );";
	printf("sql = %s\n", (char*)sql);
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Delete(const _Table_& values, const Buffer& condition)
{//DELETE FROM 表全名 WHERE 条件
	Buffer sql = "DELETE FROM " + (Buffer)*this + " ";//mysql才为DELETE * FROM
	/*Buffer Wheresql = "";
	bool isFirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_CONDITION) {
			if (!isFirst) {
				Wheresql += " AND ";
			}
			else isFirst = false;
			Wheresql += (Buffer)*values.FieldDefine[i] + " = " + values.FieldDefine[i]->toSqlStr();
		}
	}*/
	/*if (Wheresql.size()) {
		sql += " WHERE " + Wheresql;
	}*/
	if (condition.size() > 0)
		sql += "WHERE " + condition;

	sql += " ;";
	printf("sql = %s\n", (char*)sql);
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Modify(const _Table_& values, const Buffer& condition)
{//UPDATE 表全名 SET 列1=值1,...,列n=值n [WHERE 条件];
	bool isFirst = true;//首列
	Buffer sql = "UPDATE " + (Buffer)*this + " SET ";
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_MODIFY) {
			if (!isFirst) {
				sql += ',';
			}
			else isFirst = false;
			sql += (Buffer)*values.FieldDefine[i] + " = " + values.FieldDefine[i]->toSqlStr();
		}
	}
	/*Buffer Wheresql = "";
	isFirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_CONDITION) {
			if (!isFirst) {
				Wheresql += " AND ";
			}
			else isFirst = false;
			Wheresql += (Buffer)*values.FieldDefine[i] + " = " + values.FieldDefine[i]->toSqlStr();
		}
	}
	if (Wheresql.size()) {
		sql += " WHERE " + Wheresql;
	}*/
	if (condition.size() > 0)
		sql += "WHERE " + condition;

	sql += " ;";
	printf("sql = %s\n", (char*)sql);
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Query(const Buffer& condition)
{//SELECT 列名1,列名2,...,列名n FROM 表全名 (可能带条件where );
	Buffer sql = "SELECT ";
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		if (i > 0) sql += ',';
		sql += '`' + FieldDefine[i]->Name + "` ";
	}
	sql += " FROM " + (Buffer)*this + " ";
	if (condition.size() > 0)
		sql += "WHERE " + condition;
	sql += ";";

	printf("sql = %s\n", (char*)sql);
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

PTable _mysql_table_::Copy() const
{
	return PTable(new _mysql_table_(*this));
}

void _mysql_table_::ClearFieldCondition()
{
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		FieldDefine[i]->Condition = 0;
	}
}

_mysql_table_::operator const Buffer() const
{
	Buffer Head;
	if (Database.size())//指定了数据库
		Head = '`' + Database + "`.";
	return Head + '`' + Name + '`';
}

_mysql_field_::_mysql_field_():_Field_()
{
	nType = TYPE_NULL;
	Value.Double = 0.0;
}

_mysql_field_::_mysql_field_(int ntype, const Buffer& name, const Buffer& type, const Buffer& size, unsigned attr, const Buffer& default_, const Buffer& check)
{
	Name = name;
	Type = type;
	Size = size;
	Attr = attr;
	Default = default_;
	Check = check;
	nType = ntype;
	switch (nType) {
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.Str = new Buffer();
		break;
	}
}

_mysql_field_::_mysql_field_(const _mysql_field_& field)
{
	Name = field.Name;
	Type = field.Type;
	Size = field.Size;
	Attr = field.Attr;
	Default = field.Default;
	Check = field.Check;
	nType = field.nType;
	switch (nType) {
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.Str = new Buffer();
		*Value.Str = *field.Value.Str;
		break;
	}
}

_mysql_field_::~_mysql_field_()
{
	switch (nType) {
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		if (Value.Str) {//存在
			Buffer* p = Value.Str;
			Value.Str = NULL;
			delete p;
		}
		break;
	}
}

Buffer _mysql_field_::Create()
{
	Buffer sql = "`" + Name +"`" + Type + Size + " ";
	if (Attr & NOT_NULL) {
		sql += "NOT NULL";
	}
	else {
		sql += "NULL";//可为空也要加上
	}
	//BLOB TEXT GEOMETRY(坐标) JSON不能有默认值
	if ((Attr & DEFAULT) &&
		(Default.size() > 0)&&
		(Type != "BLOB") && 
		(Type != "TEXT") && 
		(Type != "GEOMETRY") && 
		(Type != "JSON")) 
	{
		sql += " DEFAULT \"" + Default + "\" ";
	}
	//UNIQUE PRIMARY_KEY外面处理，MYSQL在定义时不支持CHECK
	if (Attr & AUTOINCREMENT) {
		sql += " AUTO_INCREMENT ";
	}
	return sql;
}

void _mysql_field_::LoadFromStr(const Buffer& str)
{
	switch (nType)
	{
	case TYPE_NULL:
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		Value.Int = atoi(str);
		break;
	case TYPE_REAL:
		Value.Double = atof(str);
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
		*Value.Str = str;
		break;
	case TYPE_BLOB://十六进制
		*Value.Str = Str2Hex(str);
		break;
	default:
		printf("type=%d wrong\n", nType);
		//TRACEW("type=%d wrong", nType);
		break;
	}
}

Buffer _mysql_field_::toEqualExp() const
{
	Buffer sql = (Buffer)*this + " = ";
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Int;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB://十六进制也以字符串Buffer存储，所以和Text和Varchar类型一样
		sql += '"' + *Value.Str + "\" ";
		break;
	default:
		printf("type=%d wrong\n", nType);
		//TRACEW("type=%d wrong", nType);
		break;
	}
	return sql;
}

Buffer _mysql_field_::toSqlStr() const
{
	Buffer sql = "";
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Int;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB://十六进制也以字符串Buffer存储，所以和Text和Varchar类型一样
		sql += '"' + *Value.Str + "\" ";
		break;
	default:
		printf("type=%d wrong\n", nType);
		//TRACEW("type=%d wrong", nType);
		break;
	}
	return sql;
}

_mysql_field_::operator const Buffer() const
{
	return '`' + Name + '`';
}

Buffer _mysql_field_::Str2Hex(const Buffer& data) const
{
	const char* hex = "0123456789ABCDEF";
	std::stringstream ss;
	for (char ch : data) {
		ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];//高位和低位取出转成hex中的十六进制格式
	}
	return ss.str();
}
