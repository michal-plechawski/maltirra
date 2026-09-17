// Altirra portable emulated-to-native socket bridge tests

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <vd2/system/VDString.h>
#include <vd2/system/binary.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <at/atcore/asyncdispatcherimpl.h>
#include <at/atnetwork/emusocket.h>
#include <at/atnetwork/ethernet.h>
#include <at/atnetworksockets/nativesockets.h>
#include <at/atnetworksockets/worker.h>
#include <at/attest/portabletest.h>

namespace {
	class ATSocketSystemScope {
	public:
		ATSocketSystemScope() : mbInitialized(ATSocketInit()) {}
		~ATSocketSystemScope() {
			if (mbInitialized)
				ATSocketShutdown();
		}
		bool mbInitialized;
	};

	template<typename TPredicate>
	bool ATWaitForSocketWorkerEvent(ATAsyncDispatcher& dispatcher, VDSignal& signal,
		TPredicate&& predicate, uint32 timeoutMs = 3000) {
		const uint32 deadline = VDGetCurrentTick() + timeoutMs;
		for (;;) {
#ifdef _WIN32
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#endif
			dispatcher.RunCallbacks();
			if (predicate())
				return true;

			const uint32 now = VDGetCurrentTick();
			if ((sint32)(deadline - now) <= 0)
				return false;
			signal.tryWait(std::min<uint32>(deadline - now, 20));
		}
	}

	class ATMockIpStack final : public IATEmuNetIpStack {
	public:
		uint32 GetIpAddress() const override { return mGatewayAddress; }
		uint32 GetIpNetMask() const override { return VDToBE32(0xFFFFFF00); }
		bool IsLocalOrBroadcastAddress(uint32 ip) const override { return ip == mGatewayAddress; }

		uint32 mGatewayAddress = VDToBE32(0x0A000001);
	};

	class ATMockUdpStack final : public IATEmuNetUdpStack {
	public:
		IATEmuNetIpStack *GetIpStack() const override { return const_cast<ATMockIpStack *>(&mIpStack); }

		bool Bind(uint16 port, IATEmuNetUdpSocketListener *listener) override {
			if (mBoundListener)
				return false;
			mBoundPort = port;
			mBoundListener = listener;
			return true;
		}

		uint16 Bind(IATEmuNetUdpSocketListener *listener) override {
			return Bind(49152, listener) ? 49152 : 0;
		}

		void Unbind(uint16 port, IATEmuNetUdpSocketListener *listener) override {
			if (mBoundPort == port && mBoundListener == listener) {
				mBoundPort = 0;
				mBoundListener = nullptr;
			}
		}

		void SendDatagram(uint32 srcIpAddr, uint16 srcPort, uint32 dstIpAddr, uint16 dstPort,
			const void *data, uint32 dataLen) override {
			mReplySrcIpAddr = srcIpAddr;
			mReplySrcPort = srcPort;
			mReplyDstIpAddr = dstIpAddr;
			mReplyDstPort = dstPort;
			const uint8 *bytes = static_cast<const uint8 *>(data);
			mReply.assign(bytes, bytes + dataLen);
		}

		void SendDatagram(uint32 srcIpAddr, uint16 srcPort, uint32 dstIpAddr, uint16 dstPort,
			const ATEthernetAddr& dstHwAddr, const void *data, uint32 dataLen) override {
			(void)dstHwAddr;
			SendDatagram(srcIpAddr, srcPort, dstIpAddr, dstPort, data, dataLen);
		}

		ATMockIpStack mIpStack;
		uint16 mBoundPort = 0;
		IATEmuNetUdpSocketListener *mBoundListener = nullptr;
		uint32 mReplySrcIpAddr = 0;
		uint16 mReplySrcPort = 0;
		uint32 mReplyDstIpAddr = 0;
		uint16 mReplyDstPort = 0;
		std::vector<uint8> mReply;
	};

	class ATMockStreamSocket final : public vdrefcounted<IATStreamSocket> {
	public:
		void SetOnEvent(IATAsyncDispatcher *dispatcher, vdfunction<void(const ATSocketStatus&)> fn,
			bool callIfReady) override {
			(void)dispatcher;
			(void)fn;
			(void)callIfReady;
		}

		ATSocketStatus GetSocketStatus() const override {
			ATSocketStatus status {};
			status.mbClosed = mbClosed;
			status.mbCanRead = mReadOffset < mOutbound.size();
			status.mbCanWrite = !mbClosed;
			return status;
		}

		void CloseSocket(bool force) override {
			(void)force;
			mbClosed = true;
		}

		void PollSocket() override {}
		ATSocketAddress GetLocalAddress() const override { return mLocalAddress; }
		ATSocketAddress GetRemoteAddress() const override { return mRemoteAddress; }

		sint32 Recv(void *buf, uint32 len) override {
			const uint32 actual = std::min<uint32>(len, mOutbound.size() - mReadOffset);
			if (actual) {
				memcpy(buf, mOutbound.data() + mReadOffset, actual);
				mReadOffset += actual;
			}
			return actual;
		}

		sint32 Send(const void *buf, uint32 len) override {
			if (mbClosed)
				return -1;
			const uint8 *bytes = static_cast<const uint8 *>(buf);
			mInbound.insert(mInbound.end(), bytes, bytes + len);
			return len;
		}

		void ShutdownSocket(bool send, bool receive) override {
			mbSendShutdown |= send;
			mbReceiveShutdown |= receive;
		}

		ATSocketAddress mLocalAddress = ATSocketAddress::CreateIPv4(VDToBE32(0x0A000002), 23456);
		ATSocketAddress mRemoteAddress = ATSocketAddress::CreateIPv4(VDToBE32(0x0A000001), 0);
		std::vector<uint8> mOutbound;
		std::vector<uint8> mInbound;
		uint32 mReadOffset = 0;
		bool mbClosed = false;
		bool mbSendShutdown = false;
		bool mbReceiveShutdown = false;
	};
}

bool ATTestNetSocketWorker(ATPortableTestContext& context) {
	VDSignal dispatchSignal;
	ATAsyncDispatcher dispatcher;
	dispatcher.SetWakeCallback([&] { dispatchSignal.signal(); });
	ATSocketSystemScope socketSystem;
	AT_PORTABLE_TEST_ASSERT(context, socketSystem.mbInitialized);

	ATMockUdpStack udpStack;
	vdrefptr<IATNetSockWorker> worker;
	ATCreateNetSockWorker(&udpStack, nullptr, true, 0, 0, &dispatcher, ~worker);
	AT_PORTABLE_TEST_ASSERT(context, worker != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mBoundPort == 53);
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mBoundListener == worker->AsUdpListener());

	vdrefptr<IATDatagramSocket> receiver;
	ATSocketStatus receiverStatus {};
	uint16 receiverPort = 0;
	for(uint16 candidate = 49503; candidate < 49535; ++candidate) {
		receiver = ATNetBind(ATSocketAddress::CreateLocalhostIPv4(candidate), false);
		AT_PORTABLE_TEST_ASSERT(context, receiver != nullptr);
		receiver->SetOnEvent(&dispatcher,
			[&](const ATSocketStatus& status) { receiverStatus = status; }, true);
		if (ATWaitForSocketWorkerEvent(dispatcher, dispatchSignal, [&] {
				return !receiverStatus.mbConnecting || receiverStatus.mError != ATSocketError::None;
			}) && receiverStatus.mError == ATSocketError::None) {
			receiverPort = candidate;
			break;
		}

		receiver->SetOnEvent(nullptr, nullptr, false);
		receiver->CloseSocket(true);
		receiver = nullptr;
		receiverStatus = {};
	}
	AT_PORTABLE_TEST_ASSERT(context, receiverPort != 0);

	static constexpr uint32 kEmulatedAddress = 0x0A000002;
	static constexpr uint16 kEmulatedPort = 12345;
	static constexpr uint8 kRequest[] { 'A', 'T', 'R', 'A' };
	const ATEthernetAddr hwAddress {};
	worker->AsUdpListener()->OnUdpDatagram(
		hwAddress,
		VDToBE32(kEmulatedAddress),
		kEmulatedPort,
		udpStack.mIpStack.mGatewayAddress,
		receiverPort,
		kRequest,
		sizeof kRequest);

	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketWorkerEvent(dispatcher, dispatchSignal, [&] {
		return receiverStatus.mbCanRead || receiverStatus.mError != ATSocketError::None;
	}));
	AT_PORTABLE_TEST_ASSERT(context, receiverStatus.mError == ATSocketError::None);

	ATSocketAddress bridgeAddress;
	uint8 request[16] {};
	AT_PORTABLE_TEST_ASSERT(context,
		receiver->RecvFrom(bridgeAddress, request, sizeof request) == sizeof kRequest);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(request, kRequest, sizeof kRequest));
	AT_PORTABLE_TEST_ASSERT(context, bridgeAddress.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, bridgeAddress.mIPv4Address == 0x7F000001);

	ATSocketAddress hostAddress;
	ATSocketAddress remoteAddress;
	AT_PORTABLE_TEST_ASSERT(context, worker->GetHostAddressesForLocalAddress(
		false,
		VDToBE32(kEmulatedAddress),
		kEmulatedPort,
		udpStack.mIpStack.mGatewayAddress,
		receiverPort,
		hostAddress,
		remoteAddress));
	AT_PORTABLE_TEST_ASSERT(context, hostAddress.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, hostAddress.mPort == bridgeAddress.mPort);
	AT_PORTABLE_TEST_ASSERT(context, remoteAddress.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, remoteAddress.mIPv4Address == 0x0A000001);
	AT_PORTABLE_TEST_ASSERT(context, remoteAddress.mPort == receiverPort);

	static constexpr uint8 kReply[] { 'O', 'K' };
	AT_PORTABLE_TEST_ASSERT(context,
		receiver->SendTo(bridgeAddress, kReply, sizeof kReply));
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketWorkerEvent(dispatcher, dispatchSignal, [&] {
		return !udpStack.mReply.empty();
	}));
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mReplySrcIpAddr == udpStack.mIpStack.mGatewayAddress);
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mReplySrcPort == receiverPort);
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mReplyDstIpAddr == VDToBE32(kEmulatedAddress));
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mReplyDstPort == kEmulatedPort);
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mReply.size() == sizeof kReply);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(udpStack.mReply.data(), kReply, sizeof kReply));

	vdrefptr<IATListenSocket> tcpListener;
	ATSocketStatus tcpListenerStatus {};
	vdrefptr<ATMockStreamSocket> emulatedSocket;
	vdrefptr<IATSocketHandler> bridgeHandler;
	uint16 tcpPort = 0;
	for(uint16 candidate = 49535; candidate < 49567; ++candidate) {
		tcpListener = ATNetListen(ATSocketAddress::CreateLocalhostIPv4(candidate));
		AT_PORTABLE_TEST_ASSERT(context, tcpListener != nullptr);
		tcpListener->SetOnEvent(&dispatcher,
			[&](const ATSocketStatus& status) { tcpListenerStatus = status; }, true);

		emulatedSocket = new ATMockStreamSocket;
		static constexpr uint8 kTcpRequest[] { 'e', 'm', 'u', '-', 't', 'c', 'p' };
		emulatedSocket->mOutbound.assign(kTcpRequest, kTcpRequest + sizeof kTcpRequest);
		if (worker->AsSocketListener()->OnSocketIncomingConnection(
				VDToBE32(kEmulatedAddress), 23456,
				udpStack.mIpStack.mGatewayAddress, candidate,
				emulatedSocket, ~bridgeHandler)
			&& ATWaitForSocketWorkerEvent(dispatcher, dispatchSignal, [&] {
					return tcpListenerStatus.mbCanAccept || tcpListenerStatus.mError != ATSocketError::None;
				})) {
			tcpPort = candidate;
			break;
		}

		bridgeHandler = nullptr;
		tcpListener->SetOnEvent(nullptr, nullptr, false);
		tcpListener->CloseSocket(true);
		tcpListener = nullptr;
		tcpListenerStatus = {};
		worker->ResetAllConnections();
	}
	AT_PORTABLE_TEST_ASSERT(context, tcpPort != 0);

	vdrefptr<IATStreamSocket> nativeServer = tcpListener->Accept();
	AT_PORTABLE_TEST_ASSERT(context, nativeServer != nullptr);
	ATSocketStatus nativeServerStatus {};
	nativeServer->SetOnEvent(&dispatcher,
		[&](const ATSocketStatus& status) { nativeServerStatus = status; }, true);

	static constexpr uint8 kTcpRequest[] { 'e', 'm', 'u', '-', 't', 'c', 'p' };
	bridgeHandler->OnSocketReadReady(sizeof kTcpRequest);
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketWorkerEvent(dispatcher, dispatchSignal, [&] {
		return nativeServerStatus.mbCanRead || nativeServerStatus.mError != ATSocketError::None;
	}));

	uint8 tcpRequest[32] {};
	AT_PORTABLE_TEST_ASSERT(context,
		nativeServer->Recv(tcpRequest, sizeof tcpRequest) == sizeof kTcpRequest);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(tcpRequest, kTcpRequest, sizeof kTcpRequest));

	static constexpr uint8 kTcpReply[] { 'h', 'o', 's', 't', '-', 't', 'c', 'p' };
	AT_PORTABLE_TEST_ASSERT(context, nativeServer->Send(kTcpReply, sizeof kTcpReply) == sizeof kTcpReply);
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForSocketWorkerEvent(dispatcher, dispatchSignal, [&] {
		return emulatedSocket->mInbound.size() == sizeof kTcpReply;
	}));
	AT_PORTABLE_TEST_ASSERT(context,
		!memcmp(emulatedSocket->mInbound.data(), kTcpReply, sizeof kTcpReply));

	hostAddress = {};
	remoteAddress = {};
	AT_PORTABLE_TEST_ASSERT(context, worker->GetHostAddressesForLocalAddress(
		true,
		VDToBE32(kEmulatedAddress), 23456,
		udpStack.mIpStack.mGatewayAddress, tcpPort,
		hostAddress, remoteAddress));
	AT_PORTABLE_TEST_ASSERT(context, hostAddress.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, hostAddress.mPort != 0);
	AT_PORTABLE_TEST_ASSERT(context, remoteAddress.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, remoteAddress.mIPv4Address == 0x7F000001);
	AT_PORTABLE_TEST_ASSERT(context, remoteAddress.mPort == tcpPort);

	receiver->SetOnEvent(nullptr, nullptr, false);
	receiver->CloseSocket(true);
	nativeServer->SetOnEvent(nullptr, nullptr, false);
	nativeServer->CloseSocket(true);
	tcpListener->SetOnEvent(nullptr, nullptr, false);
	tcpListener->CloseSocket(true);
	worker = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, udpStack.mBoundListener == nullptr);
	return true;
}
