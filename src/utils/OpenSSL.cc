#include "OpenSSL.hh"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <dlfcn.h>
#endif

namespace openmsx {
namespace OpenSSL {
namespace {

// ---------------------------------------------------------------------
//  Runtime library handle
// ---------------------------------------------------------------------

#ifdef _WIN32
using DllHandle = HMODULE;
static DllHandle openSslLib(const char* name)
{
	return LoadLibraryA(name);
}
static void* resolveSymbol(DllHandle lib, const char* name)
{
	return reinterpret_cast<void*>(GetProcAddress(lib, name));
}
static const char* libraryCandidates[] = {
	"libssl-4-x64.dll", "libssl-4.dll",
	"libssl-3-x64.dll", "libssl-3.dll",
	"libssl-1_1-x64.dll", "libssl-1_1.dll",
	"libssl.dll", "ssleay32.dll"
};
#else
using DllHandle = void*;
static DllHandle openSslLib(const char* name)
{
	return dlopen(name, RTLD_NOW | RTLD_LOCAL);
}
static void* resolveSymbol(DllHandle lib, const char* name)
{
	return dlsym(lib, name);
}
static const char* libraryCandidates[] = {
	// Homebrew (Apple Silicon / Intel) and system-wide installs
	"/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib",
	"/opt/homebrew/opt/openssl@1.1/lib/libssl.1.1.dylib",
	"/usr/local/opt/openssl@3/lib/libssl.3.dylib",
	"/usr/local/opt/openssl@1.1/lib/libssl.1.1.dylib",
	"/usr/local/lib/libssl.dylib",
	"/opt/homebrew/lib/libssl.dylib",
	// Generic names (ldconfig cache on Linux, default dyld paths on macOS)
	"libssl.so.3", "libssl.so.1.1", "libssl.so",
	"libssl.3.dylib", "libssl.dylib"
};
#endif

// ---------------------------------------------------------------------
//  Resolved OpenSSL entry points (all optional except the core set)
// ---------------------------------------------------------------------

struct OpenSSLApi {
	// init
	int (*init)(unsigned long long opts, const void* settings) = nullptr;
	int (*libraryInit)() = nullptr;
	// methods / contexts
	const void* (*tlsClientMethod)() = nullptr;
	const void* (*ssl23ClientMethod)() = nullptr;
	void* (*ctxNew)(const void* method) = nullptr;
	int (*ctxSetDefaultVerifyPaths)(void* ctx) = nullptr;
	int (*ctxLoadVerifyLocations)(void* ctx, const char* caFile,
	                              const char* caPath) = nullptr;
	// sessions
	void* (*newSession)(void* ctx) = nullptr;
	void (*freeSession)(void* ssl) = nullptr;
	int (*setFd)(void* ssl, int fd) = nullptr;
	void (*setVerify)(void* ssl, int mode, void* callback) = nullptr;
	int (*connect)(void* ssl) = nullptr;
	long (*ctrl)(void* ssl, int cmd, long larg, void* parg) = nullptr;
	void* (*get0Param)(const void* ssl) = nullptr;
	int (*verifyParamSet1Host)(void* param, const char* name,
	                           size_t namelen) = nullptr;
	// data
	int (*read)(void* ssl, void* buf, int num) = nullptr;
	int (*write)(void* ssl, const void* buf, int num) = nullptr;
	int (*pending)(const void* ssl) = nullptr;
	int (*shutdown)(void* ssl) = nullptr;
	// errors / verification
	int (*getError)(const void* ssl, int ret) = nullptr;
	long (*getVerifyResult)(const void* ssl) = nullptr;
	void* (*get1PeerCertificate)(const void* ssl) = nullptr;
	void (*freeX509)(void* x) = nullptr;
	unsigned long (*errGetError)() = nullptr;
	void (*errErrorStringN)(unsigned long e, char* buf, size_t len) = nullptr;
	const char* (*version)(int type) = nullptr;
};

OpenSSLApi api;
DllHandle libHandle = nullptr;
DllHandle cryptoHandle = nullptr;
bool loadAttempted = false;
bool loaded = false;

static constexpr unsigned long OPENSSL_INIT_LOAD_SSL_STRINGS = 0x00200000UL;
static constexpr unsigned long OPENSSL_INIT_LOAD_CRYPTO_STRINGS = 0x00000001UL;

static constexpr int SSL_VERIFY_NONE = 0x00;
static constexpr int SSL_VERIFY_PEER = 0x01;
static constexpr int SSL_ERROR_WANT_READ = 2;
static constexpr int SSL_ERROR_WANT_WRITE = 3;
static constexpr int SSL_ERROR_ZERO_RETURN = 6;
static constexpr int SSL_CTRL_SET_TLSEXT_HOSTNAME = 55;
static constexpr long TLSEXT_NAMETYPE_HOST_NAME = 0;
static constexpr int OPENSSL_VERSION = 0;

// X509 verify error codes (used to map to TCP-IP UNAPI close reasons)
static constexpr long X509_V_OK = 0;
static constexpr long X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT = 2;
static constexpr long X509_V_ERR_CERT_NOT_YET_VALID = 9;
static constexpr long X509_V_ERR_CERT_HAS_EXPIRED = 10;
static constexpr long X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT = 18;
static constexpr long X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN = 19;
static constexpr long X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY = 20;
static constexpr long X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE = 21;
static constexpr long X509_V_ERR_CERT_REVOKED = 23;
static constexpr long X509_V_ERR_INVALID_CA = 24;
static constexpr long X509_V_ERR_CERT_UNTRUSTED = 27;
static constexpr long X509_V_ERR_CERT_REJECTED = 28;
static constexpr long X509_V_ERR_HOSTNAME_MISMATCH = 62;

// ---------------------------------------------------------------------
//  Trust store setup
// ---------------------------------------------------------------------

#ifdef _WIN32
// Exports the Windows system root certificates as a PEM file and loads it
// into the given context, so that the device trusts the same CAs as the
// host operating system.
static bool loadWindowsTrustStore(void* ctx)
{
	// The Windows system store provides the full root CA list
	HCERTSTORE hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0,
	                                  CERT_SYSTEM_STORE_CURRENT_USER,
	                                  "ROOT");
	if (!hStore) return false;

	char tmpPath[MAX_PATH];
	char tmpFile[MAX_PATH];
	if (!GetTempPathA(MAX_PATH, tmpPath)) {
		CertCloseStore(hStore, 0);
		return false;
	}
	// GetTempFileNameA creates a unique empty file in the temp directory
	if (!GetTempFileNameA(tmpPath, "omsx", 0, tmpFile)) {
		CertCloseStore(hStore, 0);
		return false;
	}

	bool ok = false;
	FILE* f = fopen(tmpFile, "wb");
	if (f) {
		PCCERT_CONTEXT pCert = nullptr;
		while ((pCert = CertEnumCertificatesInStore(hStore, pCert)) != nullptr) {
			DWORD len = 0;
			if (!CryptBinaryToStringA(pCert->pbCertEncoded,
			                          pCert->cbCertEncoded,
			                          CRYPT_STRING_BASE64HEADER,
			                          nullptr, &len) || len == 0) {
				continue;
			}
			std::vector<char> buf(len);
			if (CryptBinaryToStringA(pCert->pbCertEncoded,
			                         pCert->cbCertEncoded,
			                         CRYPT_STRING_BASE64HEADER,
			                         buf.data(), &len)) {
				fwrite(buf.data(), 1, len, f);
			}
		}
		fclose(f);
		ok = api.ctxLoadVerifyLocations && api.ctxLoadVerifyLocations(ctx, tmpFile, nullptr) == 1;
	}
	CertCloseStore(hStore, 0);
	DeleteFileA(tmpFile);
	return ok;
}
#endif

static void setupTrustStore(void* ctx)
{
#ifdef _WIN32
	// The Windows system store provides the full root CA list
	loadWindowsTrustStore(ctx);
#else
	// macOS keeps the root certificates at a well-known location that is
	// not part of OpenSSL's default search path (libressl uses /etc/ssl
	// with a symlink, but Homebrew builds do not)
	if (api.ctxLoadVerifyLocations) {
		api.ctxLoadVerifyLocations(ctx, "/etc/ssl/cert.pem", nullptr);
	}
#endif
	if (api.ctxSetDefaultVerifyPaths) {
		api.ctxSetDefaultVerifyPaths(ctx);
	}
}

// Shared client context, created once on first use
void* getSharedContext()
{
	static std::once_flag flag;
	static void* ctx = nullptr;
	std::call_once(flag, [] {
		const void* method = api.tlsClientMethod ? api.tlsClientMethod()
		                     : api.ssl23ClientMethod();
		if (!method || !api.ctxNew) return;
		ctx = api.ctxNew(method);
		if (ctx) {
			setupTrustStore(ctx);
		}
	});
	return ctx;
}

// ---------------------------------------------------------------------
//  Symbol resolution
// ---------------------------------------------------------------------

template<typename T>
static T resolve(DllHandle lib, const char* name)
{
	return reinterpret_cast<T>(resolveSymbol(lib, name));
}

// Tries the SSL library first, then the crypto library: the X509/ERR
// entry points live in libcrypto (GetProcAddress does not search DLL
// dependencies, so on Windows the crypto library must be loaded too)
template<typename T>
static T resolveAny(const char* name)
{
	if (auto p = resolveSymbol(libHandle, name)) {
		return reinterpret_cast<T>(p);
	}
	if (cryptoHandle) {
		if (auto p = resolveSymbol(cryptoHandle, name)) {
			return reinterpret_cast<T>(p);
		}
	}
	return nullptr;
}

// Name of the libcrypto DLL that belongs to a libssl DLL
static std::string cryptoNameFor(const char* sslName)
{
	std::string n = sslName;
	auto pos = n.find("libssl");
	if (pos != std::string::npos) {
		n.replace(pos, 6, "libcrypto");
	} else if (n == "ssleay32.dll") {
		n = "libeay32.dll";
	}
	return n;
}

static bool resolveSymbols()
{
	api.init = resolve<decltype(api.init)>(libHandle, "OPENSSL_init_ssl");
	api.libraryInit = resolve<decltype(api.libraryInit)>(libHandle, "SSL_library_init");
	api.tlsClientMethod = resolve<decltype(api.tlsClientMethod)>(libHandle, "TLS_client_method");
	api.ssl23ClientMethod = resolve<decltype(api.ssl23ClientMethod)>(libHandle, "SSLv23_client_method");
	api.ctxNew = resolve<decltype(api.ctxNew)>(libHandle, "SSL_CTX_new");
	api.ctxSetDefaultVerifyPaths = resolve<decltype(api.ctxSetDefaultVerifyPaths)>(libHandle, "SSL_CTX_set_default_verify_paths");
	api.ctxLoadVerifyLocations = resolve<decltype(api.ctxLoadVerifyLocations)>(libHandle, "SSL_CTX_load_verify_locations");
	api.newSession = resolve<decltype(api.newSession)>(libHandle, "SSL_new");
	api.freeSession = resolve<decltype(api.freeSession)>(libHandle, "SSL_free");
	api.setFd = resolve<decltype(api.setFd)>(libHandle, "SSL_set_fd");
	api.setVerify = resolve<decltype(api.setVerify)>(libHandle, "SSL_set_verify");
	api.connect = resolve<decltype(api.connect)>(libHandle, "SSL_connect");
	api.ctrl = resolve<decltype(api.ctrl)>(libHandle, "SSL_ctrl");
	api.get0Param = resolve<decltype(api.get0Param)>(libHandle, "SSL_get0_param");
	api.verifyParamSet1Host = resolveAny<decltype(api.verifyParamSet1Host)>("X509_VERIFY_PARAM_set1_host");
	api.read = resolve<decltype(api.read)>(libHandle, "SSL_read");
	api.write = resolve<decltype(api.write)>(libHandle, "SSL_write");
	api.pending = resolve<decltype(api.pending)>(libHandle, "SSL_pending");
	api.shutdown = resolve<decltype(api.shutdown)>(libHandle, "SSL_shutdown");
	api.getError = resolve<decltype(api.getError)>(libHandle, "SSL_get_error");
	api.getVerifyResult = resolve<decltype(api.getVerifyResult)>(libHandle, "SSL_get_verify_result");
	api.get1PeerCertificate = resolve<decltype(api.get1PeerCertificate)>(libHandle, "SSL_get1_peer_certificate");
	api.freeX509 = resolveAny<decltype(api.freeX509)>("X509_free");
	api.errGetError = resolveAny<decltype(api.errGetError)>("ERR_get_error");
	api.errErrorStringN = resolveAny<decltype(api.errErrorStringN)>("ERR_error_string_n");
	api.version = resolveAny<decltype(api.version)>("OPENSSL_version");
	if (!api.version) {
		// LibreSSL exports the version function with this exact name
		api.version = resolveAny<decltype(api.version)>("OpenSSL_version");
	}
	if (!api.version) {
		api.version = resolveAny<decltype(api.version)>("SSLeay_version");
	}

	// Core set: without these there is nothing to work with
	if (!api.init && !api.libraryInit) return false;
	if (!api.tlsClientMethod && !api.ssl23ClientMethod) return false;
	if (!api.ctxNew || !api.newSession || !api.freeSession ||
	    !api.setFd || !api.setVerify || !api.connect || !api.read ||
	    !api.write || !api.shutdown || !api.getError) {
		return false;
	}

	// Initialize the library (idempotent in OpenSSL >= 1.1)
	if (api.init) {
		if (api.init(OPENSSL_INIT_LOAD_SSL_STRINGS |
		             OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr) != 1) {
			return false;
		}
	} else if (api.libraryInit() != 1) {
		return false;
	}
	return true;
}

static bool openLibrary()
{
	for (const char* name : libraryCandidates) {
		if (auto h = openSslLib(name)) {
			libHandle = h;
			// Load the matching libcrypto as well: the X509/ERR entry
			// points are exported by libcrypto, and GetProcAddress does
			// not search DLL dependencies (best effort; the TLS session
			// functions themselves all live in libssl).
			if (auto ch = openSslLib(cryptoNameFor(name).c_str())) {
				cryptoHandle = ch;
			}
			return true;
		}
	}
	return false;
}

} // namespace

// ---------------------------------------------------------------------
//  OpenSSL public interface
// ---------------------------------------------------------------------

LibHandle* load()
{
	static LibHandle singleton; // only valid (and non-null) after a successful load
	if (loadAttempted) return loaded ? &singleton : nullptr;
	loadAttempted = true;
	if (!openLibrary()) return nullptr;
	loaded = resolveSymbols();
	return loaded ? &singleton : nullptr;
}

SessionHandle::SessionHandle(void* ssl_)
	: ssl(ssl_)
{
}

SessionHandle::SessionHandle(SessionHandle&& other) noexcept
	: ssl(std::exchange(other.ssl, nullptr))
{
}

SessionHandle& SessionHandle::operator=(SessionHandle&& other) noexcept
{
	if (this != &other) {
		release();
		ssl = std::exchange(other.ssl, nullptr);
	}
	return *this;
}

SessionHandle::~SessionHandle()
{
	release();
}

void SessionHandle::release() noexcept
{
	if (!ssl || !api.shutdown || !api.freeSession) return;
	// Best-effort shutdown: the peer may already be gone
	api.shutdown(ssl);
	api.freeSession(ssl);
	ssl = nullptr;
}

int SessionHandle::handshake() const
{
	int r = api.connect(ssl);
	if (r == 1) return 1;
	int e = api.getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ) return 0;
	if (e == SSL_ERROR_WANT_WRITE) return 2;
	return -1;
}

int SessionHandle::verifyResult() const
{
	if (!api.getVerifyResult) return 0;
	long vr = api.getVerifyResult(ssl);
	if (vr == X509_V_OK) return 0;

	// The server did not provide a certificate at all
	if (api.get1PeerCertificate && api.freeX509) {
		void* peer = api.get1PeerCertificate(ssl);
		if (peer) {
			api.freeX509(peer);
		} else {
			return 9; // TLS: server did not provide a certificate
		}
	}

	// Map the X509 verify error to a TCP-IP UNAPI close reason (spec 4.5.4)
	switch (vr) {
	case X509_V_ERR_HOSTNAME_MISMATCH:
		return 11; // host name didn't match
	case X509_V_ERR_CERT_HAS_EXPIRED:
	case X509_V_ERR_CERT_NOT_YET_VALID:
		return 12; // expired
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		return 13; // self-signed
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
	case X509_V_ERR_CERT_UNTRUSTED:
	case X509_V_ERR_CERT_REJECTED:
		return 14; // untrusted root
	case X509_V_ERR_CERT_REVOKED:
		return 15; // revoked
	case X509_V_ERR_INVALID_CA:
		return 16; // invalid certificate authority
	default:
		return 10; // invalid server certificate
	}
}

IoResult SessionHandle::read(std::span<char> buf) const
{
	int r = api.read(ssl, buf.data(), static_cast<int>(buf.size()));
	if (r > 0) return IoResult(r);
	int e = api.getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
		return std::unexpected(IoError::WouldBlock);
	}
	if (e == SSL_ERROR_ZERO_RETURN) {
		return std::unexpected(IoError::Closed); // clean TLS close
	}
	return std::unexpected(IoError::Failed);
}

IoResult SessionHandle::write(std::span<const char> buf) const
{
	int r = api.write(ssl, buf.data(), static_cast<int>(buf.size()));
	if (r > 0) return IoResult(r);
	int e = api.getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
		return std::unexpected(IoError::WouldBlock);
	}
	if (e == SSL_ERROR_ZERO_RETURN) {
		return std::unexpected(IoError::Closed); // peer sent close_notify
	}
	return std::unexpected(IoError::Failed);
}

int SessionHandle::pending() const
{
	return api.pending ? api.pending(ssl) : 0;
}

std::optional<SessionHandle> LibHandle::createClientSession(
	bool verify, zstring_view hostname, int fd) const
{
	if (!loaded) return std::nullopt;
	void* ctx = getSharedContext();
	if (!ctx) return std::nullopt;

	void* ssl = api.newSession(ctx);
	if (!ssl) return std::nullopt;
	if (api.setFd(ssl, fd) != 1) {
		api.freeSession(ssl);
		return std::nullopt;
	}
	api.setVerify(ssl, verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);

	// Server name: sent as SNI and, when verifying, used to check the
	// host name in the server certificate
	if (!hostname.empty()) {
		if (api.ctrl) {
			api.ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME,
			         TLSEXT_NAMETYPE_HOST_NAME,
			         const_cast<char*>(hostname.data()));
		}
		if (verify && api.get0Param && api.verifyParamSet1Host) {
			api.verifyParamSet1Host(api.get0Param(ssl), hostname.data(),
			                        hostname.size());
		}
	}
	return SessionHandle(ssl);
}

zstring_view LibHandle::version() const
{
	return api.version ? api.version(OPENSSL_VERSION) : "";
}

zstring_view LibHandle::last_error() const
{
	if (api.errGetError && api.errErrorStringN) {
		thread_local char buf[256];
		api.errErrorStringN(api.errGetError(), buf, sizeof(buf));
		return buf;
	}
	return "OpenSSL error";
}

} // namespace OpenSSL
} // namespace openmsx
