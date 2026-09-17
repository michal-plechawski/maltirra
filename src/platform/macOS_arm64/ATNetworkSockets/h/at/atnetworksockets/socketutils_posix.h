// Altirra native socket address helpers for POSIX platforms

#ifndef f_ATNETWORKSOCKETS_SOCKETUTILS_POSIX_H
#define f_ATNETWORKSOCKETS_SOCKETUTILS_POSIX_H

#include <netinet/in.h>
#include <sys/socket.h>

struct ATSocketAddress;

struct ATSocketNativeAddress {
	ATSocketNativeAddress(const ATSocketAddress& addr);

	const sockaddr *GetSockAddr() const { return mpAddr; }
	int GetSockAddrLen() const { return mAddrLen; }

	const sockaddr *mpAddr = nullptr;
	int mAddrLen = 0;

	union {
		sockaddr_in mIPv4;
		sockaddr_in6 mIPv6;
	};
};

ATSocketAddress ATSocketFromNativeAddress(const sockaddr *saddr);

#endif
