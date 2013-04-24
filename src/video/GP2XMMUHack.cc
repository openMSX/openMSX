#include "GP2XMMUHack.hh"
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>

namespace openmsx {

GP2XMMUHack::GP2XMMUHack()
{
	// some other program may have forgotton to unload (a different
	// version of) this module
	unloadModule();

	loadModule();
}

GP2XMMUHack::~GP2XMMUHack()
{
	unloadModule();
}

GP2XMMUHack& GP2XMMUHack::instance()
{
	static GP2XMMUHack oneInstance;
	return oneInstance;
}

void GP2XMMUHack::loadModule()
{
	// TODO path
	system("/sbin/insmod mmuhack.o");
}

void GP2XMMUHack::unloadModule()
{
	system("/sbin/rmmod mmuhack");
}

void GP2XMMUHack::patchPageTables()
{
	int mmufd = open("/dev/mmuhack", O_RDWR);
	if (mmufd < 0) {
		std::cerr << "mmuhack: Failed to patch pagetables" << std::endl;
		return;
	}
	close(mmufd);
	std::cerr << "mmuhack: Succesfully patched pagetables" << std::endl;
}

void GP2XMMUHack::flushCache(void* start_address, void* end_address, int flags)
{
	// arguments are passed via registers
	register void* beg asm("r0") = start_address;
	register void* end asm("r1") = end_address;
	register int   flg asm("r2") = flags;

#ifdef __ARM_EABI__
	// EABI is only support starting from kernel 2.6.16.
	// This page has some more info:
	//   http://wiki.debian.org/ArmEabiPort

	// system-call number passed via r7
	register int scno asm("r7") = 0xf0002; // sys_cacheflush
	asm volatile (
		"swi 0x0"
		: "=r" (beg)
		: "0"  (beg)
		, "r"  (end)
		, "r"  (flg)
		, "r"  (scno)
	);
#else
	// system-call number passed via swi instruction
	asm volatile (
		"swi 0x9f0002  @ sys_cacheflush"
		: "=r" (beg)
		: "0"  (beg)
		, "r"  (end)
		, "r"  (flg)
	);
#endif
}

} // namespace openmsx
