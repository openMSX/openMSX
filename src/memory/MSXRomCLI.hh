// $Id$

#ifndef __MSXROMCLI_HH__
#define __MSXROMCLI_HH__

#include "CommandLineParser.hh"


class MSXRomCLI : public CLIOption, public CLIFileType
{
	public:
		MSXRomCLI();
		virtual void parseOption(const std::string &option,
			std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp() const;
		virtual void parseFileType(const std::string &filename);
		virtual const std::string& fileTypeHelp() const;
	
	private:
		int cartridgeNr;
};

class MSXRomCLIPost : public CLIPostConfig
{
	public:
		MSXRomCLIPost(const std::string &arg);
		virtual ~MSXRomCLIPost() {}
		virtual void execute(MSXConfig *config);
	protected:
		int ps, ss;
		const std::string arg;
};
class MSXRomPostName : public MSXRomCLIPost
{
	public:
		MSXRomPostName(int slot, const std::string &arg);
		virtual ~MSXRomPostName() {}
		virtual void execute(MSXConfig *config);
	private:
		int slot;
};
class MSXRomPostNoName : public MSXRomCLIPost
{
	public:
		MSXRomPostNoName(const std::string &arg);
		virtual ~MSXRomPostNoName() {}
		virtual void execute(MSXConfig *config);
};

#endif
