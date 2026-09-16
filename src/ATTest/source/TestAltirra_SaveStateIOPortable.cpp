// Portable tests for the JSON and ZIP save-state serializer.

#include <cstring>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

#include <at/atcore/serialization.h>
#include <at/atcore/snapshotimpl.h>
#include <at/attest/portabletest.h>
#include <vd2/system/file.h>
#include <vd2/system/zip.h>
#include <vd2/vdjson/jsonreader.h>
#include <vd2/vdjson/jsonvalue.h>

#include <savestateio.h>

namespace {
	struct PortableSaveStateChild final : public ATSnapExchangeObject<PortableSaveStateChild, "PortableSaveStateChild"> {
		template<ATExchanger T>
		void Exchange(T& ex) {
			ex.Transfer("number", &mNumber);
			ex.Transfer("signed_number", &mSignedNumber);
			ex.Transfer("ratio", &mRatio);
			ex.Transfer("enabled", &mbEnabled);
			ex.Transfer("label", &mLabel);
		}

		uint32 mNumber = 0;
		sint64 mSignedNumber = 0;
		double mRatio = 0;
		bool mbEnabled = false;
		VDStringA mLabel;
	};

	struct PortableSaveStateRoot final : public ATSnapExchangeObject<PortableSaveStateRoot, "PortableSaveStateRoot"> {
		template<ATExchanger T>
		void Exchange(T& ex) {
			ex.Transfer("child", &mpChild);
			ex.Transfer("same_child", &mpSameChild);
			ex.TransferInline("inline_child", &mpInlineChild);
			ex.TransferArray("numbers", mNumbers);
			ex.Transfer("wide_label", &mWideLabel);
			ex.Transfer("large_unsigned", &mLargeUnsigned);
		}

		vdrefptr<PortableSaveStateChild> mpChild;
		vdrefptr<PortableSaveStateChild> mpSameChild;
		vdrefptr<PortableSaveStateChild> mpInlineChild;
		uint16 mNumbers[4] {};
		VDStringW mWideLabel;
		uint64 mLargeUnsigned = 0;
	};

	struct PortableSaveStatePackage final : public ATSnapExchangeObject<PortableSaveStatePackage, "PortableSaveStatePackage"> {
		template<ATExchanger T>
		void Exchange(T& ex) {
			ex.Transfer("first", &mpFirst);
			ex.Transfer("second", &mpSecond);
		}

		vdrefptr<ATSaveStateMemoryBuffer> mpFirst;
		vdrefptr<ATSaveStateMemoryBuffer> mpSecond;
	};

	bool VerifyChild(const PortableSaveStateChild *child, uint32 number, const char *label) {
		return child
			&& child->mNumber == number
			&& child->mSignedNumber == -INT64_C(0x123456789)
			&& child->mRatio == 1.25
			&& child->mbEnabled
			&& child->mLabel == label;
	}

	bool TestJSONUnicodeInput(ATPortableTestContext& context) {
		static constexpr char kEscapedJSON[] = "{\"text\":\"\\u017C\\uD83D\\uDE80\"}";
		const VDStringW expected(L"\u017C\U0001F680");

		auto parseAndCheck = [&](const void *data, size_t size) {
			VDJSONReader reader;
			VDJSONDocument document;
			return reader.Parse(data, size, document)
				&& document.Root()["text"].IsString()
				&& VDStringW(document.Root()["text"].AsString()) == expected;
		};

		AT_PORTABLE_TEST_ASSERT(context, parseAndCheck(kEscapedJSON, std::size(kEscapedJSON) - 1));

		std::vector<uint8> utf16LE;
		std::vector<uint8> utf32LE;
		for(const uint8 c : std::string_view(kEscapedJSON, std::size(kEscapedJSON) - 1)) {
			utf16LE.push_back(c);
			utf16LE.push_back(0);

			utf32LE.push_back(c);
			utf32LE.push_back(0);
			utf32LE.push_back(0);
			utf32LE.push_back(0);
		}

		AT_PORTABLE_TEST_ASSERT(context, parseAndCheck(utf16LE.data(), utf16LE.size()));
		AT_PORTABLE_TEST_ASSERT(context, parseAndCheck(utf32LE.data(), utf32LE.size()));

		static constexpr char kInvalidEscape[] = "{\"text\":\"\\q\"}";
		VDJSONReader invalidReader;
		VDJSONDocument invalidDocument;
		AT_PORTABLE_TEST_ASSERT(context, !invalidReader.Parse(kInvalidEscape, std::size(kInvalidEscape) - 1, invalidDocument));
		return true;
	}

	bool TestJSONRoundTrip(ATPortableTestContext& context) {
		vdrefptr root { new PortableSaveStateRoot };
		root->mpChild = new PortableSaveStateChild;
		root->mpChild->mNumber = 0x89ABCDEF;
		root->mpChild->mSignedNumber = -INT64_C(0x123456789);
		root->mpChild->mRatio = 1.25;
		root->mpChild->mbEnabled = true;
		root->mpChild->mLabel = "referenced";
		root->mpSameChild = root->mpChild;

		root->mpInlineChild = new PortableSaveStateChild;
		root->mpInlineChild->mNumber = 42;
		root->mpInlineChild->mSignedNumber = -INT64_C(0x123456789);
		root->mpInlineChild->mRatio = 1.25;
		root->mpInlineChild->mbEnabled = true;
		root->mpInlineChild->mLabel = "inline";
		root->mNumbers[0] = 1;
		root->mNumbers[1] = 256;
		root->mNumbers[2] = 32768;
		root->mNumbers[3] = 65535;
		root->mWideLabel = L"Za\u017c\u00f3\u0142\u0107 g\u0119\u015bl\u0105 ja\u017a\u0144 \u07FF \U0001F680 \"\\\f\x01";
		root->mLargeUnsigned = UINT64_C(0xFEDCBA9876543210);

		VDMemoryBufferStream stream;
		std::unique_ptr<IATSaveStateSerializer> serializer(ATCreateSaveStateSerializer());
		serializer->Serialize(stream, *root, L"portable-test");

		const vdspan<const uint8> json = stream.GetBuffer();
		AT_PORTABLE_TEST_ASSERT(context, json.size() > 100);
		const std::string_view jsonText(reinterpret_cast<const char *>(json.data()), json.size());
		AT_PORTABLE_TEST_ASSERT(context, jsonText.find("PortableSaveStateRoot") != std::string_view::npos);

		stream.Seek(0);
		vdrefptr<IATSerializable> restoredBase;
		std::unique_ptr<IATSaveStateDeserializer> deserializer(ATCreateSaveStateDeserializer());
		deserializer->Deserialize(stream, ~restoredBase);

		auto *restored = atser_cast<PortableSaveStateRoot *>(restoredBase);
		AT_PORTABLE_TEST_ASSERT(context, restored != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, VerifyChild(restored->mpChild, 0x89ABCDEF, "referenced"));
		AT_PORTABLE_TEST_ASSERT(context, restored->mpChild == restored->mpSameChild);
		AT_PORTABLE_TEST_ASSERT(context, VerifyChild(restored->mpInlineChild, 42, "inline"));
		AT_PORTABLE_TEST_ASSERT(context, restored->mNumbers[0] == 1);
		AT_PORTABLE_TEST_ASSERT(context, restored->mNumbers[1] == 256);
		AT_PORTABLE_TEST_ASSERT(context, restored->mNumbers[2] == 32768);
		AT_PORTABLE_TEST_ASSERT(context, restored->mNumbers[3] == 65535);
		AT_PORTABLE_TEST_ASSERT(context, restored->mWideLabel == root->mWideLabel);
		AT_PORTABLE_TEST_ASSERT(context, restored->mLargeUnsigned == root->mLargeUnsigned);
		return true;
	}

	bool TestZIPRoundTrip(ATPortableTestContext& context) {
		static constexpr uint8 kFirstData[] = { 0, 1, 2, 3, 4, 0xFE, 0xFF };
		static constexpr uint8 kSecondData[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1 };

		vdrefptr root { new PortableSaveStatePackage };
		root->mpFirst = new ATSaveStateMemoryBuffer;
		root->mpSecond = new ATSaveStateMemoryBuffer;
		root->mpFirst->mpDirectName = L"payload.bin";
		root->mpSecond->mpDirectName = L"payload.bin";
		root->mpFirst->GetWriteBuffer().assign(std::begin(kFirstData), std::end(kFirstData));
		root->mpSecond->GetWriteBuffer().assign(std::begin(kSecondData), std::end(kSecondData));

		VDMemoryBufferStream stream;
		std::unique_ptr<IVDZipArchiveWriter> zipWriter(VDCreateZipArchiveWriter(stream));
		std::unique_ptr<IATSaveStateSerializer> serializer(ATCreateSaveStateSerializer(L"state.json"));
		int serializeProgress = 0;
		serializer->SetCompressionLevel(VDDeflateCompressionLevel::Store);
		serializer->SetProgressFn([&](int, int) { ++serializeProgress; });
		serializer->BeginSerialize(*zipWriter);
		serializer->PreSerializeDirect(*root->mpFirst);
		serializer->EndSerialize(*root);
		zipWriter->Finalize();
		zipWriter.reset();

		AT_PORTABLE_TEST_ASSERT(context, root->mpFirst->GetReadBuffer().empty());
		AT_PORTABLE_TEST_ASSERT(context, serializeProgress == 1);

		stream.Seek(0);
		VDZipArchive archive;
		archive.Init(&stream);
		AT_PORTABLE_TEST_ASSERT(context, archive.GetFileCount() == 3);
		AT_PORTABLE_TEST_ASSERT(context, archive.FindFile("state.json") >= 0);
		AT_PORTABLE_TEST_ASSERT(context, archive.FindFile("payload.bin") >= 0);
		AT_PORTABLE_TEST_ASSERT(context, archive.FindFile("payload-2.bin") >= 0);

		vdrefptr<IATSerializable> restoredBase;
		std::unique_ptr<IATSaveStateDeserializer> deserializer(ATCreateSaveStateDeserializer(L"state.json"));
		int deserializeProgress = 0;
		deserializer->SetProgressFn([&](int, int) { ++deserializeProgress; });
		vdrefptr<IVDRefCount> archiveCookie { new vdrefcounted<IVDRefCount> };
		deserializer->Deserialize(archive, ~restoredBase, archiveCookie);

		auto *restored = atser_cast<PortableSaveStatePackage *>(restoredBase);
		AT_PORTABLE_TEST_ASSERT(context, restored != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, deserializeProgress == 3);
		AT_PORTABLE_TEST_ASSERT(context, restored->mpFirst != nullptr);
		AT_PORTABLE_TEST_ASSERT(context, restored->mpSecond != nullptr);

		restored->mpFirst->PrefetchReadBuffer();
		const auto& first = restored->mpFirst->GetReadBuffer();
		const auto& second = restored->mpSecond->GetReadBuffer();
		AT_PORTABLE_TEST_ASSERT(context, first.size() == std::size(kFirstData));
		AT_PORTABLE_TEST_ASSERT(context, second.size() == std::size(kSecondData));
		AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(first.data(), kFirstData, std::size(kFirstData)));
		AT_PORTABLE_TEST_ASSERT(context, !std::memcmp(second.data(), kSecondData, std::size(kSecondData)));
		return true;
	}
}

bool ATTestAltirraSaveStateIO(ATPortableTestContext& context) {
	return TestJSONUnicodeInput(context)
		&& TestJSONRoundTrip(context)
		&& TestZIPRoundTrip(context);
}
