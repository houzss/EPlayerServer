#include "Sqlite3Client.h"
#include "Logger.h"
int CSqlite3Client::Connect(const KeyValue& args)
{
    auto it = args.find("host");
    if (it == args.end()) return -1;//找不到域名host
    if (m_db != NULL) return -2;
    int ret = sqlite3_open(it->second, &m_db);//打开数据库
    if (ret != 0) {
        TRACEE("connect failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -3;
    }
    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql)
{
    //printf("sql={%s}\n", (char*)sql);
    if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, sql, NULL, this, NULL);
    if (ret != SQLITE_OK) {
        printf("sql={%s}\n", (char*)sql);
        printf("sql_exec failed:%d [%s]\n", ret, sqlite3_errmsg(m_db));
        TRACEE("sql={%s}", sql);
        TRACEE("sql_exec failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table)
{
    char* errmsg = NULL;
    if (m_db == NULL) return -1;
    ExecParam param(this, result, table);//能用临时变量的原因是只在这个函数使用，用完就可以随着函数结束自动执行析构函数了（默认生成的），只释放变量名不释放真实使用地址
    int ret = sqlite3_exec(m_db, sql, //对数据库文件描述符执行sql操作，将返回的count,value和names传入回调函数ExecCallback
        &CSqlite3Client::ExecCallback,
        (void*)&param,//param会作为ExecCallback的arg参数
        &errmsg);
    if (ret != SQLITE_OK) {
        printf("sql={%s}\n", (char*)sql);
        printf("sql_exec failed:%d [%s]\n", ret, sqlite3_errmsg(m_db));
        TRACEE("sql={%s}", sql);
        TRACEE("sql_exec failed:%d [%s]", ret, errmsg);
        if (errmsg) sqlite3_free(errmsg);
        return -2;
    }
    if (errmsg) sqlite3_free(errmsg);
    return 0;
}

int CSqlite3Client::StartTransaction()
{
    if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, "BEGIN TRANSACTION", 0, 0, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={BEGIN TRANSACTION}");
        TRACEE("sql_exec failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::CommitTransaction()
{
    if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, "COMMIT TRANSACTION", 0, 0, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={COMMIT TRANSACTION}");
        TRACEE("sql_exec failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::RollbackTransaction()
{
    if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, "ROLLBACK TRANSACTION", 0, 0, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={ROLLBACK TRANSACTION}");
        TRACEE("sql_exec failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::Close()
{
    if (m_db == NULL) return -1;
    int ret = sqlite3_close(m_db);
    if (ret != SQLITE_OK) {
        TRACEE("Close failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    m_db = NULL;
    return 0;
}

bool CSqlite3Client::isConnected()
{
	return m_db != NULL;
}

int CSqlite3Client::ExecCallback(void* arg, int count, char** values, char** names)
{
    ExecParam* param = (ExecParam*)arg;//转换回去
    return param->obj->ExecCallback(param->result, param->table, count, names, values);
}

int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{//逐键值找到对应列项修改（从value获取修改后的正确值）后加入结果list中
    PTable pTable = table.Copy();
    if (pTable == nullptr) {
        printf("table %s null error!\n", (const char*)(Buffer)table);
        TRACEE(
            "table %s null error!",(const char*)(Buffer)table);
        return -1;
    }
    for (int i = 0; i < count; i++) {
        Buffer name = names[i];//执行sql语句返回的属性名
        char* value = values[i];//执行sql语句返回的属性值
        auto it = pTable->Fields.find(name);//查看对应表是否存在这个名称
        if (it == pTable->Fields.end()) {//不存在则报错无法匹配
            printf("table %s unmatched error!\n", (const char*)(Buffer)table);
            TRACEE("table %s unmatched error!", (const char*)(Buffer)table);//不匹配
            return -2;
        }
        if (value != NULL) {
            it->second->LoadFromStr(value);//否则，若value非空则将结果加入对应列中
        }
    }
    result.push_back(pTable);//将列对象插入结果集中
    return 0;
}

_sqlite3_table_::_sqlite3_table_(const _sqlite3_table_& table)
{
    Database = table.Database;
    Name = table.Name;
    for (size_t i = 0; i < table.FieldDefine.size(); i++)
    {
        PField field = PField(new _sqlite3_field_(* 
            (_sqlite3_field_*)table.FieldDefine[i].get()));
        FieldDefine.push_back(field);
        Fields[field->Name] = field;
    }
}

_sqlite3_table_& _sqlite3_table_::operator=(const _sqlite3_table_& table)
{
    if (this != &table) {
        Database = table.Database;
        Name = table.Name;
        for (size_t i = 0; i < table.FieldDefine.size(); i++)
        {
            PField field = PField(new _sqlite3_field_(*
                (_sqlite3_field_*)table.FieldDefine[i].get()));
            FieldDefine.push_back(field);
            Fields[field->Name] = field;
        }
    }
    return *this;
}

Buffer _sqlite3_table_::Create()
{//CREATE TABLE IF NOT EXISTS 表全名 (列定义,...);
    //表全名 = 数据库.表名
    Buffer sql = "CREATE TABLE IF NOT EXISTS "+(Buffer)*this +"(\r\n";
    for (size_t i = 0; i < FieldDefine.size(); i++) {
        if (i > 0) sql += ',';
        sql += FieldDefine[i]->Create();
    }
    sql += ");";
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Drop()
{
    Buffer sql = "DROP TABLE IF EXISTS " + (Buffer)*this + ";";//TODO:注意这里我加了一个IF EXISTS
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Insert(const _Table_& values)
{//INSERT INTO 表全名 (列1，列2，...,列n)  
 // VALUES(值1,...,值n)
    bool isFirst = true;//首列
    Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
    Buffer ColNamesql = "",ColValsql = "";
    for (size_t i = 0; i < values.FieldDefine.size(); i++) {//O(2N)改成O(N)
        if (values.FieldDefine[i]->Condition&SQL_INSERT) {
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
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Delete(const _Table_& values, const Buffer& condition)
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
            Wheresql += (Buffer)*values.FieldDefine[i] +" = " + values.FieldDefine[i]->toSqlStr();
        }
    }*/
    /*if (Wheresql.size()) {
        sql += " WHERE " + Wheresql;
    }*/
    if (condition.size() > 0)
        sql += "WHERE " + condition;
    sql += ';';
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Modify(const _Table_& values, const Buffer& condition)
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
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

Buffer _sqlite3_table_::Query(const Buffer& condition)//TODO:带条件查询未实现
{//SELECT 列名1,列名2,...,列名n FROM 表全名 (可能带条件where ?);
    Buffer sql = "SELECT ";
    for (size_t i = 0; i < FieldDefine.size(); i++) {
        if (i > 0) sql += ',';
        sql += '"' + FieldDefine[i]->Name + "\" ";
    }
    sql += " FROM " + (Buffer)*this + " ";
    if (condition.size() > 0)
        sql += "WHERE " + condition;
    sql += ";";
 
    TRACEI("sql = %s", (char*)sql);
    return sql;
}

PTable _sqlite3_table_::Copy() const
{
    return PTable(new _sqlite3_table_(*this));
}

void _sqlite3_table_::ClearFieldCondition()//改自ClearFieldUsed
{
    for (size_t i = 0; i < FieldDefine.size(); i++) {
        FieldDefine[i]->Condition = 0;
    }
}

_sqlite3_table_::operator const Buffer() const
{
    Buffer Head;
    if (Database.size())//指定了数据库
        Head = '"' + Database + "\".";
    return Head + '"' + Name + '"';
}

_sqlite3_field_::_sqlite3_field_()
    :_Field_() {
    nType = TYPE_NULL;
    Value.Double = 0.0;
}

_sqlite3_field_::_sqlite3_field_(int ntype, const Buffer& name, const Buffer& type, const Buffer& size, unsigned attr, const Buffer& default_, const Buffer& check)
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

_sqlite3_field_::_sqlite3_field_(const _sqlite3_field_& field)
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

_sqlite3_field_::~_sqlite3_field_()
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

Buffer _sqlite3_field_::Create()
{//"名称" 类型 属性
    Buffer sql= '"' +Name+"\" " + Type +" ";
    if (Attr & NOT_NULL) sql += " NOT NULL ";
    if (Attr & DEFAULT) sql += " DEFAULT "+Default + " ";
    if (Attr & UNIQUE) sql += " UNIQUE ";
    if (Attr & PRIMARY_KEY) sql += " PRIMARY KEY ";
    if (Attr & CHECK) sql += " CHECK( " + Check +")";
    if (Attr & AUTOINCREMENT) sql += " AUTOINCREMENT ";
    return sql;
}

void _sqlite3_field_::LoadFromStr(const Buffer& str)
{
    switch(nType)
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
        TRACEW("type=%d wrong",nType);
        break;
    }
}

Buffer _sqlite3_field_::toEqualExp() const
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
        TRACEW("type=%d wrong", nType);
        break;
    }
    return sql;
}

Buffer _sqlite3_field_::toSqlStr() const
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
        TRACEW("type=%d wrong", nType);
        break;
    }
    return sql;
}

_sqlite3_field_::operator const Buffer() const
{
	return '"' + Name + '"';
}

Buffer _sqlite3_field_::Str2Hex(const Buffer& data) const
{
    const char* hex = "0123456789ABCDEF";
    std::stringstream ss;
    for (char ch : data) {
        ss << hex[(unsigned char)ch >> 4]<< hex[(unsigned char)ch & 0xF];//高位和低位取出转成hex中的十六进制格式
    }
    return ss.str();
}


