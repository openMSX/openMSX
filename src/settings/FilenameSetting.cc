// $Id$

#include "FilenameSetting.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CliCommOutput.hh"
#include "CommandController.hh"


namespace openmsx {

FilenameSetting::FilenameSetting(const string &name,
	const string &description,
	const string &initialValue)
	: StringSetting(name, description, initialValue)
{
}

void FilenameSetting::setValue(const string &newValue)
{
	string resolved;
	try {
		UserFileContext context;
		resolved = context.resolve(newValue);
	} catch (FileException &e) {
		// File not found.
		CliCommOutput::instance().printWarning(
			"couldn't find file: \"" + newValue + "\"");
		return;
	}
	if (checkFile(resolved)) {
		StringSetting::setValue(newValue);
	}
}

void FilenameSetting::tabCompletion(vector<string> &tokens) const
{
	CommandController::completeFileName(tokens);
}

bool FilenameSetting::checkFile(const string &filename)
{
	return true;
}

} // namespace openmsx
