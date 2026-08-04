#ifndef OPENSSL_HH
#define OPENSSL_HH

#include <cstddef>
#include <cstdint>

namespace openmsx {

// Minimal TLS wrapper around the OpenSSL runtime library installed on the
// host, loaded dynamically at runtime (LoadLibrary / dlopen). Building
// openMSX itself does not require any OpenSSL development package: all
// OpenSSL types are opaque and every function is reached through resolved
// entry points.
//
// When OpenSSL is not installed, load() returns false and the GenericUNAPI
// device degrades gracefully: the TLS capability bits are not advertised
// and TCP_OPEN with the TLS flag returns ERR_NOT_IMP.
//
// Supported: OpenSSL 1.1.x and 3.x on Windows, Linux and macOS (LibreSSL
// is mostly API-compatible and will also be picked up as a fallback).
class OpenSSL
{
public:
	// Loads the runtime library and resolves the needed entry points.
	// Idempotent; returns false when OpenSSL is not available.
	static bool load();

	[[nodiscard]] static bool available();
	// Version string, e.g. "OpenSSL 3.1.4 24 Oct 2023" ("" if not loaded).
	[[nodiscard]] static const char* version();

	// Creates a TLS client session on the given connected (non-blocking)
	// socket.
	// - verify: validate the server certificate (the TCP-IP UNAPI spec
	//   "Verify the server certificate" flag). When false, no validation
	//   at all is performed.
	// - hostname: server name, sent as SNI and (when verify is set) used
	//   to check the certificate host name. May be null.
	// Returns an opaque session handle, or nullptr on failure.
	static void* createClientSession(bool verify, const char* hostname, int fd);

	// Drives the TLS handshake. Returns 1 when completed, 0 when it needs
	// read readiness, 2 when it needs write readiness, -1 on failure.
	static int handshake(void* ssl);

	// Certificate validation result after a completed handshake (only
	// meaningful when created with verify=true). Returns 0 when the
	// certificate is valid, otherwise a TCP-IP UNAPI close reason code in
	// the range 9..19 (spec 4.5.4).
	static int verifyResult(void* ssl);

	// Decrypts inbound data. Returns the number of bytes read, 0 on a
	// clean TLS close (close_notify) and -1 on error. On -1, 'want' is
	// set to 1 (needs read) or 2 (needs write) when the call would block
	// on the non-blocking socket, 0 for a real error.
	static ptrdiff_t read(void* ssl, char* buf, size_t len, int& want);

	// Encrypts and sends data. Returns the number of bytes written, or
	// -1 with 'want' set as described above.
	static ptrdiff_t write(void* ssl, const char* buf, size_t len, int& want);

	// Plaintext bytes already decrypted and buffered by the SSL layer,
	// readable without waiting for socket readiness.
	static int pending(void* ssl);

	// Best-effort TLS shutdown and session teardown.
	static void close(void* ssl);

	// Description of the last error (for console diagnostics).
	static const char* lastError();

private:
	static bool openLibrary();
};

} // namespace openmsx

#endif
