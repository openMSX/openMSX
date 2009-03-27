// $Id$

#ifndef SSPI_UTILS_HH
#define SSPI_UTILS_HH

#ifdef _WIN32

#include <winsock2.h>
#ifdef __GNUC__
// MinGW32's security support has issues
#include <subauth.h>
#define NEGOSSP_NAME_W  L"Negotiate"
#endif
#include <security.h>
#include <vector>

//
// NOTE: This file MUST be kept in sync between the openmsx and openmsx-debugger projects
//

namespace openmsx {

const unsigned STREAM_ERROR = 0xffffffff;

class StreamWrapper
{
public:
	virtual unsigned Read(void* buffer, unsigned cb) = 0;
	virtual unsigned Write(void* buffer, unsigned cb) = 0;
};

class SspiPackageBase
{
protected:
	CredHandle hCreds;
	CtxtHandle hContext;

	StreamWrapper& stream;
	unsigned cbMaxTokenSize;

public:
	SspiPackageBase(StreamWrapper& stream);
	~SspiPackageBase();
};

// Generic access control flags, used with AccessCheck
const DWORD ACCESS_READ = 0x1;
const DWORD ACCESS_WRITE = 0x2;
const DWORD ACCESS_EXECUTE = 0x4;
const DWORD ACCESS_ALL = ACCESS_READ | ACCESS_WRITE | ACCESS_EXECUTE;

const GENERIC_MAPPING mapping = { 
	ACCESS_READ, ACCESS_WRITE, ACCESS_EXECUTE, ACCESS_ALL };

void InitTokenContextBuffer(PSecBufferDesc pSecBufferDesc, PSecBuffer pSecBuffer);
void ClearContextBuffers(PSecBufferDesc pSecBufferDesc);
void PrintSecurityStatus(const char* context, SECURITY_STATUS ss);
void PrintSecurityBool(const char* context, BOOL ret);
void PrintSecurityPackageName(PCtxtHandle phContext);
void PrintSecurityPrincipalName(PCtxtHandle phContext);
void PrintSecurityDescriptor(PSECURITY_DESCRIPTOR psd);
PSECURITY_DESCRIPTOR CreateCurrentUserSecurityDescriptor();
unsigned long GetPackageMaxTokenSize(wchar_t* package);

bool SendChunk(StreamWrapper& stream, void* buffer, unsigned cb);
unsigned RecvChunk(StreamWrapper& stream, std::vector<char>& buffer, unsigned cbMaxSize);

} // namespace openmsx

#endif // _WIN32

#endif // SSPI_UTILS_HH
