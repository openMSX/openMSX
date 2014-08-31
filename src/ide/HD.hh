#ifndef HD_HH
#define HD_HH

#include "Filename.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "TigerTree.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class HDCommand;
class File;
class DeviceConfig;

class HD : public SectorAccessibleDisk, public DiskContainer
         , public TTData
{
public:
	explicit HD(const DeviceConfig& config);
	virtual ~HD();

	const std::string& getName() const { return name; }
	const Filename& getImageName() const { return filename; }
	void switchImage(const Filename& filename);

	std::string getTigerTreeHash();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	// SectorAccessibleDisk:
	virtual void readSectorImpl (size_t sector,       SectorBuffer& buf);
	virtual void writeSectorImpl(size_t sector, const SectorBuffer& buf);
	virtual size_t getNbSectorsImpl() const;
	virtual bool isWriteProtectedImpl() const;
	virtual Sha1Sum getSha1Sum();

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;
	virtual bool diskChanged();
	virtual int insertDisk(string_ref filename);

	// TTData
	virtual uint8_t* getData(size_t offset, size_t size);
	virtual bool isCacheStillValid(time_t& time);

	void openImage();

	MSXMotherBoard& motherBoard;
	std::string name;
	std::unique_ptr<HDCommand> hdCommand;
	std::unique_ptr<TigerTree> tigerTree;

	std::unique_ptr<File> file;
	Filename filename;
	size_t filesize;
	bool alreadyTried;
};

REGISTER_BASE_CLASS(HD, "HD");
SERIALIZE_CLASS_VERSION(HD, 2);

} // namespace openmsx

#endif
