// Altirra portable native socket address tests

#include <cstring>

#include <at/attest/portabletest.h>
#include <vd2/system/VDString.h>
#include <at/atnetwork/socket.h>

#ifdef _WIN32
#include <at/atnetworksockets/socketutils_win32.h>
#else
#include <at/atnetworksockets/socketutils_posix.h>
#endif

bool ATTestNetNativeAddress(ATPortableTestContext& context) {
	{
		const ATSocketAddress address {};
		const ATSocketNativeAddress nativeAddress(address);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.GetSockAddr() == nullptr);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.GetSockAddrLen() == 0);
	}

	{
		const ATSocketAddress address = ATSocketAddress::CreateIPv4(0xC0000201, 6502);
		const ATSocketNativeAddress nativeAddress(address);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.GetSockAddr() != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.GetSockAddrLen() == sizeof(sockaddr_in));
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.mIPv4.sin_family == AF_INET);
		AT_PORTABLE_TEST_ASSERT(context, ntohs(nativeAddress.mIPv4.sin_port) == 6502);
#ifdef _WIN32
		AT_PORTABLE_TEST_ASSERT(context, ntohl(nativeAddress.mIPv4.sin_addr.S_un.S_addr) == 0xC0000201);
#else
		AT_PORTABLE_TEST_ASSERT(context, ntohl(nativeAddress.mIPv4.sin_addr.s_addr) == 0xC0000201);
#endif

		const ATSocketAddress roundTrip = ATSocketFromNativeAddress(nativeAddress.GetSockAddr());
		AT_PORTABLE_TEST_ASSERT(context, roundTrip.mType == ATSocketAddressType::IPv4);
		AT_PORTABLE_TEST_ASSERT(context, roundTrip.mPort == address.mPort);
		AT_PORTABLE_TEST_ASSERT(context, roundTrip.mIPv4Address == address.mIPv4Address);
	}

	{
		ATSocketAddress address = ATSocketAddress::CreateIPv6(65535);
		const uint8 bytes[16] {
			0x20, 0x01, 0x0D, 0xB8, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		};
		memcpy(address.mIPv6.mAddress, bytes, sizeof bytes);
		address.mIPv6.mScopeId = 7;

		const ATSocketNativeAddress nativeAddress(address);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.GetSockAddr() != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.GetSockAddrLen() == sizeof(sockaddr_in6));
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.mIPv6.sin6_family == AF_INET6);
		AT_PORTABLE_TEST_ASSERT(context, ntohs(nativeAddress.mIPv6.sin6_port) == 65535);
		AT_PORTABLE_TEST_ASSERT(context, nativeAddress.mIPv6.sin6_scope_id == 7);

		const ATSocketAddress roundTrip = ATSocketFromNativeAddress(nativeAddress.GetSockAddr());
		AT_PORTABLE_TEST_ASSERT(context, roundTrip.mType == ATSocketAddressType::IPv6);
		AT_PORTABLE_TEST_ASSERT(context, roundTrip.mPort == address.mPort);
		AT_PORTABLE_TEST_ASSERT(context, roundTrip.mIPv6.mScopeId == address.mIPv6.mScopeId);
		AT_PORTABLE_TEST_ASSERT(context,
			!memcmp(roundTrip.mIPv6.mAddress, address.mIPv6.mAddress, sizeof address.mIPv6.mAddress));
	}

	{
		sockaddr unsupported {};
		unsupported.sa_family = AF_UNSPEC;
		AT_PORTABLE_TEST_ASSERT(context,
			ATSocketFromNativeAddress(&unsupported).mType == ATSocketAddressType::None);
	}

	return true;
}
