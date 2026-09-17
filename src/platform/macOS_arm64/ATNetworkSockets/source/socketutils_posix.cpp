// Altirra native socket address helpers for POSIX platforms

#include <cstring>

#include <arpa/inet.h>

#include <vd2/system/VDString.h>
#include <at/atnetwork/socket.h>
#include <at/atnetworksockets/socketutils_posix.h>

ATSocketNativeAddress::ATSocketNativeAddress(const ATSocketAddress& addr) {
	if (addr.mType == ATSocketAddressType::IPv4) {
		memset(&mIPv4, 0, sizeof mIPv4);

		mIPv4.sin_family = AF_INET;
		mIPv4.sin_port = htons(addr.mPort);
		mIPv4.sin_addr.s_addr = htonl(addr.mIPv4Address);

		mpAddr = reinterpret_cast<const sockaddr *>(&mIPv4);
		mAddrLen = sizeof mIPv4;
	} else if (addr.mType == ATSocketAddressType::IPv6) {
		memset(&mIPv6, 0, sizeof mIPv6);

		mIPv6.sin6_family = AF_INET6;
		mIPv6.sin6_port = htons(addr.mPort);
		memcpy(mIPv6.sin6_addr.s6_addr, addr.mIPv6.mAddress, sizeof addr.mIPv6.mAddress);
		mIPv6.sin6_scope_id = addr.mIPv6.mScopeId;

		mpAddr = reinterpret_cast<const sockaddr *>(&mIPv6);
		mAddrLen = sizeof mIPv6;
	}
}

ATSocketAddress ATSocketFromNativeAddress(const sockaddr *saddr) {
	ATSocketAddress addr;

	if (saddr) {
		if (saddr->sa_family == AF_INET) {
			sockaddr_in saddr4 {};
			memcpy(&saddr4, saddr, sizeof saddr4);

			addr.mType = ATSocketAddressType::IPv4;
			addr.mIPv4Address = ntohl(saddr4.sin_addr.s_addr);
			addr.mPort = ntohs(saddr4.sin_port);
		} else if (saddr->sa_family == AF_INET6) {
			sockaddr_in6 saddr6 {};
			memcpy(&saddr6, saddr, sizeof saddr6);

			addr.mType = ATSocketAddressType::IPv6;
			memcpy(addr.mIPv6.mAddress, saddr6.sin6_addr.s6_addr, sizeof addr.mIPv6.mAddress);
			addr.mIPv6.mScopeId = saddr6.sin6_scope_id;
			addr.mPort = ntohs(saddr6.sin6_port);
		}
	}

	return addr;
}
