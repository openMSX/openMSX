// $Id$

#ifndef DISKCHANGECOMMAND_HH
#define DISKCHANGECOMMAND_HH

#include "Command.hh"

namespace openmsx {

class DiskChangeCommand : public SimpleCommand
{
public:
	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

protected:
	virtual void insertDisk(const std::string& disk,
	                        const std::vector<std::string>& patches) = 0;
	virtual void ejectDisk() = 0;
	virtual const std::string& getDriveName() const = 0;
	virtual const std::string& getCurrentDiskName() const = 0;
};

} // namespace openmsx

#endif
