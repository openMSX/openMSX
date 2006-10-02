// $Id$

#ifndef IPSPATCH_HH
#define IPSPATCH_HH

#include "PatchInterface.hh"
#include "noncopyable.hh"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class IPSPatch : public PatchInterface, private noncopyable
{
public:
	typedef std::map<unsigned, std::vector<byte> > PatchMap;

	IPSPatch(const std::string& filename,
	         std::auto_ptr<const PatchInterface> parent);

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const;
	virtual unsigned getSize() const;

private:
	const std::auto_ptr<const PatchInterface> parent;
	PatchMap patchMap;
	unsigned size;
};

} // namespace openmsx

#endif
