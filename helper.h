#pragma once
#include "VKStart.h"

// 情况1：根据函数返回值决定是否抛出异常
#ifdef VK_RESULT_THROW
class result_t
{
private :
	VkResult result;
public :
	static void(*callback_throw)(VkResult);
	result_t(VkResult result) :result(result) {}
	result_t(result_t&& other) noexcept :result(other.result) { other.result = VK_SUCCESS; }
    ~result_t() noexcept(false)
    {
        if (uint32_t(result) < VK_RESULT_MAX_ENUM)
            return;
        if (callback_throw)
            callback_throw(result);
        throw result;
    }
    operator VkResult()
    {
        VkResult result = this->result;
        this->result = VK_SUCCESS;
        return result;
    }
    inline void(*result_t::callback_throw)(VkResult);
};

// 情况2：若抛弃函数返回值，让编译器发出警告
#elifdef VK_RESULT_NODISCARD
struct [[nodiscard]] result_t
{
    VkResult result;
    result_t(VkResult result) :result(result) {}
    operator VkResult() const { return result; }
};
// 在本文件中关闭弃值提醒
#pragma warning(disable:4834)
#pragma warning(disable:6031)

// 情况3：什么也不做
#else
using result_t = VkResult;
#endif

template<typename T>
class arrayRef
{
private :
    T* const pArray = nullptr;
    size_t count = 0;

public :
    arrayRef() = default;
    // 从单个对象进行构造
    arrayRef(T& data) : pArray(&data), count(1) {}
    template<typename R>
    arrayRef(R&& range) requires requires(R r)
    {
        requires std::ranges::contiguous_range<R>;
        requires std::ranges::sized_range<R>;
        requires std::ranges::borrowed_range<R>;
        requires std::convertible_to<decltype(std::ranges::data(r)), T*>;
        requires sizeof(std::iter_value_t<R>) == sizeof(T);
    } :pArray(std::ranges::data(range)), count(std::ranges::size(range)) {}
    // 从指针和元素个数构造
    arrayRef(T *data, size_t count) : pArray(data), count(count) {}
    //若T带const修饰，兼容从对应的无const修饰版本的arrayRef构造
    arrayRef(const arrayRef<std::remove_const_t<T>>& other) :pArray(other.Pointer()), count(other.Count()) {}
    // Getter
    T* Pointer() const { return pArray; }
    size_t Count() const { return count; }
    // Const Function
    T& operator[](size_t index) const { return pArray[index]; }
    T* begin() const { return pArray; }
    T* end() const { return pArray + count; }
    // 禁止复制 / 移动赋值
    arrayRef& operator=(const arrayRef&) = delete;
};

#ifndef NDEBUG
#define ENABLE_DEBUG_MESSENGER true
#else
#define ENABLE_DEBUG_MESSENGER false
#endif

// 用于析构器中销毁 Vulkan 对象的宏，该宏调用相应 Destroy 函数后将 handle 设置为 VK_NULL_HANDLE，以防止重复析构
#define DestroyHandleBy(Func) if (handle) { Func(graphicsBase::Base().Device(), handle, nullptr); handle = VK_NULL_HANDLE; }
// 用于移动构造器中的宏，该宏复制来自另一对象的 handle 后，将另一对象的 handle 设置为 VK_NULL_HANDLE，转移析构权限
#define MoveHandle handle = other.handle; other.handle = VK_NULL_HANDLE;
// 移动赋值函数
#define DefineMoveAssignmentOperator(type) type& operator=(type&& other) { this->~type(); MoveHandle; return *this; }
// 宏定义转换函数，通过返回 handle 将封装类型对象隐式转换到被封装 handle 的原始类型
#define DefineHandleTypeOperator operator decltype(handle)() const { return handle; }
// 宏定义转换函数，用于取得被封装 handle 的地址
#define DefineAddressFunction const decltype(handle)* Address() const { return &handle; }

constexpr struct outStream_t
{
    static std::stringstream ss;
    struct returnedStream_t
    {
        returnedStream_t operator<<(const std::string& string) const
        {
            ss << string;
            return {};
        }
    };
    returnedStream_t operator<<(const std::string& string) const
    {
        ss.clear();
        ss << string;
        return {};
    }
} outStream;
inline std::stringstream outStream_t::ss;

