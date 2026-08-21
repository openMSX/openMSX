#ifndef OPENSSL_HH
#define OPENSSL_HH

#include "zstring_view.hh"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>

namespace openmsx::OpenSSL {

// Minimal TLS wrapper around the OpenSSL runtime library installed on the
// host, loaded dynamically at runtime (LoadLibrary / dlopen). Building
// openMSX itself does not require any OpenSSL development package: all
// OpenSSL types are opaque and every function is reached through resolved
// entry points.
//
// When OpenSSL is not installed, load() returns nullptr and the GenericUNAPI
// device degrades gracefully: the TLS capability bits are not advertised
// and TCP_OPEN with the TLS flag returns ERR_NOT_IMP.
//
// Supported: OpenSSL 1.1.x and 3.x on Windows, Linux and macOS (LibreSSL
// is mostly API-compatible and will also be picked up as a fallback).

// Opaque handles for OpenSSL runtime types, kept forward-declared so that
// openMSX itself never needs the OpenSSL development headers.
struct SslCtx;
struct Ssl;
struct X509;
struct X509VerifyParam;
struct SslMethod;
struct X509StoreCtx;
struct SslInitSettings;

// Result of a (non-blocking) read() or write(): success carries the
// number of bytes transferred; on failure IoError says why.
enum class IoError {
	WouldBlock, // the call would block; retry later (either readiness)
	Closed,     // clean TLS close (peer sent close_notify)
	Failed      // real error
};
using IoResult = std::expected<size_t, IoError>;

// An active TLS session on a connected (non-blocking) socket. Move-only
// RAII: the destructor performs the best-effort shutdown and releases the
// session, so a SessionHandle can never outlive its session.
struct SessionHandle {
	SessionHandle(const SessionHandle&) = delete;
	SessionHandle& operator=(const SessionHandle&) = delete;
	SessionHandle(SessionHandle&& other) noexcept;
	SessionHandle& operator=(SessionHandle&& other) noexcept;
	~SessionHandle();

	// Drives the TLS handshake. Returns 1 when completed, 0 when it needs
	// read readiness, 2 when it needs write readiness, -1 on failure.
	[[nodiscard]] int handshake() const;

	// Certificate validation result after a completed handshake (only
	// meaningful when the session was created with verify=true). Returns
	// 0 when the certificate is valid, otherwise a TCP-IP UNAPI close
	// reason code in the range 9..19 (spec 4.5.4).
	[[nodiscard]] int verifyResult() const;

	// Decrypts inbound data. Success: number of bytes read. Failure:
	// WouldBlock, or Closed on a clean TLS close (close_notify).
	[[nodiscard]] IoResult read(std::span<char> buf) const;

	// Encrypts and sends data. Success: number of bytes written. Failure:
	// WouldBlock, or Closed when the peer sent close_notify.
	[[nodiscard]] IoResult write(std::span<const char> buf) const;

	// Plaintext bytes already decrypted and buffered by the SSL layer,
	// readable without waiting for socket readiness.
	[[nodiscard]] int pending() const;

private:
	friend struct LibHandle;
	explicit SessionHandle(Ssl* ssl_);
	void release() noexcept;
	Ssl* ssl;
};

// Handle to the loaded OpenSSL runtime. Obtained from load(); its members
// may only be called after load() succeeded.
struct LibHandle {
	// Version string, e.g. "OpenSSL 3.1.4 24 Oct 2023".
	[[nodiscard]] zstring_view version() const;

	// Description of the last OpenSSL error (for console diagnostics).
	[[nodiscard]] zstring_view last_error() const;

	// Creates a TLS client session on the given connected (non-blocking)
	// socket.
	// - verify: validate the server certificate (the TCP-IP UNAPI spec
	//   "Verify the server certificate" flag). When false, no validation
	//   at all is performed.
	// - hostname: server name, sent as SNI and (when verify is set) used
	//   to check the certificate host name. May be empty.
	// Returns a session handle, or nullopt on failure.
	[[nodiscard]] std::optional<SessionHandle> createClientSession(
		bool verify, zstring_view hostname, int fd) const;

private:
	friend LibHandle* load();
	LibHandle() = default;
};

// Loads the runtime library and resolves the needed entry points.
// Idempotent; returns nullptr when OpenSSL is not available (the returned
// pointer is the same on every successful call).
LibHandle* load();

} // namespace openmsx::OpenSSL

#endif
