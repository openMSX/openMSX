// $Id$

#ifndef GP2XMMUHACK_HH
#define GP2XMMUHACK_HH

namespace openmsx {

/** This is a helper class to apply the 'mmuhack' on GP2X. (It's really
  * known under this name.) See here for more info:
  *   http://wiki.gp2x.org/wiki/Using_the_upper_32MB_of_memory
  */
class GP2XMMUHack
{
public:
	static GP2XMMUHack& instance();

	void patchPageTables();
	void flushCache(void* start_address, void* end_address, int flags);

private:
	GP2XMMUHack();
	~GP2XMMUHack();

	void loadModule();
	void unloadModule();
};

} // namespace openmsx

#endif
