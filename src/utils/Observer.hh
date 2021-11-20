#ifndef OBSERVER_HH
#define OBSERVER_HH

namespace openmsx {

/**
 * Generic Gang-of-Four Observer class, templatized edition.
 */
template<typename T> class Observer
{
public:
	Observer(const Observer&) = delete;
	Observer& operator=(const Observer&) = delete;

	virtual void update(const T& subject) noexcept = 0;
	virtual void subjectDeleted(const T& /*subject*/) { /*nothing*/ }

protected:
	Observer() = default;
	~Observer() = default;
};

} // namespace openmsx

#endif
