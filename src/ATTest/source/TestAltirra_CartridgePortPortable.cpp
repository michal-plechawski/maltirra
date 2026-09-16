// Portable behavioral tests for the cartridge port signal manager.

#include <array>
#include <vector>

#include <at/attest/portabletest.h>
#include <at/atcore/devicecart.h>
#include <cartridgeport.h>

namespace {
	class TestCartridge final : public IATDeviceCartridge {
	public:
		void InitCartridge(IATDeviceCartridgePort *) override {}
		bool IsLeftCartActive() const override { return mActive; }

		void SetCartEnables(bool leftEnable, bool rightEnable, bool cctlEnable) override {
			mLeftEnabled = leftEnable;
			mRightEnabled = rightEnable;
			mCCTLEnabled = cctlEnable;
			mEnableHistory.push_back({ leftEnable, rightEnable, cctlEnable });
		}

		void UpdateCartSense(bool leftActive) override {
			mDownstreamActive = leftActive;
			mSenseHistory.push_back(leftActive);
		}

		bool mActive = false;
		bool mLeftEnabled = false;
		bool mRightEnabled = false;
		bool mCCTLEnabled = false;
		bool mDownstreamActive = false;
		std::vector<std::array<bool, 3>> mEnableHistory;
		std::vector<bool> mSenseHistory;
	};
}

bool ATTestAltirraCartridgePort(ATPortableTestContext& context) {
	ATCartridgePort port;
	std::vector<bool> mappedChanges;
	port.SetLeftMapChangedHandler([&](bool mapped) { mappedChanges.push_back(mapped); });

	TestCartridge internalCart;
	TestCartridge defaultCart;
	TestCartridge passThroughCart;
	uint32 internalId = 0;
	uint32 defaultId = 0;
	uint32 passThroughId = 0;
	port.AddCartridge(&internalCart, kATCartridgePriority_Internal, internalId);
	port.AddCartridge(&defaultCart, kATCartridgePriority_Default, defaultId);
	port.AddCartridge(&passThroughCart, kATCartridgePriority_PassThrough, passThroughId);

	AT_PORTABLE_TEST_ASSERT(context, internalId && defaultId && passThroughId);
	AT_PORTABLE_TEST_ASSERT(context, internalId != defaultId && defaultId != passThroughId);
	AT_PORTABLE_TEST_ASSERT(context, internalCart.mLeftEnabled && internalCart.mRightEnabled && internalCart.mCCTLEnabled);
	AT_PORTABLE_TEST_ASSERT(context, defaultCart.mLeftEnabled && defaultCart.mRightEnabled && defaultCart.mCCTLEnabled);
	AT_PORTABLE_TEST_ASSERT(context, passThroughCart.mLeftEnabled && passThroughCart.mRightEnabled && passThroughCart.mCCTLEnabled);

	port.EnablePassThrough(internalId, false, true, false);
	AT_PORTABLE_TEST_ASSERT(context, port.IsLeftWindowEnabled(internalId));
	AT_PORTABLE_TEST_ASSERT(context, !port.IsLeftWindowEnabled(defaultId));
	AT_PORTABLE_TEST_ASSERT(context, !port.IsLeftWindowEnabled(passThroughId));
	AT_PORTABLE_TEST_ASSERT(context, port.IsRightWindowEnabled(defaultId));
	AT_PORTABLE_TEST_ASSERT(context, !port.IsCCTLEnabled(defaultId));
	AT_PORTABLE_TEST_ASSERT(context, !defaultCart.mLeftEnabled && defaultCart.mRightEnabled && !defaultCart.mCCTLEnabled);
	AT_PORTABLE_TEST_ASSERT(context, !passThroughCart.mLeftEnabled && passThroughCart.mRightEnabled && !passThroughCart.mCCTLEnabled);

	defaultCart.mActive = true;
	port.OnLeftWindowChanged(defaultId, true);
	AT_PORTABLE_TEST_ASSERT(context, !port.IsLeftMapped());
	AT_PORTABLE_TEST_ASSERT(context, port.IsLeftWindowActiveDownstream(internalId));
	AT_PORTABLE_TEST_ASSERT(context, internalCart.mDownstreamActive);
	AT_PORTABLE_TEST_ASSERT(context, mappedChanges.empty());

	port.EnablePassThrough(internalId, true, true, true);
	AT_PORTABLE_TEST_ASSERT(context, port.IsLeftMapped());
	AT_PORTABLE_TEST_ASSERT(context, mappedChanges.size() == 1 && mappedChanges.back());
	AT_PORTABLE_TEST_ASSERT(context, defaultCart.mLeftEnabled && defaultCart.mCCTLEnabled);
	AT_PORTABLE_TEST_ASSERT(context, passThroughCart.mLeftEnabled && passThroughCart.mCCTLEnabled);

	passThroughCart.mActive = true;
	port.OnLeftWindowChanged(passThroughId, true);
	AT_PORTABLE_TEST_ASSERT(context, defaultCart.mDownstreamActive);
	defaultCart.mActive = false;
	port.OnLeftWindowChanged(defaultId, false);
	AT_PORTABLE_TEST_ASSERT(context, port.IsLeftMapped());
	passThroughCart.mActive = false;
	port.OnLeftWindowChanged(passThroughId, false);
	AT_PORTABLE_TEST_ASSERT(context, !port.IsLeftMapped());
	AT_PORTABLE_TEST_ASSERT(context, mappedChanges.size() == 2 && !mappedChanges.back());
	AT_PORTABLE_TEST_ASSERT(context, !defaultCart.mDownstreamActive && !internalCart.mDownstreamActive);

	port.EnablePassThrough(defaultId, true, false, false);
	AT_PORTABLE_TEST_ASSERT(context, port.IsRightWindowEnabled(defaultId));
	AT_PORTABLE_TEST_ASSERT(context, !port.IsRightWindowEnabled(passThroughId));
	AT_PORTABLE_TEST_ASSERT(context, port.IsCCTLEnabled(defaultId));
	AT_PORTABLE_TEST_ASSERT(context, !port.IsCCTLEnabled(passThroughId));
	AT_PORTABLE_TEST_ASSERT(context, passThroughCart.mLeftEnabled && !passThroughCart.mRightEnabled && !passThroughCart.mCCTLEnabled);

	port.EnableCarts(true, false, true);
	AT_PORTABLE_TEST_ASSERT(context, !internalCart.mRightEnabled && !defaultCart.mRightEnabled && !passThroughCart.mRightEnabled);
	port.EnableCarts(true, true, true);
	AT_PORTABLE_TEST_ASSERT(context, internalCart.mRightEnabled && defaultCart.mRightEnabled && !passThroughCart.mRightEnabled);
	port.EnablePassThrough(defaultId, true, true, true);
	AT_PORTABLE_TEST_ASSERT(context, passThroughCart.mRightEnabled && passThroughCart.mCCTLEnabled);

	port.RemoveCartridge(passThroughId, &passThroughCart);
	TestCartridge replacementCart;
	uint32 replacementId = 0;
	port.AddCartridge(&replacementCart, kATCartridgePriority_PassThrough, replacementId);
	AT_PORTABLE_TEST_ASSERT(context, replacementId == passThroughId);
	AT_PORTABLE_TEST_ASSERT(context, replacementCart.mLeftEnabled && replacementCart.mRightEnabled && replacementCart.mCCTLEnabled);
	port.RemoveCartridge(replacementId, &replacementCart);
	port.RemoveCartridge(defaultId, &defaultCart);
	port.RemoveCartridge(internalId, &internalCart);

	return true;
}
