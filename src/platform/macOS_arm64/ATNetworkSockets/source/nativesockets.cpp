// Altirra native socket entry points for POSIX platforms

#include <cstring>

#include <netinet/in.h>

#include <vd2/system/VDString.h>
#include <at/atnetworksockets/nativesockets.h>
#include <at/atnetworksockets/internal/lookupworker.h>
#include <at/atnetworksockets/internal/socketworker.h>

namespace {
	ATNetLookupWorker *g_pATNetLookupWorker = nullptr;
	ATNetSocketWorker *g_pATNetSocketWorker = nullptr;
}

bool ATSocketInit() {
	if (!g_pATNetLookupWorker) {
		g_pATNetLookupWorker = new ATNetLookupWorker();
		if (!g_pATNetLookupWorker->Init()) {
			delete g_pATNetLookupWorker;
			g_pATNetLookupWorker = nullptr;
		}
	}

	if (!g_pATNetSocketWorker) {
		g_pATNetSocketWorker = new ATNetSocketWorker();
		if (!g_pATNetSocketWorker->Init()) {
			delete g_pATNetSocketWorker;
			g_pATNetSocketWorker = nullptr;
		}
	}

	if (g_pATNetLookupWorker && g_pATNetSocketWorker)
		return true;

	ATSocketPreShutdown();
	return false;
}

void ATSocketPreShutdown() {
	if (g_pATNetSocketWorker) {
		delete g_pATNetSocketWorker;
		g_pATNetSocketWorker = nullptr;
	}

	if (g_pATNetLookupWorker) {
		delete g_pATNetLookupWorker;
		g_pATNetLookupWorker = nullptr;
	}
}

void ATSocketShutdown() {
	ATSocketPreShutdown();
}

vdrefptr<IATNetLookupResult> ATNetLookup(const wchar_t *hostname, const wchar_t *service) {
	if (!g_pATNetLookupWorker)
		return nullptr;

	return g_pATNetLookupWorker->Lookup(hostname, service);
}

vdrefptr<IATStreamSocket> ATNetConnect(const wchar_t *hostname, const wchar_t *service, bool dualStack) {
	if (!g_pATNetLookupWorker || !g_pATNetSocketWorker)
		return nullptr;

	auto socket = g_pATNetSocketWorker->CreateStreamSocket();
	if (!socket)
		return nullptr;

	auto lookup = g_pATNetLookupWorker->Lookup(hostname, service);
	if (!lookup)
		return nullptr;

	lookup->SetOnCompleted(nullptr,
		[lookup, socket, dualStack](const ATSocketAddress& address) {
			socket->Connect(address, dualStack);
		},
		true);

	return socket;
}

vdrefptr<IATStreamSocket> ATNetConnect(const ATSocketAddress& address, bool dualStack) {
	if (!g_pATNetSocketWorker)
		return nullptr;

	vdrefptr<ATNetStreamSocket> socket = g_pATNetSocketWorker->CreateStreamSocket();
	if (!socket)
		return nullptr;

	socket->Connect(address, dualStack);
	return socket;
}

vdrefptr<IATListenSocket> ATNetListen(const ATSocketAddress& address, bool dualStack) {
	if (!g_pATNetSocketWorker)
		return nullptr;

	return g_pATNetSocketWorker->CreateListenSocket(address, dualStack);
}

vdrefptr<IATListenSocket> ATNetListen(ATSocketAddressType addressType, uint16 port, bool dualStack) {
	ATSocketAddress address;
	address.mType = addressType;
	address.mPort = port;

	if (addressType == ATSocketAddressType::IPv4) {
		address.mIPv4Address = ntohl(INADDR_ANY);
	} else if (addressType == ATSocketAddressType::IPv6) {
		memset(address.mIPv6.mAddress, 0, sizeof address.mIPv6.mAddress);
		address.mIPv6.mScopeId = 0;
	}

	return ATNetListen(address, dualStack);
}

vdrefptr<IATDatagramSocket> ATNetBind(const ATSocketAddress& address, bool dualStack) {
	if (!g_pATNetSocketWorker)
		return nullptr;

	return g_pATNetSocketWorker->CreateDatagramSocket(address, dualStack);
}
