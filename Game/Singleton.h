#pragma once
#include <memory>
#include <mutex>

template<class T>
class Singleton
{
public:
	virtual ~Singleton() {};
	static T& GetInstance();

	Singleton(const Singleton& src) = delete;
	Singleton& operator=(const Singleton& rhs) = delete;
protected:
	inline Singleton() {}
private:
	static std::unique_ptr<T> instance_;
	static std::once_flag once_flag_;
};

template<class T>
std::unique_ptr<T> Singleton<T>::instance_ = nullptr;
template<class T>
std::once_flag Singleton<T>::once_flag_;

template<class T>
inline T& Singleton<T>::GetInstance()
{
	// 한번만 호출되는것을 보장해 준다.
	std::call_once(once_flag_,
		[]{
			instance_.reset(new T());
		});
	return *instance_;
}
