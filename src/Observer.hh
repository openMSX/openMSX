// $Id$

#ifndef OBSERVER_HH
#define OBSERVER_HH

namespace openmsx {

template <typename T> class Observer
{
	public:
		virtual void update(const T& subject) = 0;

	protected:
		virtual ~Observer() {}
};

}

#endif
