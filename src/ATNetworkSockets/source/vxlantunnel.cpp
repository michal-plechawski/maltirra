// Altirra native socket VXLAN tunnel implementation

#include <cstring>

#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <at/atnetwork/ethernetframe.h>
#include <at/atnetworksockets/internal/vxlantunnel.h>
#include <at/atnetworksockets/nativesockets.h>

ATNetSockVxlanTunnel::ATNetSockVxlanTunnel() {
}

ATNetSockVxlanTunnel::~ATNetSockVxlanTunnel() {
	Shutdown();
}

bool ATNetSockVxlanTunnel::Init(uint32 tunnelAddr, uint16 tunnelSrcPort, uint16 tunnelTgtPort, IATEthernetSegment *ethSeg, uint32 ethClockIndex, IATAsyncDispatcher *dispatcher) {
	mTunnelSrcPort = tunnelSrcPort;
	mTunnelAddress = ATSocketAddress::CreateIPv4(VDFromBE32(tunnelAddr), tunnelTgtPort);
	mEthClockIndex = ethClockIndex;

	mpEthSegment = ethSeg;
	mEthSource = mpEthSegment->AddEndpoint(this);

	mPacketBuffer.resize(4096);

	mpTunnelSocket = ATNetBind(ATSocketAddress::CreateIPv4(mTunnelSrcPort), true);
	if (!mpTunnelSocket)
		return false;

	mpTunnelSocket->SetOnEvent(dispatcher,
		[this](const ATSocketStatus& status) {
			if (status.mbCanRead)
				OnReadPacket();
		},
		true);

	return true;
}

void ATNetSockVxlanTunnel::Shutdown() {
	if (mEthSource) {
		mpEthSegment->RemoveEndpoint(mEthSource);
		mEthSource = 0;
	}

	if (mpTunnelSocket) {
		mpTunnelSocket->SetOnEvent(nullptr, nullptr, false);
		mpTunnelSocket->CloseSocket(true);
		mpTunnelSocket = nullptr;
	}
}

void ATNetSockVxlanTunnel::ReceiveFrame(const ATEthernetPacket& packet, ATEthernetFrameDecodedType decType, const void *decInfo) {
	const uint32 len = 20 + packet.mLength;

	if (mPacketBuffer.size() < len)
		mPacketBuffer.resize(len);

	// Set VXLAN header with VNI absent.
	memset(mPacketBuffer.data(), 0, 8);
	mPacketBuffer[0] = 0x08;

	// Add the Ethernet header around the payload used by the emulated segment.
	memcpy(&mPacketBuffer[8], &packet.mDstAddr, 6);
	memcpy(&mPacketBuffer[14], &packet.mSrcAddr, 6);
	memcpy(&mPacketBuffer[20], packet.mpData, packet.mLength);

	mpTunnelSocket->SendTo(mTunnelAddress, mPacketBuffer.data(), len);
}

void ATNetSockVxlanTunnel::OnReadPacket() {
	for(;;) {
		ATSocketAddress addr;
		const sint32 len = mpTunnelSocket->RecvFrom(addr, mPacketBuffer.data(), mPacketBuffer.size());
		if (len < 0)
			break;

		if (len < 8 + 12 + 2)
			continue;

		const uint8 *vxlanHeader = mPacketBuffer.data();
		if (!(vxlanHeader[0] & 0x08) || (VDReadUnalignedBEU32(&vxlanHeader[4]) & 0xFFFFFF00))
			continue;

		const uint8 *payload = vxlanHeader + 8;
		const uint32 payloadLen = len - 8;
		if (payloadLen < 14 || payloadLen > 1502)
			continue;

		ATEthernetPacket packet {};
		packet.mClockIndex = mEthClockIndex;
		packet.mTimestamp = 100;
		memcpy(&packet.mSrcAddr, payload + 6, 6);
		memcpy(&packet.mDstAddr, payload, 6);
		packet.mpData = payload + 12;
		packet.mLength = payloadLen - 12;
		mpEthSegment->TransmitFrame(mEthSource, packet);
	}
}

void ATCreateNetSockVxlanTunnel(uint32 tunnelAddr, uint16 tunnelSrcPort, uint16 tunnelTgtPort, IATEthernetSegment *ethSeg, uint32 ethClockIndex, IATAsyncDispatcher *dispatcher, IATNetSockVxlanTunnel **pp) {
	ATNetSockVxlanTunnel *tunnel = new ATNetSockVxlanTunnel;

	if (!tunnel->Init(tunnelAddr, tunnelSrcPort, tunnelTgtPort, ethSeg, ethClockIndex, dispatcher)) {
		delete tunnel;
		throw MyMemoryError();
	}

	tunnel->AddRef();
	*pp = tunnel;
}
