// $Id$

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "CliExtension.hh"
#include "HardwareConfig.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "ReadDir.hh"

namespace openmsx {

CliExtension::CliExtension(CommandLineParser& cmdLineParser)
{
	cmdLineParser.registerOption("-ext", this);

	SystemFileContext context;
	const vector<string>& paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		string path = FileOperations::expandTilde(*it);
		createExtensions(path + "extensions/");
	}
}

CliExtension::~CliExtension()
{
}

bool CliExtension::parseOption(const string &option,
                               list<string> &cmdLine)
{
	string extension = getArgument(option, cmdLine);
	ExtensionsMap::const_iterator it = extensions.find(extension);
	if (it != extensions.end()) {
		SystemFileContext context;
		XMLElement& devices = HardwareConfig::instance().getChild("devices");
		HardwareConfig::loadHardware(devices, context, it->second);
	} else {
		throw FatalError("Extension \"" + extension + "\" not found!");
	}
	return true;
}

const string& CliExtension::optionHelp() const
{
	static string help("Insert the extension specified in argument");
	return help;
}


static int select(const struct dirent* d)
{
	// entry must be a directory and must contain the file "hardwareconfig.xml"
	string name = d->d_name;
	return FileOperations::isDirectory(name) && 
	       FileOperations::isRegularFile(name + "/hardwareconfig.xml");
}

void CliExtension::createExtensions(const string& basepath)
{
	char buf[PATH_MAX];
	if (!getcwd(buf, PATH_MAX)) {
		return;
	}
	if (chdir(basepath.c_str())) {
		return;
	}

	ReadDir dir(".");
	while (dirent* d = dir.getEntry()) {
		if (select(d)) {
			string name(d->d_name);
			string path(basepath + name +
					       "/hardwareconfig.xml");
			if (extensions.find(name) == extensions.end()) {
				// not yet found in prev directory
				extensions[name] = path;
			}
		}
	}
	chdir(buf);
}

} // namespace openmsx
