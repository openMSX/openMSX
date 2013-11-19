#ifndef IPSPATCH_HH
#define IPSPATCH_HH

#include "PatchInterface.hh"
#include "Filename.hh"
#include "noncopyable.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class IPSPatch : public PatchInterface, private noncopyable
{
public:
	typedef std::map<size_t, std::vector<byte>> PatchMap;

	IPSPatch(const Filename& filename,
	         std::unique_ptr<const PatchInterface> parent);

	virtual void copyBlock(size_t src, byte* dst, size_t num) const;
	virtual size_t getSize() const;
	virtual std::vector<Filename> getFilenames() const;

private:
	const Filename filename;
	const std::unique_ptr<const PatchInterface> parent;
	PatchMap patchMap;
	size_t size;
};

} // namespace openmsx

#endif
