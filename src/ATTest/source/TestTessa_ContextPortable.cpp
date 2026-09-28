// Portable Tessa context helper tests.

#include <string>

#include <at/attest/portabletest.h>
#include <vd2/Tessa/Context.h>

namespace {
	class VDTTestContext final : public IVDTContext {
	public:
		void *AsInterface(uint32) override { return nullptr; }
		int AddRef() override { return 1; }
		int Release() override { return 1; }

		const VDTDeviceCaps& GetDeviceCaps() override { return mCaps; }
		bool IsFormatSupportedTexture2D(VDTFormat) override { return false; }
		bool IsMonitorHDREnabled(void *, bool& systemSupport) override { systemSupport = false; return false; }
		bool CreateReadbackBuffer(uint32, uint32, VDTFormat, IVDTReadbackBuffer **) override { return false; }
		bool CreateSurface(uint32, uint32, VDTFormat, VDTUsage, IVDTSurface **) override { return false; }
		bool CreateTexture2D(uint32, uint32, VDTFormat, uint32, VDTUsage, const VDTInitData2D *, IVDTTexture2D **) override { return false; }
		bool CreateVertexProgram(VDTProgramFormat, VDTData, IVDTVertexProgram **) override { return false; }
		bool CreateFragmentProgram(VDTProgramFormat, VDTData, IVDTFragmentProgram **) override { return false; }
		bool CreateComputeProgram(VDTProgramFormat, VDTData, IVDTComputeProgram **) override { return false; }
		bool CreateVertexFormat(const VDTVertexElement *, uint32, IVDTVertexProgram *, IVDTVertexFormat **) override { return false; }
		bool CreateVertexBuffer(uint32, bool, const void *, IVDTVertexBuffer **) override { return false; }
		bool CreateIndexBuffer(uint32, bool, bool, const void *, IVDTIndexBuffer **) override { return false; }
		vdrefptr<IVDTConstantBuffer> CreateConstantBuffer(uint32, const void *) override { return {}; }
		bool CreateBlendState(const VDTBlendStateDesc&, IVDTBlendState **) override { return false; }
		bool CreateRasterizerState(const VDTRasterizerStateDesc&, IVDTRasterizerState **) override { return false; }
		bool CreateSamplerState(const VDTSamplerStateDesc&, IVDTSamplerState **) override { return false; }
		bool CreateSwapChain(const VDTSwapChainDesc&, IVDTSwapChain **) override { return false; }
		IVDTSurface *GetRenderTarget(uint32) const override { return nullptr; }
		bool GetRenderTargetBypass(uint32) const override { return false; }
		void SetVertexFormat(IVDTVertexFormat *) override {}
		void SetVertexProgram(IVDTVertexProgram *) override {}
		void SetFragmentProgram(IVDTFragmentProgram *) override {}
		void SetVertexStream(uint32, IVDTVertexBuffer *, uint32, uint32) override {}
		void SetIndexStream(IVDTIndexBuffer *) override {}
		void SetRenderTarget(uint32, IVDTSurface *, bool) override {}
		void SetBlendState(IVDTBlendState *) override {}
		void SetSamplerStates(uint32, uint32, IVDTSamplerState *const *) override {}
		void SetTextures(uint32, uint32, IVDTTexture *const *) override {}
		void ClearTexturesStartingAt(uint32) override {}
		void SetRasterizerState(IVDTRasterizerState *) override {}
		VDTViewport GetViewport() override { return {}; }
		void SetViewport(const VDTViewport&) override {}
		vdrect32 GetScissorRect() override { return {}; }
		void SetScissorRect(const vdrect32&) override {}
		void VsSetConstantBuffer(uint32, IVDTConstantBuffer *) override {}
		void VsClearConstantBuffersStartingAt(uint32) override {}
		void PsSetConstantBuffer(uint32, IVDTConstantBuffer *) override {}
		void PsClearConstantBuffersStartingAt(uint32) override {}
		void Clear(VDTClearFlags, uint32, float, uint32) override {}
		void DrawPrimitive(VDTPrimitiveType, uint32, uint32) override {}
		void DrawIndexedPrimitive(VDTPrimitiveType, uint32, uint32, uint32, uint32, uint32) override {}
		void CsSetProgram(IVDTComputeProgram *) override {}
		void CsSetConstantBuffer(uint32, IVDTConstantBuffer *) override {}
		void CsSetSamplers(uint32, uint32, IVDTSamplerState *const *) override {}
		void CsSetTextures(uint32, uint32, IVDTTexture *const *) override {}
		void CsClearTexturesStartingAt(uint32) override {}
		void CsSetUnorderedAccessViews(uint32, uint32, IVDTUnorderedAccessView *const *) override {}
		void CsClearUnorderedAccessViewsStartingAt(uint32) override {}
		void CsDispatch(uint32, uint32, uint32) override {}
		uint32 InsertFence() override { return 0; }
		bool CheckFence(uint32) override { return false; }
		vdrefptr<IVDTTimestampQuery> CreateTimestampQuery() override { return {}; }
		vdrefptr<IVDTTimestampFrequencyQuery> CreateTimestampFrequencyQuery() override { return {}; }
		bool RecoverDevice() override { return false; }
		bool OpenScene() override { return false; }
		bool CloseScene() override { return false; }
		bool IsDeviceLost() const override { return false; }
		uint32 GetDeviceLossCounter() const override { return 0; }
		void Present() override {}
		void SetGpuPriority(int) override {}
		void BeginScope(uint32 color, const char *message) override {
			mColor = color;
			mMessage = message;
			++mBeginCount;
		}
		void EndScope() override { ++mEndCount; }

		VDTDeviceCaps mCaps {};
		uint32 mColor = 0;
		std::string mMessage;
		uint32 mBeginCount = 0;
		uint32 mEndCount = 0;
	};
}

bool ATTestTessaContext(ATPortableTestContext& context) {
	VDTTestContext testContext;

	VDTBeginScopeF(&testContext, 0x12345678, "%s %d", "frame", 42);
	AT_PORTABLE_TEST_ASSERT(context, testContext.mColor == 0x12345678);
	AT_PORTABLE_TEST_ASSERT(context, testContext.mMessage == "frame 42");
	AT_PORTABLE_TEST_ASSERT(context, testContext.mBeginCount == 1);

	const std::string longMessage(300, 'x');
	VDTBeginScopeF(&testContext, 7, "%s", longMessage.c_str());
	AT_PORTABLE_TEST_ASSERT(context, testContext.mMessage.size() == 255);
	AT_PORTABLE_TEST_ASSERT(context, testContext.mMessage == std::string(255, 'x'));

	{
		VDTAutoScope scope(testContext, "automatic", 0xAABBCCDD);
		AT_PORTABLE_TEST_ASSERT(context, testContext.mColor == 0xAABBCCDD);
		AT_PORTABLE_TEST_ASSERT(context, testContext.mMessage == "automatic");
		AT_PORTABLE_TEST_ASSERT(context, testContext.mEndCount == 0);
	}

	AT_PORTABLE_TEST_ASSERT(context, testContext.mBeginCount == 3);
	AT_PORTABLE_TEST_ASSERT(context, testContext.mEndCount == 1);
	return true;
}
