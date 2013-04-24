#if 0

#include "CircularBuffer.hh"
#include <iostream>

int main()
{
	openmsx::CircularBuffer<int, 2> buf;
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

	buf[1] = 35;
	assert(buf[0] == 17);
	assert(buf[1] == 35);
	buf[0] = 27;
	assert(buf[0] == 27);
	assert(buf[1] == 35);

	int a = buf.removeBack();
	assert(a == 35);
	assert(buf.size() == 1);
	assert(buf[0] == 27);

	int b = buf.removeFront();
	assert(b == 27);
	assert(buf.isEmpty());

	std::cout << "Test passed!" << std::endl;
}

#endif
