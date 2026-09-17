// Altirra emulated-to-native socket bridge for macOS

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <arpa/inet.h>

#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include <at/atnetwork/emusocket.h>
#include <at/atnetwork/socket.h>
#include <at/atnetworksockets/nativesockets.h>
#include <at/atnetworksockets/worker.h>

namespace {
	class ATNetSockWorker;

	class ATNetSockBridgeHandler final : public vdrefcounted<IATSocketHandler> {
	public:
		ATNetSockBridgeHandler(ATNetSockWorker *parent, IATStreamSocket *nativeSocket, IATStreamSocket *localSocket,
			uint32 srcIpAddr, uint16 srcPort, uint32 dstIpAddr, uint16 dstPort);
		~ATNetSockBridgeHandler();

		uint32 GetSrcIpAddr() const { return mSrcIpAddr; }
		uint16 GetSrcPort() const { return mSrcPort; }
		uint32 GetDstIpAddr() const { return mDstIpAddr; }
		uint16 GetDstPort() const { return mDstPort; }
		ATSocketAddress GetHostAddress() const;
		ATSocketAddress GetRemoteAddress() const;

		void StartNativeEvents(IATAsyncDispatcher *dispatcher);
		void SetLocalSocket(IATStreamSocket *socket);
		void SetSrcAddress(const ATSocketAddress& address);
		void Shutdown();

		void OnSocketOpen() override;
		void OnSocketReadReady(uint32 len) override;
		void OnSocketWriteReady(uint32 len) override;
		void OnSocketClose() override;
		void OnSocketError() override;

	private:
		void OnNativeSocketEvent(const ATSocketStatus& status);
		void TryCopyToNative();
		void TryCopyFromNative();
		void FinishIfClosed();

		ATNetSockWorker *mpParent = nullptr;
		vdrefptr<IATStreamSocket> mpNativeSocket;
		vdrefptr<IATStreamSocket> mpLocalSocket;
		uint32 mSrcIpAddr = 0;
		uint16 mSrcPort = 0;
		uint32 mDstIpAddr = 0;
		uint16 mDstPort = 0;
		bool mbLocalClosed = false;
		bool mbNativeConnected = false;
		bool mbNativeClosed = false;
		bool mbShutdown = false;
		uint32 mRecvBase = 0;
		uint32 mRecvLimit = 0;
		uint32 mSendBase = 0;
		uint32 mSendLimit = 0;
		char mRecvBuffer[1024] {};
		char mSendBuffer[1024] {};
	};

	struct ATUdpConnection {
		uint32 mSrcIpAddr = 0;
		uint32 mDstIpAddr = 0;
		uint16 mSrcPort = 0;
		uint16 mDstPort = 0;
		vdrefptr<IATDatagramSocket> mpSocket;
	};

	class ATNetSockWorker final : public vdrefcounted<IATNetSockWorker>, public IATEmuNetSocketListener, public IATEmuNetUdpSocketListener {
		friend class ATNetSockBridgeHandler;
	public:
		~ATNetSockWorker() { Shutdown(); }

		bool Init(IATEmuNetUdpStack *udp, IATEmuNetTcpStack *tcp, bool externalAccess,
			uint32 forwardingAddr, uint16 forwardingPort, IATAsyncDispatcher *dispatcher);
		void Shutdown();

		IATEmuNetSocketListener *AsSocketListener() override { return this; }
		IATEmuNetUdpSocketListener *AsUdpListener() override { return this; }
		void ResetAllConnections() override;
		bool GetHostAddressesForLocalAddress(bool tcp, uint32 srcIpAddr, uint16 srcPort,
			uint32 dstIpAddr, uint16 dstPort, ATSocketAddress& hostAddr, ATSocketAddress& remoteAddr) const override;

		bool OnSocketIncomingConnection(uint32 srcIpAddr, uint16 srcPort, uint32 dstIpAddr,
			uint16 dstPort, IATStreamSocket *socket, IATSocketHandler **handler) override;
		void OnUdpDatagram(const ATEthernetAddr& srcHwAddr, uint32 srcIpAddr, uint16 srcPort,
			uint32 dstIpAddr, uint16 dstPort, const void *data, uint32 dataLen) override;

	private:
		void ClearConnections();
		ATUdpConnection *CreateUdpConnection(uint32 srcIpAddr, uint16 srcPort,
			uint32 dstIpAddr, uint16 dstPort, bool redirected);
		void OnUdpSocketEvent(ATUdpConnection& connection, const ATSocketStatus& status);
		void OnListenSocketEvent(const ATSocketStatus& status);
		void DeleteConnection(ATNetSockBridgeHandler *handler);

		IATAsyncDispatcher *mpDispatcher = nullptr;
		IATEmuNetTcpStack *mpTcpStack = nullptr;
		IATEmuNetUdpStack *mpUdpStack = nullptr;
		bool mbAllowExternalAccess = false;
		bool mbShutdown = false;
		uint32 mForwardingAddr = 0;
		uint16 mForwardingPort = 0;
		vdrefptr<IATListenSocket> mpTcpListeningSocket;
		std::vector<vdrefptr<ATNetSockBridgeHandler>> mTcpConnections;
		std::vector<ATUdpConnection *> mUdpConnections;
	};

	uint32 ATGetDnsServerAddress() {
		FILE *file = fopen("/etc/resolv.conf", "r");
		if (!file)
			return 0;

		char line[512];
		uint32 address = 0;
		while (fgets(line, sizeof line, file)) {
			char textAddress[INET_ADDRSTRLEN] {};
			if (sscanf(line, " nameserver %15s", textAddress) != 1)
				continue;

			in_addr nativeAddress {};
			if (inet_pton(AF_INET, textAddress, &nativeAddress) == 1) {
				address = ntohl(nativeAddress.s_addr);
				break;
			}
		}

		fclose(file);
		return address;
	}

	ATNetSockBridgeHandler::ATNetSockBridgeHandler(ATNetSockWorker *parent, IATStreamSocket *nativeSocket,
		IATStreamSocket *localSocket, uint32 srcIpAddr, uint16 srcPort, uint32 dstIpAddr, uint16 dstPort)
		: mpParent(parent)
		, mpNativeSocket(nativeSocket)
		, mpLocalSocket(localSocket)
		, mSrcIpAddr(srcIpAddr)
		, mSrcPort(srcPort)
		, mDstIpAddr(dstIpAddr)
		, mDstPort(dstPort)
	{
	}

	ATNetSockBridgeHandler::~ATNetSockBridgeHandler() {
		if (mpNativeSocket) {
			mpNativeSocket->SetOnEvent(nullptr, nullptr, false);
			mpNativeSocket->CloseSocket(true);
		}
		if (mpLocalSocket)
			mpLocalSocket->CloseSocket(true);
	}

	ATSocketAddress ATNetSockBridgeHandler::GetHostAddress() const {
		return mpNativeSocket ? mpNativeSocket->GetLocalAddress() : ATSocketAddress {};
	}

	ATSocketAddress ATNetSockBridgeHandler::GetRemoteAddress() const {
		return mpNativeSocket ? mpNativeSocket->GetRemoteAddress() : ATSocketAddress {};
	}

	void ATNetSockBridgeHandler::StartNativeEvents(IATAsyncDispatcher *dispatcher) {
		mpNativeSocket->SetOnEvent(dispatcher,
			[this](const ATSocketStatus& status) { OnNativeSocketEvent(status); }, true);
	}

	void ATNetSockBridgeHandler::SetLocalSocket(IATStreamSocket *socket) {
		mpLocalSocket = socket;
	}

	void ATNetSockBridgeHandler::SetSrcAddress(const ATSocketAddress& address) {
		if (address.mType == ATSocketAddressType::IPv4) {
			mSrcIpAddr = address.mIPv4Address;
			mSrcPort = address.mPort;
		}
	}

	void ATNetSockBridgeHandler::Shutdown() {
		if (mbShutdown)
			return;

		mbShutdown = true;
		vdrefptr<ATNetSockBridgeHandler> pin(this);

		if (mpNativeSocket) {
			mpNativeSocket->SetOnEvent(nullptr, nullptr, false);
			mpNativeSocket->CloseSocket(true);
			mpNativeSocket = nullptr;
		}

		if (mpLocalSocket) {
			mpLocalSocket->CloseSocket(true);
			mpLocalSocket = nullptr;
		}

		if (mpParent) {
			ATNetSockWorker *parent = mpParent;
			mpParent = nullptr;
			parent->DeleteConnection(this);
		}
	}

	void ATNetSockBridgeHandler::OnNativeSocketEvent(const ATSocketStatus& status) {
		vdrefptr<ATNetSockBridgeHandler> pin(this);
		if (mbShutdown)
			return;

		if (status.mError != ATSocketError::None || status.mbClosed) {
			Shutdown();
			return;
		}

		if (!status.mbConnecting)
			mbNativeConnected = true;

		if (mbNativeConnected) {
			TryCopyToNative();
			TryCopyFromNative();
		}

		if (status.mbRemoteClosed) {
			mbNativeClosed = true;
			TryCopyFromNative();
			if (mpLocalSocket && mRecvBase == mRecvLimit)
				mpLocalSocket->ShutdownSocket(true, false);
		}

		FinishIfClosed();
	}

	void ATNetSockBridgeHandler::OnSocketOpen() {
	}

	void ATNetSockBridgeHandler::OnSocketReadReady(uint32 len) {
		(void)len;
		TryCopyToNative();
	}

	void ATNetSockBridgeHandler::OnSocketWriteReady(uint32 len) {
		(void)len;
		TryCopyFromNative();
	}

	void ATNetSockBridgeHandler::OnSocketClose() {
		if (mbShutdown)
			return;

		mbLocalClosed = true;
		if (mpNativeSocket)
			mpNativeSocket->ShutdownSocket(true, false);
		FinishIfClosed();
	}

	void ATNetSockBridgeHandler::OnSocketError() {
		Shutdown();
	}

	void ATNetSockBridgeHandler::TryCopyToNative() {
		if (mbShutdown || !mbNativeConnected || !mpNativeSocket || !mpLocalSocket)
			return;

		for (;;) {
			if (mSendBase == mSendLimit) {
				const sint32 actual = mpLocalSocket->Recv(mSendBuffer, sizeof mSendBuffer);
				if (actual <= 0)
					break;
				mSendBase = 0;
				mSendLimit = actual;
			}

			const sint32 actual = mpNativeSocket->Send(mSendBuffer + mSendBase, mSendLimit - mSendBase);
			if (actual <= 0)
				break;
			mSendBase += actual;
		}
	}

	void ATNetSockBridgeHandler::TryCopyFromNative() {
		if (mbShutdown || !mbNativeConnected || !mpNativeSocket || !mpLocalSocket)
			return;

		for (;;) {
			if (mRecvBase == mRecvLimit) {
				const sint32 actual = mpNativeSocket->Recv(mRecvBuffer, sizeof mRecvBuffer);
				if (actual <= 0)
					break;
				mRecvBase = 0;
				mRecvLimit = actual;
			}

			const sint32 actual = mpLocalSocket->Send(mRecvBuffer + mRecvBase, mRecvLimit - mRecvBase);
			if (actual <= 0)
				break;
			mRecvBase += actual;
		}

		if (mbNativeClosed && mRecvBase == mRecvLimit)
			mpLocalSocket->ShutdownSocket(true, false);
	}

	void ATNetSockBridgeHandler::FinishIfClosed() {
		if (mbLocalClosed && mbNativeClosed && mRecvBase == mRecvLimit)
			Shutdown();
	}

	bool ATNetSockWorker::Init(IATEmuNetUdpStack *udp, IATEmuNetTcpStack *tcp, bool externalAccess,
		uint32 forwardingAddr, uint16 forwardingPort, IATAsyncDispatcher *dispatcher) {
		mpUdpStack = udp;
		mpTcpStack = tcp;
		mpDispatcher = dispatcher;
		mbAllowExternalAccess = externalAccess;
		mForwardingAddr = forwardingAddr;
		mForwardingPort = forwardingPort;

		if (!mpUdpStack || !mpDispatcher || !mpUdpStack->Bind(53, this))
			return false;

		if (mForwardingAddr) {
			mpTcpListeningSocket = ATNetListen(ATSocketAddress::CreateIPv4(mForwardingPort));
			if (mpTcpListeningSocket) {
				mpTcpListeningSocket->SetOnEvent(mpDispatcher,
					[this](const ATSocketStatus& status) { OnListenSocketEvent(status); }, true);
			}
		}

		return true;
	}

	void ATNetSockWorker::Shutdown() {
		if (mbShutdown)
			return;

		mbShutdown = true;
		ClearConnections();

		if (mpTcpListeningSocket) {
			mpTcpListeningSocket->SetOnEvent(nullptr, nullptr, false);
			mpTcpListeningSocket->CloseSocket(true);
			mpTcpListeningSocket = nullptr;
		}

		if (mpUdpStack) {
			mpUdpStack->Unbind(53, this);
			mpUdpStack = nullptr;
		}

		mpTcpStack = nullptr;
		mpDispatcher = nullptr;
	}

	void ATNetSockWorker::ClearConnections() {
		while (!mTcpConnections.empty())
			mTcpConnections.back()->Shutdown();

		for (ATUdpConnection *connection : mUdpConnections) {
			connection->mpSocket->SetOnEvent(nullptr, nullptr, false);
			connection->mpSocket->CloseSocket(true);
			delete connection;
		}
		mUdpConnections.clear();
	}

	void ATNetSockWorker::ResetAllConnections() {
		ClearConnections();
		if (!mbShutdown && mForwardingAddr)
			CreateUdpConnection(mForwardingAddr, mForwardingPort, 0, mForwardingPort, false);
	}

	bool ATNetSockWorker::GetHostAddressesForLocalAddress(bool tcp, uint32 srcIpAddr, uint16 srcPort,
		uint32 dstIpAddr, uint16 dstPort, ATSocketAddress& hostAddr, ATSocketAddress& remoteAddr) const {
		if (tcp) {
			for (const vdrefptr<ATNetSockBridgeHandler>& handler : mTcpConnections) {
				if (handler->GetSrcIpAddr() == srcIpAddr && handler->GetSrcPort() == srcPort
					&& handler->GetDstIpAddr() == dstIpAddr && handler->GetDstPort() == dstPort) {
					hostAddr = handler->GetHostAddress();
					remoteAddr = handler->GetRemoteAddress();
					return hostAddr.IsValid() || remoteAddr.IsValid();
				}
			}
		} else {
			for (const ATUdpConnection *connection : mUdpConnections) {
				if (connection->mSrcIpAddr != srcIpAddr || connection->mSrcPort != srcPort)
					continue;
				if (connection->mDstIpAddr && (connection->mDstIpAddr != dstIpAddr || connection->mDstPort != dstPort))
					continue;

				hostAddr = connection->mpSocket->GetLocalAddress();
				remoteAddr = ATSocketAddress::CreateIPv4(VDFromBE32(dstIpAddr), dstPort);
				return hostAddr.IsValid();
			}
		}

		return false;
	}

	bool ATNetSockWorker::OnSocketIncomingConnection(uint32 srcIpAddr, uint16 srcPort, uint32 dstIpAddr,
		uint16 dstPort, IATStreamSocket *socket, IATSocketHandler **handler) {
		uint32 redirectedDstIpAddr = dstIpAddr;
		if (mpUdpStack->GetIpStack()->IsLocalOrBroadcastAddress(dstIpAddr))
			redirectedDstIpAddr = VDToBE32(0x7F000001);
		else if (!mbAllowExternalAccess)
			return false;

		vdrefptr<IATStreamSocket> nativeSocket = ATNetConnect(
			ATSocketAddress::CreateIPv4(VDFromBE32(redirectedDstIpAddr), dstPort));
		if (!nativeSocket)
			return false;

		vdrefptr<ATNetSockBridgeHandler> bridge(new ATNetSockBridgeHandler(this, nativeSocket, socket,
			srcIpAddr, srcPort, dstIpAddr, dstPort));
		mTcpConnections.push_back(bridge);
		bridge->StartNativeEvents(mpDispatcher);

		bridge->AddRef();
		*handler = bridge;
		return true;
	}

	void ATNetSockWorker::OnUdpDatagram(const ATEthernetAddr& srcHwAddr, uint32 srcIpAddr, uint16 srcPort,
		uint32 dstIpAddr, uint16 dstPort, const void *data, uint32 dataLen) {
		(void)srcHwAddr;

		uint32 redirectedDstIpAddr = dstIpAddr;
		bool redirected = false;
		if (dstPort == 53) {
			if (!mbAllowExternalAccess)
				return;
			redirectedDstIpAddr = VDToBE32(ATGetDnsServerAddress());
			if (!redirectedDstIpAddr)
				return;
			redirected = true;
		} else if (mpUdpStack->GetIpStack()->IsLocalOrBroadcastAddress(dstIpAddr)) {
			redirectedDstIpAddr = VDToBE32(0x7F000001);
			redirected = true;
		} else if (!mbAllowExternalAccess) {
			return;
		}

		ATUdpConnection *connection = CreateUdpConnection(srcIpAddr, srcPort, dstIpAddr, dstPort, redirected);
		if (connection)
			connection->mpSocket->SendTo(
				ATSocketAddress::CreateIPv4(VDFromBE32(redirectedDstIpAddr), dstPort), data, dataLen);
	}

	ATUdpConnection *ATNetSockWorker::CreateUdpConnection(uint32 srcIpAddr, uint16 srcPort,
		uint32 dstIpAddr, uint16 dstPort, bool redirected) {
		const uint32 keyDstIpAddr = redirected ? dstIpAddr : 0;
		const uint16 keyDstPort = redirected ? dstPort : 0;
		for (ATUdpConnection *connection : mUdpConnections) {
			if (connection->mSrcIpAddr == srcIpAddr && connection->mSrcPort == srcPort
				&& connection->mDstIpAddr == keyDstIpAddr && connection->mDstPort == keyDstPort)
				return connection;
		}

		const uint16 bindPort = !dstIpAddr && dstPort ? dstPort : 0;
		vdrefptr<IATDatagramSocket> socket = ATNetBind(ATSocketAddress::CreateIPv4(bindPort), false);
		if (!socket)
			return nullptr;

		ATUdpConnection *connection = new ATUdpConnection;
		connection->mSrcIpAddr = srcIpAddr;
		connection->mSrcPort = srcPort;
		connection->mDstIpAddr = keyDstIpAddr;
		connection->mDstPort = keyDstPort;
		connection->mpSocket = socket;
		mUdpConnections.push_back(connection);

		socket->SetOnEvent(mpDispatcher,
			[this, connection](const ATSocketStatus& status) { OnUdpSocketEvent(*connection, status); }, true);
		return connection;
	}

	void ATNetSockWorker::OnUdpSocketEvent(ATUdpConnection& connection, const ATSocketStatus& status) {
		if (!status.mbCanRead)
			return;

		char buffer[4096];
		for (;;) {
			ATSocketAddress sourceAddress;
			const sint32 len = connection.mpSocket->RecvFrom(sourceAddress, buffer, sizeof buffer);
			if (len < 0)
				break;
			if (sourceAddress.mType != ATSocketAddressType::IPv4)
				continue;

			mpUdpStack->SendDatagram(
				connection.mDstIpAddr ? connection.mDstIpAddr : VDToBE32(sourceAddress.mIPv4Address),
				connection.mDstPort ? connection.mDstPort : sourceAddress.mPort,
				connection.mSrcIpAddr,
				connection.mSrcPort,
				buffer,
				len);
		}
	}

	void ATNetSockWorker::OnListenSocketEvent(const ATSocketStatus& status) {
		if (!status.mbCanAccept || !mpTcpStack)
			return;

		for (;;) {
			vdrefptr<IATStreamSocket> nativeSocket = mpTcpListeningSocket->Accept();
			if (!nativeSocket)
				break;

			const ATSocketAddress sourceAddress = nativeSocket->GetRemoteAddress();
			if (sourceAddress.mType != ATSocketAddressType::IPv4) {
				nativeSocket->CloseSocket(true);
				continue;
			}

			vdrefptr<ATNetSockBridgeHandler> bridge(new ATNetSockBridgeHandler(this, nativeSocket, nullptr,
				VDToBE32(sourceAddress.mIPv4Address), sourceAddress.mPort, mForwardingAddr, mForwardingPort));
			vdrefptr<IATStreamSocket> localSocket;
			if (!mpTcpStack->Connect(mForwardingAddr, mForwardingPort, *bridge, ~localSocket)) {
				nativeSocket->CloseSocket(true);
				continue;
			}

			bridge->SetLocalSocket(localSocket);
			bridge->SetSrcAddress(localSocket->GetLocalAddress());
			mTcpConnections.push_back(bridge);
			bridge->StartNativeEvents(mpDispatcher);
		}
	}

	void ATNetSockWorker::DeleteConnection(ATNetSockBridgeHandler *handler) {
		auto it = std::find_if(mTcpConnections.begin(), mTcpConnections.end(),
			[handler](const vdrefptr<ATNetSockBridgeHandler>& entry) { return entry == handler; });
		if (it != mTcpConnections.end())
			mTcpConnections.erase(it);
	}
}

void ATCreateNetSockWorker(IATEmuNetUdpStack *udp, IATEmuNetTcpStack *tcp, bool externalAccess,
	uint32 forwardingAddr, uint16 forwardingPort, IATAsyncDispatcher *dispatcher, IATNetSockWorker **pp) {
	ATNetSockWorker *worker = new ATNetSockWorker;
	if (!worker->Init(udp, tcp, externalAccess, forwardingAddr, forwardingPort, dispatcher)) {
		delete worker;
		throw MyMemoryError();
	}

	worker->AddRef();
	*pp = worker;
}
