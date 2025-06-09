#ifndef SCOPEDASSIGN_HH
#define SCOPEDASSIGN_HH

#include <utility>

/** Assign new value to some variable and restore the original value
  * when this object goes out of scope.
  */
template<typename T> class ScopedAssign
{
public:
	ScopedAssign(const ScopedAssign&) = delete;
	ScopedAssign(ScopedAssign&&) = delete;
	ScopedAssign& operator=(const ScopedAssign&) = delete;
	ScopedAssign& operator=(ScopedAssign&&) = delete;

	template<typename T2 = T>
	[[nodiscard]] ScopedAssign(T& var_, T2&& newValue)
		: var(var_), oldValue(std::exchange(var, std::forward<T2>(newValue)))
	{
	}

	~ScopedAssign()
	{
		var = std::move(oldValue);
	}

private:
	T& var;
	T oldValue;
};

#endif
