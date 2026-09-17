// Altirra portable VXLAN tunnel tests

#include <algorithm>
#include <cstring>
#include <vector>

#include <vd2/system/VDString.h>
#include <vd2/system/binary.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <at/atcore/asyncdispatcherimpl.h>
#include <at/atnetwork/ethernet.h>
#include <at/atnetworksockets/nativesockets.h>
#include <at/atnetworksockets/vxlantunnel.h>
#include <at/attest/portabletest.h>

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
	bool ATWaitForVxlanEvent(ATAsyncDispatcher& dispatcher, VDSignal& signal, TPredicate&& predicate, uint32 timeoutMs = 3000) {
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

	class ATTestEthernetSegment final : public IATEthernetSegment {
	public:
		uint32 AddEndpoint(IATEthernetEndpoint *endpoint) override {
			mpEndpoint = endpoint;
			return 37;
		}

		void RemoveEndpoint(uint32 endpointId) override {
			if (endpointId == 37)
				mpEndpoint = nullptr;
		}

		IATEthernetClock *GetClock(uint32 clockId) const override { return nullptr; }
		uint32 AddClock(IATEthernetClock *clock) override { return 0; }
		void RemoveClock(uint32 clockId) override {}

		void TransmitFrame(uint32 source, const ATEthernetPacket& packet) override {
			++mTransmittedFrames;
			mSource = source;
			mClockIndex = packet.mClockIndex;
			mTimestamp = packet.mTimestamp;
			mSrcAddr = packet.mSrcAddr;
			mDstAddr = packet.mDstAddr;
			mPayload.assign(packet.mpData, packet.mpData + packet.mLength);
		}

		IATEthernetEndpoint *mpEndpoint = nullptr;
		uint32 mTransmittedFrames = 0;
		uint32 mSource = 0;
		uint32 mClockIndex = 0;
		uint32 mTimestamp = 0;
		ATEthernetAddr mSrcAddr {};
		ATEthernetAddr mDstAddr {};
		std::vector<uint8> mPayload;
	};
}

bool ATTestNetVxlanTunnel(ATPortableTestContext& context) {
	VDSignal dispatchSignal;
	ATAsyncDispatcher dispatcher;
	dispatcher.SetWakeCallback([&] { dispatchSignal.signal(); });
	ATSocketSystemScope socketSystem;
	AT_PORTABLE_TEST_ASSERT(context, socketSystem.mbInitialized);

	uint16 tunnelSourcePort = 0;
	uint16 tunnelTargetPort = 0;
	vdrefptr<IATDatagramSocket> targetSocket;
	ATSocketStatus targetStatus {};
	for(uint16 candidate = 49521; candidate < 49553; candidate += 2) {
		ATSocketStatus sourceProbeStatus {};
		vdrefptr<IATDatagramSocket> sourceProbe = ATNetBind(ATSocketAddress::CreateLocalhostIPv4(candidate), false);
		targetSocket = ATNetBind(ATSocketAddress::CreateLocalhostIPv4(candidate + 1), false);
		AT_PORTABLE_TEST_ASSERT(context, sourceProbe != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, targetSocket != nullptr);

		sourceProbe->SetOnEvent(&dispatcher, [&](const ATSocketStatus& status) { sourceProbeStatus = status; }, true);
		targetSocket->SetOnEvent(&dispatcher, [&](const ATSocketStatus& status) { targetStatus = status; }, true);
		const bool ready = ATWaitForVxlanEvent(dispatcher, dispatchSignal, [&] {
			return ((!sourceProbeStatus.mbConnecting && !targetStatus.mbConnecting)
				|| sourceProbeStatus.mError != ATSocketError::None
				|| targetStatus.mError != ATSocketError::None);
		});

		if (ready && sourceProbeStatus.mError == ATSocketError::None && targetStatus.mError == ATSocketError::None) {
			sourceProbe->CloseSocket(true);
			AT_PORTABLE_TEST_ASSERT(context, ATWaitForVxlanEvent(dispatcher, dispatchSignal, [&] {
				return sourceProbeStatus.mbClosed;
			}));
			sourceProbe->SetOnEvent(nullptr, nullptr, false);
			tunnelSourcePort = candidate;
			tunnelTargetPort = candidate + 1;
			break;
		}

		sourceProbe->SetOnEvent(nullptr, nullptr, false);
		targetSocket->SetOnEvent(nullptr, nullptr, false);
		sourceProbe->CloseSocket(true);
		targetSocket->CloseSocket(true);
		targetSocket = nullptr;
		targetStatus = {};
	}

	AT_PORTABLE_TEST_ASSERT(context, tunnelSourcePort != 0);
	AT_PORTABLE_TEST_ASSERT(context, targetStatus.mError == ATSocketError::None);
	vdrefptr<IATDatagramSocket> sourceSocket = ATNetBind(ATSocketAddress::CreateIPv4(), false);
	AT_PORTABLE_TEST_ASSERT(context, sourceSocket != nullptr);

	ATTestEthernetSegment segment;
	vdrefptr<IATNetSockVxlanTunnel> tunnel;
	ATCreateNetSockVxlanTunnel(
		VDToBE32(UINT32_C(0x7F000001)),
		tunnelSourcePort,
		tunnelTargetPort,
		&segment,
		11,
		&dispatcher,
		~tunnel);
	AT_PORTABLE_TEST_ASSERT(context, tunnel != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, segment.mpEndpoint != nullptr);

	const ATEthernetAddr sourceAddress {{ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 }};
	const ATEthernetAddr targetAddress {{ 0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5 }};
	const uint8 ethernetPayload[] { 0x08, 0x00, 0x45, 0x00, 0x00, 0x14, 0x12, 0x34 };
	ATEthernetPacket packet {};
	packet.mSrcAddr = sourceAddress;
	packet.mDstAddr = targetAddress;
	packet.mpData = ethernetPayload;
	packet.mLength = sizeof ethernetPayload;
	segment.mpEndpoint->ReceiveFrame(packet, kATEthernetFrameDecodedType_None, nullptr);

	AT_PORTABLE_TEST_ASSERT(context, ATWaitForVxlanEvent(dispatcher, dispatchSignal, [&] {
		return targetStatus.mbCanRead || targetStatus.mError != ATSocketError::None;
	}));
	AT_PORTABLE_TEST_ASSERT(context, targetStatus.mError == ATSocketError::None);

	ATSocketAddress datagramSource;
	uint8 vxlanPacket[128] {};
	const sint32 vxlanLength = targetSocket->RecvFrom(datagramSource, vxlanPacket, sizeof vxlanPacket);
	AT_PORTABLE_TEST_ASSERT(context, vxlanLength == 20 + (sint32)sizeof ethernetPayload);
	AT_PORTABLE_TEST_ASSERT(context, datagramSource.mType == ATSocketAddressType::IPv4);
	AT_PORTABLE_TEST_ASSERT(context, datagramSource.mPort == tunnelSourcePort);
	AT_PORTABLE_TEST_ASSERT(context, vxlanPacket[0] == 0x08);
	for(size_t i = 1; i < 8; ++i)
		AT_PORTABLE_TEST_ASSERT(context, vxlanPacket[i] == 0);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(vxlanPacket + 8, targetAddress.mAddr, 6));
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(vxlanPacket + 14, sourceAddress.mAddr, 6));
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(vxlanPacket + 20, ethernetPayload, sizeof ethernetPayload));

	AT_PORTABLE_TEST_ASSERT(context, sourceSocket->SendTo(
		ATSocketAddress::CreateLocalhostIPv4(tunnelSourcePort), vxlanPacket, (uint32)vxlanLength));
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForVxlanEvent(dispatcher, dispatchSignal, [&] {
		return segment.mTransmittedFrames > 0;
	}));

	AT_PORTABLE_TEST_ASSERT(context, segment.mTransmittedFrames == 1);
	AT_PORTABLE_TEST_ASSERT(context, segment.mSource == 37);
	AT_PORTABLE_TEST_ASSERT(context, segment.mClockIndex == 11);
	AT_PORTABLE_TEST_ASSERT(context, segment.mTimestamp == 100);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(segment.mSrcAddr.mAddr, sourceAddress.mAddr, 6));
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(segment.mDstAddr.mAddr, targetAddress.mAddr, 6));
	AT_PORTABLE_TEST_ASSERT(context, segment.mPayload.size() == sizeof ethernetPayload);
	AT_PORTABLE_TEST_ASSERT(context, !memcmp(segment.mPayload.data(), ethernetPayload, sizeof ethernetPayload));

	// Packets without the VXLAN valid-VNI flag or with a non-zero VNI are not
	// part of the VNI 0 tunnel and must be discarded.
	vxlanPacket[0] = 0;
	AT_PORTABLE_TEST_ASSERT(context, sourceSocket->SendTo(
		ATSocketAddress::CreateLocalhostIPv4(tunnelSourcePort), vxlanPacket, (uint32)vxlanLength));
	vxlanPacket[0] = 0x08;
	vxlanPacket[6] = 1;
	AT_PORTABLE_TEST_ASSERT(context, sourceSocket->SendTo(
		ATSocketAddress::CreateLocalhostIPv4(tunnelSourcePort), vxlanPacket, (uint32)vxlanLength));
	vxlanPacket[6] = 0;
	AT_PORTABLE_TEST_ASSERT(context, sourceSocket->SendTo(
		ATSocketAddress::CreateLocalhostIPv4(tunnelSourcePort), vxlanPacket, (uint32)vxlanLength));
	AT_PORTABLE_TEST_ASSERT(context, ATWaitForVxlanEvent(dispatcher, dispatchSignal, [&] {
		return segment.mTransmittedFrames >= 2;
	}));
	AT_PORTABLE_TEST_ASSERT(context, segment.mTransmittedFrames == 2);

	tunnel = nullptr;
	AT_PORTABLE_TEST_ASSERT(context, segment.mpEndpoint == nullptr);
	targetSocket->SetOnEvent(nullptr, nullptr, false);
	targetSocket->CloseSocket(true);
	sourceSocket->CloseSocket(true);
	return true;
}
