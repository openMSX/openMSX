// $Id$

#ifndef __MSXFILEPATH_HH__
#define __MSXFILEPATH_HH__

#include "MSXConfig.hh"

namespace MSXConfig
{

class FilePath: public virtual CustomConfig
{
public:
	FilePath();
	virtual ~FilePath();

	virtual void dump()=0;

	/**
	 * is the client internet enabled?
	 */
	virtual bool internet()=0;

	/**
	 * the paths
	 */
	std::list<std::string*> paths;

	/**
	 * want caching?
	 */
	virtual bool caching()=0;
	/**
	 * cachedir, raises exception if no caching
	 */
	virtual const std::string &cachedir()=0;
	/**
	 * cache expire?
	 */
	virtual bool cacheExpire()=0;
	/**
	 * cache expire # days, only meaningfull when
	 * caching is enabled
	 */
	virtual int cacheDays()=0;
	/**
	 * directory for persistant files like SRAM's
	 */
	virtual const std::string &persistantdir()=0;
private:
	FilePath(const FilePath &foo); // block usage
	FilePath &operator=(const FilePath &foo); // block usage
};

}; // end namespace MSXConfig

#endif // defined __MSXFILEPATH_HH__
