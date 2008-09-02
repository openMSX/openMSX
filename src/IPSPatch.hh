// $Id$

#ifndef IPSPATCH_HH
#define IPSPATCH_HH

#include "PatchInterface.hh"
#include "noncopyable.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class Filename;

class IPSPatch : public PatchInterface, private noncopyable
{
public:
	typedef std::map<unsigned, std::vector<byte> > PatchMap;

	IPSPatch(const Filename& filename,
	         std::auto_ptr<const PatchInterface> parent);

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const;
	virtual unsigned getSize() const;
	virtual void getFilenames(std::vector<Filename>& result) const;

private:
	const Filename filename;
	const std::auto_ptr<const PatchInterface> parent;
	PatchMap patchMap;
	unsigned size;
};

} // namespace openmsx

#endif
