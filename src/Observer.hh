// $Id$

#ifndef OBSERVER_HH
#define OBSERVER_HH

namespace openmsx {

/**
 * Generic Gang-of-Four Observer class, templatized edition.
 */
template <typename T> class Observer
{
	public:
		virtual void update(const T& subject) = 0;

	protected:
		virtual ~Observer() {}
};

}

#endif
