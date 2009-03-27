// $Id$

#ifdef _WIN32

#include "SspiUtils.hh"
#include "openmsx.hh"
#include "MSXException.hh"
#include <sddl.h>
#include <cassert>

//
// NOTE: This file MUST be kept in sync between the openmsx and openmsx-debugger projects
//

namespace openmsx {

SspiPackageBase::SspiPackageBase(StreamWrapper& userStream) 
	: stream(userStream)
{
	memset(&hCreds, 0, sizeof(hCreds));
	memset(&hContext, 0, sizeof(hContext));

	cbMaxTokenSize = GetPackageMaxTokenSize(NEGOSSP_NAME_W);
	if (!cbMaxTokenSize) {
		throw MSXException("GetPackageMaxTokenSize failed");
	}
}

SspiPackageBase::~SspiPackageBase()
{
	DeleteSecurityContext(&hContext);
	FreeCredentialsHandle(&hCreds);
}

void InitTokenContextBuffer(PSecBufferDesc pSecBufferDesc, PSecBuffer pSecBuffer)
{
	pSecBuffer->BufferType = SECBUFFER_TOKEN;
	pSecBuffer->cbBuffer = 0;
	pSecBuffer->pvBuffer = NULL;

	pSecBufferDesc->ulVersion = SECBUFFER_VERSION;
	pSecBufferDesc->cBuffers = 1;
	pSecBufferDesc->pBuffers = pSecBuffer;
}

void ClearContextBuffers(PSecBufferDesc pSecBufferDesc)
{
	for (ULONG i = 0; i < pSecBufferDesc->cBuffers; i ++)
	{
		FreeContextBuffer(pSecBufferDesc->pBuffers[i].pvBuffer);
		pSecBufferDesc->pBuffers[i].cbBuffer = 0;
		pSecBufferDesc->pBuffers[i].pvBuffer = NULL;
	}
}

void PrintSecurityStatus(const char* context, SECURITY_STATUS ss)
{
	(void)&context;
	(void)&ss;
#ifdef DEBUG
	switch (ss)
	{
	case SEC_E_OK:
		PRT_DEBUG(context << ": SEC_E_OK");
		break;
	case SEC_I_CONTINUE_NEEDED:
		PRT_DEBUG(context << ": SEC_I_CONTINUE_NEEDED");
		break;
	case SEC_E_INVALID_TOKEN:
		PRT_DEBUG(context << ": SEC_E_INVALID_TOKEN");
		break;
	case SEC_E_BUFFER_TOO_SMALL:
		PRT_DEBUG(context << ": SEC_E_BUFFER_TOO_SMALL");
		break;
	case SEC_E_INVALID_HANDLE:
		PRT_DEBUG(context << ": SEC_E_INVALID_HANDLE");
		break;
	case SEC_E_WRONG_PRINCIPAL:
		PRT_DEBUG(context << ": SEC_E_WRONG_PRINCIPAL");
		break;
	default:
		PRT_DEBUG(context << ": " << ss);
		break;
	}
#endif
}

void PrintSecurityBool(const char* context, BOOL ret)
{
	(void)&context;
	(void)&ret;
#ifdef DEBUG
	if (ret) {
		PRT_DEBUG(context << ": true");
	} else {
		PRT_DEBUG(context << ": false - " << GetLastError());
	}
#endif
}

void PrintSecurityPackageName(PCtxtHandle phContext)
{
	(void)&phContext;
#ifdef DEBUG
	SecPkgContext_PackageInfoA package;
	SECURITY_STATUS ss = QueryContextAttributesA(phContext, SECPKG_ATTR_PACKAGE_INFO, &package);
	if (ss == SEC_E_OK) {
		PRT_DEBUG("Using " << package.PackageInfo->Name << " package");
	}
#endif
}

void PrintSecurityPrincipalName(PCtxtHandle phContext)
{
	(void)&phContext;
#ifdef DEBUG
	SecPkgContext_NamesA name;
	SECURITY_STATUS ss = QueryContextAttributesA(phContext, SECPKG_ATTR_NAMES, &name);
	if (ss == SEC_E_OK) {
		PRT_DEBUG("Client principal " << name.sUserName);
	}
#endif
}

void PrintSecurityDescriptor(PSECURITY_DESCRIPTOR psd)
{
	(void)&psd;
#ifdef DEBUG
	char* sddl;
	BOOL ret = ConvertSecurityDescriptorToStringSecurityDescriptorA(
		psd,
		SDDL_REVISION, 
		OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
		DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION, 
		&sddl, 
		NULL);
	if (ret) {
		PRT_DEBUG("SecurityDescriptor: " << sddl);
		LocalFree(sddl);
	}
#endif
}

// If successful, caller must free the results with LocalFree()
// If unsuccessful, returns null
PTOKEN_USER GetProcessToken()
{
	PTOKEN_USER pToken = NULL;

	HANDLE hProcessToken;
	BOOL ret = OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hProcessToken);
	PrintSecurityBool("OpenProcessToken", ret);
	if (ret) {

		DWORD cbToken;
		ret = GetTokenInformation(hProcessToken, TokenUser, NULL, 0, &cbToken);
		assert(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && cbToken);

		pToken = (TOKEN_USER*)LocalAlloc(LMEM_ZEROINIT, cbToken);
		if (pToken) {
			ret = GetTokenInformation(hProcessToken, TokenUser, pToken, cbToken, &cbToken);
			PrintSecurityBool("GetTokenInformation", ret);
			if (!ret) {
				LocalFree(pToken);
				pToken = NULL;
			}
		}

		CloseHandle(hProcessToken);
	}

	return pToken;
}

// If successful, caller must free the results with LocalFree()
// If unsuccessful, returns null
PSECURITY_DESCRIPTOR CreateCurrentUserSecurityDescriptor()
{
	PSECURITY_DESCRIPTOR psd = NULL;
	PTOKEN_USER pToken = GetProcessToken();
	if (pToken) {
		PSID pUserSid = pToken->User.Sid;
		const DWORD cbEachAce = sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD);
		const DWORD cbACL = sizeof(ACL) + cbEachAce + GetLengthSid(pUserSid);

		// Allocate the SD and the ACL in one allocation, so we only have one buffer to manage
		// The SD structure ends with a pointer, so the start of the ACL will be well aligned
		BYTE* buffer = (BYTE*)LocalAlloc(LMEM_ZEROINIT, SECURITY_DESCRIPTOR_MIN_LENGTH + cbACL);
		if (buffer) {
			psd = (PSECURITY_DESCRIPTOR)buffer;
			PACL pacl = (PACL)(buffer + SECURITY_DESCRIPTOR_MIN_LENGTH);
			PACCESS_ALLOWED_ACE pUserAce;
			if (InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION) &&
				InitializeAcl(pacl, cbACL, ACL_REVISION) &&
				AddAccessAllowedAce(pacl, ACL_REVISION, ACCESS_ALL, pUserSid) &&
				SetSecurityDescriptorDacl(psd, TRUE, pacl, FALSE) &&
				// Need to set the Group and Owner on the SD in order to use it with AccessCheck()
				GetAce(pacl, 0, (void**)&pUserAce) &&
				SetSecurityDescriptorGroup(psd, &pUserAce->SidStart, FALSE) &&
				SetSecurityDescriptorOwner(psd, &pUserAce->SidStart, FALSE))
			{
				buffer = NULL;
			} else {
				psd = NULL;
			}

			LocalFree(buffer);
		}

		LocalFree(pToken);
	}

	if (psd) {
		assert(IsValidSecurityDescriptor(psd));
		PrintSecurityDescriptor(psd);
	}

	return psd;
}

unsigned long GetPackageMaxTokenSize(wchar_t* package)
{
	PSecPkgInfoW pkgInfo;
	SECURITY_STATUS ss = QuerySecurityPackageInfoW(package, &pkgInfo);
	PrintSecurityStatus("QuerySecurityPackageInfoW", ss);
	if (ss != SEC_E_OK) {
		return 0;
	}

	unsigned long cbMaxToken = pkgInfo->cbMaxToken;
	FreeContextBuffer(pkgInfo);
	return cbMaxToken;
}

bool Send(StreamWrapper& stream, void* buffer, unsigned cb)
{
	unsigned sent = 0;
	while (sent < cb)
	{
		unsigned ret = stream.Write((char*)buffer + sent, cb - sent);
		if (ret == STREAM_ERROR) {
			return false;
		}
		sent += ret;
	}
	return true;
}

bool SendChunk(StreamWrapper& stream, void* buffer, unsigned cb)
{
	unsigned nl = htonl(cb);
	if (!Send(stream, &nl, sizeof(nl))) {
		return false;
	}
	return Send(stream, buffer, cb);
}

bool Recv(StreamWrapper& stream, void* buffer, unsigned cb)
{
	unsigned recvd = 0;
	while (recvd < cb) {
		unsigned ret = stream.Read((char*)buffer + recvd, cb - recvd);
		if (ret == STREAM_ERROR) {
			return false;
		}
		recvd += ret;
	}

	return true;
}

bool RecvChunkSize(StreamWrapper& stream, unsigned* pcb)
{
	unsigned cb;
	bool ret = Recv(stream, &cb, sizeof(cb));
	if (ret) {
		*pcb = ntohl(cb);
	}
	return ret;
}

unsigned RecvChunk(StreamWrapper& stream, std::vector<char>& buffer, unsigned cbMaxSize)
{
	unsigned cb;
	if (!RecvChunkSize(stream, &cb) || cb > cbMaxSize) {
		return 0;
	}
	if (cb > buffer.size()) {
		buffer.resize(cb);
	}
	if (!Recv(stream, &buffer[0], cb)) {
		return 0;
	}
	return cb;
}

} // namespace openmsx

#endif
