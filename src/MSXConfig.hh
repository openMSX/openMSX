// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>

namespace MSXConfig
{

class Config
{
public:

	// a parameter is the same for all backends:
	class Parameter
	{
	public:
		Parameter(const std::string &name, const std::string &value, const std::string &clasz);
		~Parameter();
	private:
		Parameter(); // block usage
		Parameter(const Parameter &); // block usage
		Parameter &operator=(const Parameter &); // block usage

	public:
		const bool getAsBool() const;
		const int getAsInt() const ;
		const uint64 getAsUint64() const;
		// please let me know what types are needed exactly
		
		const std::string name;
		const std::string value;
		const std::string clasz;
	};

	Config();
	virtual ~Config();

	virtual bool isDevice()=0;
	virtual const std::string &getType()=0;
	virtual const std::string &getId()=0;
};

class Backend
{
public:
	void loadFile(const std::string &filename)=0;
	void saveFile()=0;
	void saveFile(const std::string &filename)=0;

	Config* getConfigById(const std::string &type)=0;

	/**
	 * backend factory
	 * create a backend
	 */
	static Backend* createBackend(const std::string &name);

private:
	// only I create instances of me
	Backend();
	virtual ~Backend();

};

}; // end namespace MSXConfig

#endif
