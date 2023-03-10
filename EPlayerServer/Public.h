#pragma once
#include <string>
#include <string.h>
class Buffer :public std::string
{//string的子类，做了封装更好用
public:
	Buffer() :std::string() {}//默认构造函数，它和下面的函数都调用了父类（基类std::string）的默认构造函数
	Buffer(size_t size) :std::string() { resize(size); }//继承父类string的成员函数 }//调用了基类的构造函数以外还改变了大小
	Buffer(const std::string& str) :std::string(str) {}//提供自动转换string到Buffer对象的构造函数
	Buffer(const char* str) :std::string(str) {}//提供自动转换char*到Buffer对象的构造函数
	Buffer(const char* str, size_t length) :std::string() {
		resize(length);//继承父类string的成员函数
		memcpy((char*)c_str(), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		long int len = end - begin;//左闭右开，所以长度不用加1
		if (len > 0) {
			resize(len);
			memcpy((char*)c_str(), begin, len);
		}
	}
	//注意：定义构造函数的同时还需要重新定义默认构造函数
	operator char* () { return (char*)c_str(); }//允许强制转换
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* () const { return (char*)c_str(); }//允许常量对象作为函数参数参与强制转换（隐式声明this）
	operator const char* () const { return c_str(); }//允许常量类型的强制转换并返回常量类型指针
	operator void* () { return (void*)c_str(); }
	operator const void* () const { return (void*)c_str(); }
};