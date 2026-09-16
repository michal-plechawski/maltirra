// Altirra portable printer device tests

#include <cmath>
#include <vector>

#include <at/atcore/device.h>
#include <at/atcore/propertyset.h>
#include <at/attest/portabletest.h>
#include <printer.h>
#include <printerfont.h>
#include <printeroutput.h>

namespace {
	bool Near(float a, float b) {
		return std::fabs(a - b) < 0.0001f;
	}

	class PrinterDeviceManager final : public IATDeviceManager {
	public:
		void *GetService(uint32 iid) override {
			if (iid == IATPrinterOutputManager::kTypeID)
				return static_cast<IATPrinterOutputManager *>(&mOutputManager);

			return nullptr;
		}

		void NotifyDeviceStatusChanged(IATDevice&) override {}

		ATPrinterOutputManager mOutputManager;
	};

	class CapturingPrinterOutput final : public vdrefcounted<IATPrinterOutput> {
	public:
		void *AsInterface(uint32 iid) override {
			return iid == IATPrinterOutput::kTypeID ? static_cast<IATPrinterOutput *>(this) : nullptr;
		}

		bool WantUnicode() const override { return false; }

		void WriteRaw(const uint8 *buf, size_t len) override {
			mBytes.insert(mBytes.end(), buf, buf + len);
		}

		std::vector<uint8> mBytes;
	};

	class CapturingPrinterOutputManager final : public vdrefcounted<IATPrinterOutputManager> {
	public:
		CapturingPrinterOutputManager()
			: mpOutput(new CapturingPrinterOutput)
		{
		}

		vdrefptr<IATPrinterOutput> CreatePrinterOutput(const wchar_t *) override {
			return mpOutput;
		}

		vdrefptr<IATPrinterGraphicalOutput> CreatePrinterGraphicalOutput(
			const wchar_t *, const ATPrinterGraphicsSpec&) override
		{
			return nullptr;
		}

		vdrefptr<CapturingPrinterOutput> mpOutput;
	};

	class CapturingDeviceManager final : public IATDeviceManager {
	public:
		explicit CapturingDeviceManager(IATPrinterOutputManager& outputManager)
			: mOutputManager(outputManager)
		{
		}

		void *GetService(uint32 iid) override {
			return iid == IATPrinterOutputManager::kTypeID ? &mOutputManager : nullptr;
		}

		void NotifyDeviceStatusChanged(IATDevice&) override {}

		IATPrinterOutputManager& mOutputManager;
	};
}

bool ATTestAltirraPrinters(ATPortableTestContext& context) {
	// Verify the generic printer's public protocol and portable settings.
	ATDevicePrinter genericPrinter;
	AT_PORTABLE_TEST_ASSERT(context, genericPrinter.IsSupportedDeviceId(0x40));
	AT_PORTABLE_TEST_ASSERT(context, !genericPrinter.IsSupportedDeviceId(0x41));
	AT_PORTABLE_TEST_ASSERT(context, genericPrinter.IsSupportedOrientation(0x4E));
	AT_PORTABLE_TEST_ASSERT(context, genericPrinter.IsSupportedOrientation(0x53));
	AT_PORTABLE_TEST_ASSERT(context, !genericPrinter.IsSupportedOrientation(0));
	AT_PORTABLE_TEST_ASSERT(context, genericPrinter.GetWidthForOrientation(0x4E) == 40);
	AT_PORTABLE_TEST_ASSERT(context, genericPrinter.GetWidthForOrientation(0x53) == 29);

	ATPropertySet settings;
	settings.SetEnum("translation_mode", ATPrinterPortTranslationMode::AtasciiToUtf8);
	AT_PORTABLE_TEST_ASSERT(context, genericPrinter.SetSettings(settings));
	settings.Clear();
	genericPrinter.GetSettings(settings);
	AT_PORTABLE_TEST_ASSERT(context,
		settings.GetEnum<ATPrinterPortTranslationMode>("translation_mode") ==
			ATPrinterPortTranslationMode::AtasciiToUtf8);

	// The default raw-output mode strips the inverse-video bit and translates
	// ATASCII EOL to carriage return, stopping at the first EOL.
	CapturingPrinterOutputManager captureManager;
	CapturingDeviceManager captureDeviceManager(captureManager);
	ATDevicePrinter rawPrinter;
	rawPrinter.SetManager(&captureDeviceManager);
	settings.Clear();
	settings.SetEnum("translation_mode", ATPrinterPortTranslationMode::Default);
	rawPrinter.SetSettings(settings);
	uint8 rawLine[] { 0xC1, 0x9B, 0x42 };
	rawPrinter.HandleFrameInternal(0x4E, rawLine, sizeof rawLine, false);
	AT_PORTABLE_TEST_ASSERT(context,
		captureManager.mpOutput->mBytes == std::vector<uint8>({ 0x41, 0x0D }));

	// Exercise the 825 byte-stream interpreter through a real graphical output.
	PrinterDeviceManager manager;
	ATDevicePrinter825 printer825;
	printer825.SetManager(&manager);
	printer825.Init();
	printer825.ColdReset();
	AT_PORTABLE_TEST_ASSERT(context, !printer825.WantUnicode());
	AT_PORTABLE_TEST_ASSERT(context, manager.mOutputManager.GetGraphicalOutputCount() == 1);

	ATPrinterGraphicalOutput& output = manager.mOutputManager.GetGraphicalOutput(0);
	const uint8 letterA = 'A';
	printer825.WriteRaw(&letterA, 1);

	ATPrinterGraphicalOutput::CullInfo cullInfo {};
	const vdrect32f page(0, 0, 216, 40);
	AT_PORTABLE_TEST_ASSERT(context, output.PreCull(cullInfo, page));
	vdfastvector<ATPrinterGraphicalOutput::RenderColumn> columns;
	float renderY = 0;
	AT_PORTABLE_TEST_ASSERT(context, output.ExtractNextLine(columns, renderY, cullInfo, page));
	AT_PORTABLE_TEST_ASSERT(context, Near(renderY, 8.0f));

	std::vector<ATPrinterGraphicalOutput::RenderColumn> expected;
	const uint8 *fontColumns = &g_ATPrinterFont825Mono.mColumns[
		(letterA - 0x20) * g_ATPrinterFont825Mono.kWidth];
	for(uint32 i = 0; i < g_ATPrinterFont825Mono.kWidth; ++i) {
		if (fontColumns[i])
			expected.push_back({ 8.0f + i * 0.254f, fontColumns[i] });
	}

	AT_PORTABLE_TEST_ASSERT(context, columns.size() == expected.size());
	for(size_t i = 0; i < expected.size(); ++i) {
		AT_PORTABLE_TEST_ASSERT(context, Near(columns[i].mX, expected[i].mX));
		AT_PORTABLE_TEST_ASSERT(context, columns[i].mPins == expected[i].mPins);
	}

	// Underlining a blank character produces every other ninth-pin dot.
	output.Clear();
	printer825.ColdReset();
	const uint8 underlinedSpace[] { 0x0F, ' ' };
	printer825.WriteRaw(underlinedSpace, sizeof underlinedSpace);
	AT_PORTABLE_TEST_ASSERT(context, output.PreCull(cullInfo, page));
	columns.clear();
	AT_PORTABLE_TEST_ASSERT(context, output.ExtractNextLine(columns, renderY, cullInfo, page));
	AT_PORTABLE_TEST_ASSERT(context, columns.size() == 5);
	for(size_t i = 0; i < columns.size(); ++i) {
		AT_PORTABLE_TEST_ASSERT(context, Near(columns[i].mX, 8.0f + (float)i * 0.508f));
		AT_PORTABLE_TEST_ASSERT(context, columns[i].mPins == 0x100);
	}

	printer825.Shutdown();
	AT_PORTABLE_TEST_ASSERT(context, manager.mOutputManager.GetGraphicalOutputCount() == 0);
	return true;
}
