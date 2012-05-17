// $Id$

#ifndef HD_HH
#define HD_HH

#include "Filename.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class HDCommand;
class File;
class DeviceConfig;

class HD : public SectorAccessibleDisk, public DiskContainer
{
public:
	HD(MSXMotherBoard& motherBoard, const DeviceConfig& config);
	virtual ~HD();

	const std::string& getName() const;
	const Filename& getImageName() const;
	void switchImage(const Filename& filename);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SectorAccessibleDisk:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual unsigned getNbSectorsImpl() const;
	virtual bool isWriteProtectedImpl() const;
	virtual std::string getSha1Sum();

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;
	virtual bool diskChanged();
	virtual int insertDisk(const std::string& filename);

	void openImage();

	MSXMotherBoard& motherBoard;
	std::string name;
	std::auto_ptr<HDCommand> hdCommand;

	std::auto_ptr<File> file;
	Filename filename;
	unsigned filesize;
	bool alreadyTried;
};

REGISTER_BASE_CLASS(HD, "HD");

} // namespace openmsx

#endif
