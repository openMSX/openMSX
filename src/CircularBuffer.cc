// $Id$

#include "CircularBuffer.hh"


#if 0
namespace openmsx {

int main()
{
	CircularBuffer<int, 2> buf;
	assert(buf.isEmpty());
	assert(!buf.isFull());
	assert(buf.size() == 0);

	buf.addBack(15);
	assert(!buf.isEmpty());
	assert(!buf.isFull());
	assert(buf.size() == 1);
	assert(buf[0] == 15);

	buf[0] = 25;
	assert(buf[0] == 25);

	buf.addFront(17);
	assert(!buf.isEmpty());
	assert(buf.isFull());
	assert(buf.size() == 2);
	assert(buf[0] == 17);
	assert(buf[1] == 25);

	buf.removeBack();
	assert(buf[0] == 17);
	
	buf.removeFront();
	assert(buf.isEmpty());
}

} // namespace openmsx
#endif
