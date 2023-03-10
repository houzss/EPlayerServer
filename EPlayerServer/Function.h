#ifndef FUNCTIOND
#define FUNCTIOND

#include <unistd.h>//注意这里需要在这个项目附加目录加上当前目录"."才能找到这个头文件
#include <sys/types.h>//类型定义
#include <functional>
//对于一个函数:虚函数特性和模板函数特性不能同时存在,
//但是一个模板类可以有虚函数
class CSocketBase;
class Buffer;
class CFunctionBase //通过定义基类防止CProcess中m_func被传染为模板函数
{
public:
    virtual ~CFunctionBase() {}
    virtual int operator()() { return -1; };
    virtual int operator()(CSocketBase*) { return -1; };//这两个函数只在业务逻辑上有用,设置为直接return-1,使得后面的子类不实现问题也不大
    virtual int operator()(CSocketBase*, const Buffer&) { return -1; };//这两个函数只在业务逻辑上有用,设置为直接return-1,使得后面的子类不实现问题也不大
};

template<typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase
{
public:
    CFunction(_FUNCTION_ func, _ARGS_... args)//这里的func一定是外面传来的函数所在地址,所以外面的func传入都是带上&func(取地址),因为不知道这个函数对应的对象,传入复制的类成员函数没有道理(不知道对应哪个对象,不能用)
        :m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)//forward表示原样转发，()...表示参数展开 std::forward<T>(u)有两个参数：T 与 u。当T为左值引用类型时，u将被转换为T类型的左值，否则u将被转换为T类型右值。如此定义std::forward是为了在使用右值引用参数的函数模板中解决参数的完美转发问题。
        //bind绑定类成员函数时，第一个参数表示对象的成员函数的指针(必须显式地指定&,因为编译器不会将对象的成员函数隐式转换成函数指针)，第二个参数表示对象的地址(使用对象成员函数的指针时，必须要知道该指针属于哪个对象，因此第二个参数为对象的地址 &base)。
    {}//上面m_binder前面的冒号代表初始化
    virtual ~CFunction() {}
    virtual int operator()() {
        return m_binder();//函数执行时(*m_func)(); <=>  执行了m_binder()生成的可调用对象;
    }// return  std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type
  //  virtual int operator()(CSocketBase* pClient) {
  //      return m_binder(pClient);
  //  }// std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type
  //  virtual int operator()(CSocketBase* pClient, const Buffer& data) {
		//return m_binder(pClient, data);
  //  }
    //std::bind用于给一个可调用对象绑定参数。可调用对象包括函数对象（仿函数）、函数指针、函数引用、成员函数指针和数据成员指针。
    typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;//相当于反传
};

#endif