// $Id$

#ifndef __MSXROMCLI_HH__
#define __MSXROMCLI_HH__

#include "CommandLineParser.hh"

namespace openmsx {

class MSXRomCLI : public CLIOption, public CLIFileType
{
public:
	MSXRomCLI();
	virtual bool parseOption(const string& option,
	                         list<string>& cmdLine);
	virtual const string& optionHelp() const;
	virtual void parseFileType(const string& filename);
	virtual const string& fileTypeHelp() const;

private:
	int cartridgeNr;
};

class MSXRomCLIPost : public CLIPostConfig
{
public:
	MSXRomCLIPost(const string& arg);
	virtual ~MSXRomCLIPost() {}
	virtual void execute(MSXConfig& config);
protected:
	int ps, ss;
	const string arg;
};
class MSXRomPostName : public MSXRomCLIPost
{
public:
	MSXRomPostName(int slot, const string& arg);
	virtual ~MSXRomPostName() {}
	virtual void execute(MSXConfig& config);
private:
	int slot;
};
class MSXRomPostNoName : public MSXRomCLIPost
{
public:
	MSXRomPostNoName(const string& arg);
	virtual ~MSXRomPostNoName() {}
	virtual void execute(MSXConfig& config);
};

} // namespace openmsx

#endif
