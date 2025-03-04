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

	[[nodiscard]] ScopedAssign(T& var_, T newValue)
		: var(var_)
	{
		oldValue = var;
		var = std::move(newValue);
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
