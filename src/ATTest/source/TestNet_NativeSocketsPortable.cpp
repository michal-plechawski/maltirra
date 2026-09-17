// Altirra portable native socket loopback tests

#include <algorithm>
#include <cstring>

#include <vd2/system/VDString.h>
#include <at/atcore/asyncdispatcherimpl.h>
#include <at/atnetworksockets/nativesockets.h>
#include <at/attest/portabletest.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>

namespace {
	class ATSocketSystemScope {
	public:
		ATSocketSystemScope()
			: mbInitialized(ATSocketInit())
		{
		}

		~ATSocketSystemScope() {
			if (mbInitialized)
				ATSocketShutdown();
		}

		bool mbInitialized;
	};

	template<typename TPredicate>
	bool ATWaitForSocketEvent(ATAsyncDispatcher& dispatcher, VDSignal& signal, TPredicate&& predicate, uint32 timeoutMs = 3000) {
		const uint32 deadline = VDGetCurrentTick() + timeoutMs;

		for (;;) {
			dispatcher.RunCallbacks();
			if (predicate())
				return true;

			const uint32 now = VDGetCurrentTick();
			if ((sint32)(deadline - now) <= 0)
				return false;

			signal.tryWait(std::min<uint32>(deadline - now, 20));
		}
	}
}

bool ATTestNetNativeSockets(ATPortableTestContext& context) {
	VDSignal dispatchSignal;
	ATAsyncDispatcher dispatcher;
	dispatcher.SetWakeCallback([&] { dispatchSignal.signal(); });
	ATSocketSystemScope socketSystem;
	AT_PORTABLE_TEST_ASSERT(context, socketSystem.mbInitialized);

	vdrefptr<IATListenSocket> listener;
	vdrefptr<IATStreamSocket> client;
	ATSocketStatus listenerStatus {};
	ATSocketStatus clientStatus {};
	uint16 tcpPort = 0;

	for(uint16 candidate = 49371; candidate < 49403; ++candidate) {
		listener = ATNetListen(ATSocketAddress::CreateLocalhostIPv4(candidate));
		client = ATNetConnect(ATSocketAddress::CreateLocalhostIPv4(candidate));
		AT_PORTABLE_TEST_ASSERT(context, listener != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, client != nullptr);

		listener->SetOnEvent(&dispatcher, [&](const ATSocketStatus& status) { listenerStatus = status; }, true);
		client->SetOnEvent(&dispatcher, [&](const ATSocketStatus& status) { clientStatus = status; }, true);

		const bool finished = ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
			return (listenerStatus.mbCanAccept && !clientStatus.mbConnecting)
				|| listenerStatus.mError != ATSocketError::None
				|| clientStatus.mError != ATSocketError::None;
		});

		if (finished && listenerStatus.mbCanAccept && clientStatus.mError == ATSocketError::None) {
			tcpPort = candidate;
			break;
		}

		client->SetOnEvent(nullptr, nullptr, false);
		listener->SetOnEvent(nullptr, nullptr, false);
		client->CloseSocket(true);
		listener->CloseSocket(true);
		client = nullptr;
		listener = nullptr;
		listenerStatus = {};
		clientStatus = {};
	}

	AT_PORTABLE_TEST_ASSERT(context, tcpPort != 0);
	AT_PORTABLE_TEST_ASSERT(context, !clientStatus.mbConnecting);
	AT_PORTABLE_TEST_ASSERT(context, clientStatus.mError == ATSocketError::None);

	vdrefptr<IATStreamSocket> server = listener->Accept();
	AT_PORTABLE_TEST_ASSERT(context, server != nullptr);

	ATSocketStatus serverStatus {};
	server->SetOnEvent(&dispatcher, [&](const ATSocketStatus& status) { serverStatus = status; }, true);
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
		return !serverStatus.mbConnecting || serverStatus.mError != ATSocketError::None;
	}));
	AT_PORTABLE_TEST_ASSERT(context, serverStatus.mError == ATSocketError::None);

	static constexpr char kClientMessage[] = "Altirra TCP client";
	AT_PORTABLE_TEST_ASSERT(context, client->Send(kClientMessage, sizeof kClientMessage) == sizeof kClientMessage);
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
		return serverStatus.mbCanRead || serverStatus.mError != ATSocketError::None;
	}));

	char tcpBuffer[64] {};
	AT_PORTABLE_TEST_ASSERT(context, server->Recv(tcpBuffer, sizeof tcpBuffer) == sizeof kClientMessage);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(tcpBuffer, kClientMessage, sizeof kClientMessage));

	static constexpr char kServerMessage[] = "Altirra TCP server";
	AT_PORTABLE_TEST_ASSERT(context, server->Send(kServerMessage, sizeof kServerMessage) == sizeof kServerMessage);
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
		return clientStatus.mbCanRead || clientStatus.mError != ATSocketError::None;
	}));
	AT_PORTABLE_TEST_ASSERT(context, client->Recv(tcpBuffer, sizeof tcpBuffer) == sizeof kServerMessage);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(tcpBuffer, kServerMessage, sizeof kServerMessage));

	server->ShutdownSocket(true, false);
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
		return clientStatus.mbRemoteClosed || clientStatus.mError != ATSocketError::None;
	}));
	AT_PORTABLE_TEST_ASSERT(context, clientStatus.mbRemoteClosed);

	vdrefptr<IATDatagramSocket> receiver;
	vdrefptr<IATDatagramSocket> sender = ATNetBind(ATSocketAddress::CreateIPv4(), false);
	AT_PORTABLE_TEST_ASSERT(context, sender != nullptr);

	ATSocketStatus receiverStatus {};
	uint16 udpPort = 0;
	for(uint16 candidate = 49403; candidate < 49435; ++candidate) {
		receiver = ATNetBind(ATSocketAddress::CreateLocalhostIPv4(candidate), false);
		AT_PORTABLE_TEST_ASSERT(context, receiver != nullptr);
		receiver->SetOnEvent(&dispatcher, [&](const ATSocketStatus& status) { receiverStatus = status; }, true);
		const bool ready = ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
			return !receiverStatus.mbConnecting || receiverStatus.mError != ATSocketError::None;
		});

		static constexpr uint8 kProbe[] { 0x41, 0x54, 0x52, 0x41 };
		if (ready && receiverStatus.mError == ATSocketError::None)
			AT_PORTABLE_TEST_ASSERT(context, sender->SendTo(ATSocketAddress::CreateLocalhostIPv4(candidate), kProbe, sizeof kProbe));

		const bool finished = ready && ATWaitForSocketEvent(dispatcher, dispatchSignal, [&] {
			return receiverStatus.mbCanRead || receiverStatus.mError != ATSocketError::None;
		});
		if (finished && receiverStatus.mbCanRead) {
			udpPort = candidate;
			break;
		}

		receiver->SetOnEvent(nullptr, nullptr, false);
		receiver->CloseSocket(true);
		receiver = nullptr;
		receiverStatus = {};
	}

	AT_PORTABLE_TEST_ASSERT(context, udpPort != 0);
	ATSocketAddress sourceAddress;
	uint8 udpBuffer[16] {};
	AT_PORTABLE_TEST_ASSERT(context, receiver->RecvFrom(sourceAddress, udpBuffer, sizeof udpBuffer) == 4);
	AT_PORTABLE_TEST_ASSERT(context, sourceAddress.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, udpBuffer[0] == 0x41 && udpBuffer[1] == 0x54 && udpBuffer[2] == 0x52 && udpBuffer[3] == 0x41);

	receiver->SetOnEvent(nullptr, nullptr, false);
	server->SetOnEvent(nullptr, nullptr, false);
	client->SetOnEvent(nullptr, nullptr, false);
	listener->SetOnEvent(nullptr, nullptr, false);
	receiver->CloseSocket(true);
	sender->CloseSocket(true);
	server->CloseSocket(true);
	client->CloseSocket(true);
	listener->CloseSocket(true);
	return true;
}
