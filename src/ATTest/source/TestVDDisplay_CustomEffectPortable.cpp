// Portable VDDisplay custom effect helper tests.

#include <algorithm>
#include <bit>
#include <climits>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include <at/attest/portabletest.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/VDDisplay/internal/customeffectbase.h>
#include <vd2/VDDisplay/internal/customeffectpassbase.h>
#include <vd2/VDDisplay/internal/customeffectutils.h>

namespace {
	class TestCustomEffectPass final : public VDDCustomEffectPassBase {
	public:
		using VDDCustomEffectPassBase::VDDCustomEffectPassBase;

		void ResetVariables(VDDCustomEffectVarStorage&, uint32, bool) override {}

		void Parse(const VDDisplayCustomShaderProps& props) {
			ParseCommonProps(props);
		}

		vdint2 GetRenderSize(const vdint2& sourceSize, const vdint2& viewportSize) const {
			return ComputeRenderSize(sourceSize, viewportSize);
		}
	};

	class TestParsedEffectPass final : public VDDCustomEffectPassBase {
	public:
		TestParsedEffectPass(uint32 passIndex, const VDDisplayCustomShaderProps& props, std::vector<std::string>& events)
			: VDDCustomEffectPassBase(passIndex)
			, mEvents(events) {
			ParseCommonProps(props);
		}

		void ResetVariables(VDDCustomEffectVarStorage&, uint32 prevOutputFramesNeeded, bool outputFilteringNeeded) override {
			mEvents.push_back(
				"reset:" + std::to_string(mPassIndex)
				+ ":" + std::to_string(prevOutputFramesNeeded)
				+ ":" + (outputFilteringNeeded ? "linear" : "point"));
		}

	private:
		std::vector<std::string>& mEvents;
	};

	class TestCustomEffectPipeline final : public VDDCustomEffectBase {
	public:
		~TestCustomEffectPipeline() {
			for(VDDCustomEffectPassBase *pass : mPasses)
				delete pass;
		}

		bool ContainsFinalBlit() const override { return false; }
		bool HasTimingInfo() const override { return false; }
		vdspan<const VDDisplayCustomShaderPassInfo> GetPassTimings() override { return {}; }

		bool IsNewFrame() const { return mbNewFrame; }

		std::vector<std::string> mEvents;
		std::vector<VDStringW> mTexturePaths;
		std::vector<VDStringW> mPassBasePaths;
		uint32 mProfilingInitCount = 0;

	protected:
		void LoadTexture(const char *name, const wchar_t *path, bool linear) override {
			mEvents.push_back(std::string("texture:") + name + (linear ? ":linear" : ":point"));
			mTexturePaths.emplace_back(path);
		}

		void BeginPasses(uint32 numPasses) override {
			mEvents.push_back("begin:" + std::to_string(numPasses));
		}

		void AddPass(uint32 passIndex, const VDDisplayCustomShaderProps& props, const char *shaderPath, const wchar_t *basePath) override {
			mEvents.push_back("pass:" + std::to_string(passIndex) + ":" + shaderPath);
			mPassBasePaths.emplace_back(basePath);
			mMaxPrevFramesPerPass[passIndex] = passIndex + 2;
			mPasses.push_back(new TestParsedEffectPass(passIndex, props, mEvents));
		}

		void EndPasses() override {
			mEvents.push_back("end");
		}

		void InitProfiling() override {
			++mProfilingInitCount;
			mEvents.push_back("profiling");
		}
	};

	class TestCustomEffectConfigFile {
	public:
		TestCustomEffectConfigFile() {
			VDStringW name;
			name.sprintf(
				L"altirra-customeffect-%u-%llu.cgp",
				static_cast<unsigned>(VDGetCurrentProcessId()),
				static_cast<unsigned long long>(VDGetCurrentTick64()));
			mPath = VDGetFullPath(name.c_str());
		}

		~TestCustomEffectConfigFile() {
			VDRemoveFile(mPath.c_str());
		}

		void Write(const char *contents) const {
			VDFileStream stream(mPath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
			stream.Write(contents, static_cast<sint32>(strlen(contents)));
		}

		VDStringW mPath;
	};

	template<typename T_Fn>
	bool ThrowsVDException(T_Fn&& fn) {
		try {
			fn();
		} catch(const VDException&) {
			return true;
		}

		return false;
	}

	float FloatFromBits(uint32 value) {
		return std::bit_cast<float>(value);
	}
}

bool ATTestVDDisplayCustomEffect(ATPortableTestContext& context) {
	auto frameRef = VDDCustomEffectFrameRef::Parse("$IN", 20);
	AT_PORTABLE_TEST_ASSERT(context, frameRef.mbValid && frameRef.mPassIndex == 20 && frameRef.mElementIndex == 0);
	frameRef = VDDCustomEffectFrameRef::Parse("ORIG", 20);
	AT_PORTABLE_TEST_ASSERT(context, frameRef.mbValid && frameRef.mPassIndex == 0 && frameRef.mElementIndex == 0);
	frameRef = VDDCustomEffectFrameRef::Parse("PREV6", 20);
	AT_PORTABLE_TEST_ASSERT(context, frameRef.mbValid && frameRef.mPassIndex == 0 && frameRef.mElementIndex == 7);
	frameRef = VDDCustomEffectFrameRef::Parse("PASS12", 20);
	AT_PORTABLE_TEST_ASSERT(context, frameRef.mbValid && frameRef.mPassIndex == 12 && frameRef.mElementIndex == 0);
	frameRef = VDDCustomEffectFrameRef::Parse("PASSPREV12", 20);
	AT_PORTABLE_TEST_ASSERT(context, frameRef.mbValid && frameRef.mPassIndex == 7 && frameRef.mElementIndex == 0);
	AT_PORTABLE_TEST_ASSERT(context, !VDDCustomEffectFrameRef::Parse("PASS4294967296", 20).mbValid);
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([] { VDDCustomEffectFrameRef::Parse("PASS20", 20); }));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([] { VDDCustomEffectFrameRef::Parse("PREV7", 20); }));

	struct StridedValue {
		uint32 mValue;
		uint32 mPadding;
	};
	StridedValue stridedValues[] {{ 10, 0 }, { 20, 0 }, { 30, 0 }};
	VDDTexSpecView<uint32> stridedView(&stridedValues[0].mValue, 3, sizeof(StridedValue));
	AT_PORTABLE_TEST_ASSERT(context, stridedView.size() == 3);
	AT_PORTABLE_TEST_ASSERT(context, stridedView[1] == 20);
	AT_PORTABLE_TEST_ASSERT(context, stridedView.begin() < stridedView.end());
	AT_PORTABLE_TEST_ASSERT(context, stridedView.end() > stridedView.begin());
	AT_PORTABLE_TEST_ASSERT(context, stridedView.end() - stridedView.begin() == 3);
	const VDDTexSpecView<uint32>& constStridedView = stridedView;
	AT_PORTABLE_TEST_ASSERT(context, *constStridedView.begin() == 10);
	const VDDTexSpecView<const uint32> convertedStridedView = stridedView;
	AT_PORTABLE_TEST_ASSERT(context, convertedStridedView[2] == 30);

	VDDCsPropKeyView parsedKey(VDStringSpanA("scale_x12"));
	AT_PORTABLE_TEST_ASSERT(context, parsedKey.mBaseName == "scale_x");
	AT_PORTABLE_TEST_ASSERT(context, parsedKey.mCounter == 12);
	AT_PORTABLE_TEST_ASSERT(context, parsedKey.ToString() == "scale_x12");

	VDDisplayCustomShaderProps basicProps;
	AT_PORTABLE_TEST_ASSERT(context, basicProps.Add(VDDCsPropKeyView("answer", nullptr), VDStringSpanA("42")));
	AT_PORTABLE_TEST_ASSERT(context, !basicProps.Add(VDDCsPropKeyView("answer", nullptr), VDStringSpanA("43")));
	AT_PORTABLE_TEST_ASSERT(context, basicProps.TryGetInt(VDDCsPropKeyView("answer", nullptr)) == 42);
	AT_PORTABLE_TEST_ASSERT(context, basicProps.Add(VDDCsPropKeyView("bad_int", nullptr), VDStringSpanA("42junk")));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([&] { basicProps.TryGetInt(VDDCsPropKeyView("bad_int", nullptr)); }));
	AT_PORTABLE_TEST_ASSERT(context, basicProps.Add(VDDCsPropKeyView("disabled", nullptr), VDStringSpanA("false")));
	AT_PORTABLE_TEST_ASSERT(context, !basicProps.GetBool(VDDCsPropKeyView("disabled", nullptr), true));

	VDDCustomEffectVarStorage varStorage;
	AT_PORTABLE_TEST_ASSERT(context, varStorage.GetVarOffset(VDDCustomEffectVariable::VideoSize, 3, 0) < 0);
	const auto videoGather = varStorage.RequestVector(8, VDDCustomEffectVariable::VideoSize, 3, 7);
	const auto videoGatherAgain = varStorage.RequestVector(8, VDDCustomEffectVariable::VideoSize, 3, 99);
	AT_PORTABLE_TEST_ASSERT(context, videoGather.mOffset[0] == videoGatherAgain.mOffset[0]);
	const sint32 videoOffset = varStorage.GetVarOffset(VDDCustomEffectVariable::VideoSize, 3, 0);
	AT_PORTABLE_TEST_ASSERT(context, videoOffset >= 0);
	varStorage.SetVector(videoOffset, vdfloat4 { 1.0f, 2.0f, 3.0f, 4.0f });

	uint32 vectorDestination[8] {};
	varStorage.GatherVecs(vectorDestination, vdspan<const VDDCustomEffectVec4Gather>(&videoGather, 1));
	AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(vectorDestination[2]) == 1.0f);
	AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(vectorDestination[3]) == 2.0f);
	AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(vectorDestination[4]) == 3.0f);
	AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(vectorDestination[5]) == 4.0f);

	const VDDCustomEffectScalarGather scalarGather { 0, videoGather.mOffset[0] };
	uint32 scalarDestination = 0;
	varStorage.GatherBools(&scalarDestination, vdspan<const VDDCustomEffectScalarGather>(&scalarGather, 1));
	AT_PORTABLE_TEST_ASSERT(context, scalarDestination == 1);
	scalarDestination = 0;
	varStorage.GatherFloats(&scalarDestination, vdspan<const VDDCustomEffectScalarGather>(&scalarGather, 1));
	AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(scalarDestination) == 1.0f);

	const auto frameGather0 = varStorage.RequestVector(0, VDDCustomEffectVariable::FrameCount, 3, 0);
	const auto frameGather1 = varStorage.RequestVector(0, VDDCustomEffectVariable::FrameCount, 3, 1);
	AT_PORTABLE_TEST_ASSERT(context, frameGather0.mOffset[0] != frameGather1.mOffset[0]);
	VDDCEFrameParamOffsets frameOffsets;
	varStorage.ResolveFrameParamOffsets(frameOffsets, 3, 1);
	AT_PORTABLE_TEST_ASSERT(context, frameOffsets.mFrameCount == (sint32)frameGather1.mOffset[0]);

	const auto rowMajorGather = varStorage.RequestRowMajorMatrix(0, VDDCustomEffectVariable::ModelViewProj, 0, 0);
	const sint32 matrixOffset = varStorage.GetVarOffset(VDDCustomEffectVariable::ModelViewProj, 0, 0);
	const vdfloat4x4 matrix {
		{ 1, 2, 3, 4 },
		{ 5, 6, 7, 8 },
		{ 9, 10, 11, 12 },
		{ 13, 14, 15, 16 }
	};
	varStorage.SetMatrix(matrixOffset, matrix);
	uint32 matrixDestination[16] {};
	varStorage.GatherVecs(matrixDestination, vdspan<const VDDCustomEffectVec4Gather>(rowMajorGather.mVec, 4));
	for(uint32 i=0; i<16; ++i)
		AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(matrixDestination[i]) == (float)(i + 1));

	const auto columnMajorGather = varStorage.RequestColumnMajorMatrix(0, VDDCustomEffectVariable::ModelViewProj, 0, 0);
	std::fill(std::begin(matrixDestination), std::end(matrixDestination), 0);
	varStorage.GatherVecs(matrixDestination, vdspan<const VDDCustomEffectVec4Gather>(columnMajorGather.mVec, 4));
	for(uint32 row=0; row<4; ++row) {
		for(uint32 column=0; column<4; ++column)
			AT_PORTABLE_TEST_ASSERT(context, FloatFromBits(matrixDestination[row * 4 + column]) == (float)(column * 4 + row + 1));
	}

	VDDisplayCustomShaderProps defaultProps;
	TestCustomEffectPass defaultPass(2);
	defaultPass.Parse(defaultProps);
	const vdint2 defaultSize = defaultPass.GetRenderSize(vdint2 { 320, 200 }, vdint2 { 1280, 720 });
	AT_PORTABLE_TEST_ASSERT(context, !defaultPass.HasScalingFactor());
	AT_PORTABLE_TEST_ASSERT(context, defaultPass.IsInputFiltered());
	AT_PORTABLE_TEST_ASSERT(context, defaultSize == vdint2(320, 200));

	VDDisplayCustomShaderProps props;
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("scale_type_x", 2), VDStringSpanA("viewport")));
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("scale_x", 2), VDStringSpanA("0.5")));
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("scale_type_y", 2), VDStringSpanA("absolute")));
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("scale_y", 2), VDStringSpanA("240")));
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("frame_count_mod", 2), VDStringSpanA("8")));
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("srgb_framebuffer", 2), VDStringSpanA("true")));
	AT_PORTABLE_TEST_ASSERT(context, props.Add(VDDCsPropKeyView("filter_linear", 2), VDStringSpanA("false")));
	TestCustomEffectPass pass(2);
	pass.Parse(props);
	const vdint2 scaledSize = pass.GetRenderSize(vdint2 { 320, 200 }, vdint2 { 1280, 720 });
	AT_PORTABLE_TEST_ASSERT(context, pass.HasScalingFactor());
	AT_PORTABLE_TEST_ASSERT(context, pass.IsOutputSrgb());
	AT_PORTABLE_TEST_ASSERT(context, !pass.IsOutputFloat());
	AT_PORTABLE_TEST_ASSERT(context, !pass.IsInputFiltered());
	AT_PORTABLE_TEST_ASSERT(context, pass.GetFrameCountLimit() == 7);
	AT_PORTABLE_TEST_ASSERT(context, scaledSize == vdint2(640, 240));

	VDDisplayCustomShaderProps invalidFrameCountProps;
	AT_PORTABLE_TEST_ASSERT(context, invalidFrameCountProps.Add(VDDCsPropKeyView("frame_count_mod", 2), VDStringSpanA("-1")));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([&] { TestCustomEffectPass(2).Parse(invalidFrameCountProps); }));

	VDDisplayCustomShaderProps invalidFramebufferProps;
	AT_PORTABLE_TEST_ASSERT(context, invalidFramebufferProps.Add(VDDCsPropKeyView("srgb_framebuffer", 2), VDStringSpanA("true")));
	AT_PORTABLE_TEST_ASSERT(context, invalidFramebufferProps.Add(VDDCsPropKeyView("float_framebuffer", 2), VDStringSpanA("true")));
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([&] { TestCustomEffectPass(2).Parse(invalidFramebufferProps); }));

	TestCustomEffectConfigFile config;
	config.Write(
		"textures = LUT ; mask\n"
		"LUT = lut.png\n"
		"LUT_linear = false\n"
		"mask = textures/mask.tga\n"
		"shaders = 2\n"
		"shader0 = first.fx\n"
		"shader1 = second.fx\n"
		"filter_linear1 = true\n"
		"shader_show_stats = true\n");

	TestCustomEffectPipeline pipeline;
	pipeline.Parse(config.mPath.c_str());
	const VDStringW configBasePath = VDFileSplitPathLeftSpan(VDStringSpanW(config.mPath));
	const std::vector<std::string> expectedEvents {
		"texture:LUT:point",
		"texture:mask:linear",
		"begin:2",
		"pass:0:first.fx",
		"pass:1:second.fx",
		"end",
		"reset:0:2:linear",
		"reset:1:3:point",
		"profiling",
	};
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mEvents == expectedEvents);
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mTexturePaths.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mTexturePaths[0] == VDMakePath(configBasePath.c_str(), L"lut.png"));
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mTexturePaths[1] == VDMakePath(configBasePath.c_str(), L"textures/mask.tga"));
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mPassBasePaths.size() == 2);
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mPassBasePaths[0] == configBasePath);
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mPassBasePaths[1] == configBasePath);
	AT_PORTABLE_TEST_ASSERT(context, pipeline.GetMaxPrevFrames() == 2);
	AT_PORTABLE_TEST_ASSERT(context, pipeline.mProfilingInitCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, !pipeline.IsNewFrame());
	pipeline.IncrementFrame();
	AT_PORTABLE_TEST_ASSERT(context, pipeline.IsNewFrame());

	config.Write("textures = missing\nshader0 = only.fx\n");
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([&] { TestCustomEffectPipeline().Parse(config.mPath.c_str()); }));
	config.Write("shaders = 0\n");
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([&] { TestCustomEffectPipeline().Parse(config.mPath.c_str()); }));
	config.Write("shaders = 2\nshader0 = only.fx\n");
	AT_PORTABLE_TEST_ASSERT(context, ThrowsVDException([&] { TestCustomEffectPipeline().Parse(config.mPath.c_str()); }));

	config.Write("shader0 = only.fx\n");
	VDVideoDisplaySetShowCustomShaderStats(true);
	TestCustomEffectPipeline globallyProfiledPipeline;
	globallyProfiledPipeline.Parse(config.mPath.c_str());
	VDVideoDisplaySetShowCustomShaderStats(false);
	AT_PORTABLE_TEST_ASSERT(context, globallyProfiledPipeline.mProfilingInitCount == 1);

	return true;
}
