// $Id$

#ifndef __XMLFILEPATH_HH__
#define __XMLFILEPATH_HH__

#include "XMLConfig.hh"
#include "MSXFilePath.hh"

/*

Sample:

<!-- ~ is replaced by $HOME when first char -->

<filepath>
    <internet enabled="true"/>
    <datafiles>
        <path>.</path>
        <path>ROMS</path>
        <path>DISKS</path>
        <path>http://www.worldcity.nl/~andete/MSX</path>
    </datafiles>
    <cache enabled="true">
        <path>~/.openmsx/cache</path>
        <expire enabled="true" days="2"/>
    </cache>
    <persistantdata>
        <path>~/.openmsx/persistant</path>
    </persistantdata>
</filepath>

*/

namespace XMLConfig
{

class FilePath: public virtual MSXConfig::FilePath, public virtual XMLConfig::CustomConfig
{
public:
	FilePath();
	virtual ~FilePath();

	virtual void dump();

	virtual void backendInit(XML::Element *e);

	virtual bool internet();
	virtual bool caching();
	virtual const std::string &cachedir();
	virtual bool cacheExpire();
	virtual int cacheDays();
	virtual const std::string &persistantdir();

private:
	FilePath(const FilePath &foo); // block usage
	FilePath &operator=(const FilePath &foo); // block usage

	bool internet_;
	bool caching_;
	bool cacheexpire_;
	int cachedays_;
	std::string cachedir_;
	std::string persistantdir_;

	void handleInternet(XML::Element *e);
	void handleDatafiles(XML::Element *e);
	void handleCache(XML::Element *e);
	void handlePersistantdata(XML::Element *e);

	/// do ~ replacement, changes path in place
	static const std::string &expandPath(std::string &path);
};

}; // end namespace XMLConfig

#endif // defined __XMLFILEPATH_HH__
