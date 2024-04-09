// This is a simplified / stripped-down version of:
//   https://github.com/TartanLlama/function_ref/blob/master/include/tl/function_ref.hpp

///
// function_ref - A low-overhead non-owning function
// Written in 2017 by Simon Brand (@TartanLlama)
//
// To the extent possible under law, the author(s) have dedicated all
// copyright and related and neighboring rights to this software to the
// public domain worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication
// along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
///

#ifndef FUNCTION_REF_HH
#define FUNCTION_REF_HH

#include <functional>
#include <type_traits>
#include <utility>

template<typename F> class function_ref;

template<typename R, typename... Args>
class function_ref<R(Args...)>
{
public:
	constexpr function_ref() noexcept = delete;

	template<typename F,
	         std::enable_if_t<!std::is_same_v<std::decay_t<F>, function_ref> &&
	                          std::is_invocable_r_v<R, F &&, Args...>>* = nullptr>
	constexpr function_ref(F&& f) noexcept
		: obj(const_cast<void*>(reinterpret_cast<const void*>(std::addressof(f))))
	{
		callback = [](void* o, Args... args) -> R {
			return std::invoke(*reinterpret_cast<std::add_pointer_t<F>>(o),
			                   std::forward<Args>(args)...);
		};
	}

	template<typename F,
	         std::enable_if_t<std::is_invocable_r_v<R, F &&, Args...>>* = nullptr>
	constexpr function_ref& operator=(F&& f) noexcept
	{
		obj = reinterpret_cast<void*>(std::addressof(f));
		callback = [](void* o, Args... args) {
			return std::invoke(*reinterpret_cast<std::add_pointer_t<F>>(o),
			                   std::forward<Args>(args)...);
		};
		return *this;
	}

	constexpr R operator()(Args... args) const
	{
		return callback(obj, std::forward<Args>(args)...);
	}

private:
	void* obj = nullptr;
	R (*callback)(void*, Args...) = nullptr;
};

template<typename R, typename... Args>
function_ref(R(*)(Args...)) -> function_ref<R(Args...)>;

#endif
