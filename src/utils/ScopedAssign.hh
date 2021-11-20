#ifndef SCOPEDASSIGN_HH
#define SCOPEDASSIGN_HH

/** Assign new value to some variable and restore the original value
  * when this object goes out of scope.
  */
template<typename T> class ScopedAssign
{
public:
	ScopedAssign(T& var_, T newValue)
		: var(var_)
	{
		oldValue = var;
		var = newValue;
	}
	~ScopedAssign()
	{
		var = oldValue;
	}
private:
	T& var;
	T oldValue;
};

#endif
