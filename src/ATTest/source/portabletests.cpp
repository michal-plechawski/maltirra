// Altirra portable test manifest

#include <at/attest/portabletest.h>

bool ATTestAltirraArtifactingFilters(ATPortableTestContext& context);
bool ATTestAltirraConstants(ATPortableTestContext& context);
bool ATTestAltirraErrorDecode(ATPortableTestContext& context);
bool ATTestAltirraInputDefs(ATPortableTestContext& context);
bool ATTestAltirraPNG(ATPortableTestContext& context);
bool ATTestAltirraTrace(ATPortableTestContext& context);
bool ATTestAltirraTraceTape(ATPortableTestContext& context);
bool ATTestAltirraSimEventManager(ATPortableTestContext& context);
bool ATTestAltirraVideoManager(ATPortableTestContext& context);
bool ATTestCoreChecksum(ATPortableTestContext& context);
bool ATTestCoreFFT(ATPortableTestContext& context);
bool ATTestCoreTimerService(ATPortableTestContext& context);
bool ATTestIOCartridgeImage(ATPortableTestContext& context);
bool ATTestIOCassetteAudioFilters(ATPortableTestContext& context);
bool ATTestIOCassetteDecoder(ATPortableTestContext& context);
bool ATTestIOFLACAccel(ATPortableTestContext& context);
bool ATTestKasumiResolve4x(ATPortableTestContext& context);
bool ATTestKasumiResampleStages(ATPortableTestContext& context);
bool ATTestKasumiYCbCr709(ATPortableTestContext& context);
bool ATTestKasumiYUVReference(ATPortableTestContext& context);
bool ATTestSystemBinary(ATPortableTestContext& context);
bool ATTestSystemBitMath(ATPortableTestContext& context);
bool ATTestSystemCache(ATPortableTestContext& context);
bool ATTestSystemCommandLine(ATPortableTestContext& context);
bool ATTestSystemConstexpr(ATPortableTestContext& context);
bool ATTestSystemCPUAccel(ATPortableTestContext& context);
bool ATTestSystemDate(ATPortableTestContext& context);
bool ATTestSystemDebug(ATPortableTestContext& context);
bool ATTestSystemError(ATPortableTestContext& context);
bool ATTestSystemEvent(ATPortableTestContext& context);
bool ATTestSystemFile(ATPortableTestContext& context);
bool ATTestSystemFileAsync(ATPortableTestContext& context);
bool ATTestSystemFileStream(ATPortableTestContext& context);
bool ATTestSystemFileWatcher(ATPortableTestContext& context);
bool ATTestSystemFileSys(ATPortableTestContext& context);
bool ATTestSystemFraction(ATPortableTestContext& context);
bool ATTestSystemHash(ATPortableTestContext& context);
bool ATTestSystemHalfFloat(ATPortableTestContext& context);
bool ATTestSystemInt128(ATPortableTestContext& context);
bool ATTestSystemLinearAlloc(ATPortableTestContext& context);
bool ATTestSystemMath(ATPortableTestContext& context);
bool ATTestSystemMemory(ATPortableTestContext& context);
bool ATTestSystemProcess(ATPortableTestContext& context);
bool ATTestSystemRegistry(ATPortableTestContext& context);
bool ATTestSystemRegistryMemory(ATPortableTestContext& context);
bool ATTestSystemRefCount(ATPortableTestContext& context);
bool ATTestSystemStrUtil(ATPortableTestContext& context);
bool ATTestSystemText(ATPortableTestContext& context);
bool ATTestSystemThread(ATPortableTestContext& context);
bool ATTestSystemThunk(ATPortableTestContext& context);
bool ATTestSystemTime(ATPortableTestContext& context);
bool ATTestSystemTLS(ATPortableTestContext& context);
bool ATTestSystemVDAlloc(ATPortableTestContext& context);
bool ATTestSystemVDFunction(ATPortableTestContext& context);
bool ATTestSystemVDString(ATPortableTestContext& context);
bool ATTestSystemVDSTL(ATPortableTestContext& context);
bool ATTestSystemVDSTLHash(ATPortableTestContext& context);
bool ATTestSystemVDSTLHashTable(ATPortableTestContext& context);
bool ATTestSystemVectors(ATPortableTestContext& context);
bool ATTestSystemZip(ATPortableTestContext& context);

const ATPortableTestCase *ATGetPortableTests(size_t& count) {
	static const ATPortableTestCase kTests[] = {
		{ "Altirra_ArtifactingFilters", ATTestAltirraArtifactingFilters },
		{ "Altirra_Constants", ATTestAltirraConstants },
		{ "Altirra_ErrorDecode", ATTestAltirraErrorDecode },
		{ "Altirra_InputDefs", ATTestAltirraInputDefs },
		{ "Altirra_PNG", ATTestAltirraPNG },
		{ "Altirra_Trace", ATTestAltirraTrace },
		{ "Altirra_TraceTape", ATTestAltirraTraceTape },
		{ "Altirra_SimEventManager", ATTestAltirraSimEventManager },
		{ "Altirra_VideoManager", ATTestAltirraVideoManager },
		{ "Core_Checksum", ATTestCoreChecksum },
		{ "Core_FFT", ATTestCoreFFT },
		{ "Core_TimerService", ATTestCoreTimerService },
		{ "IO_CartridgeImage", ATTestIOCartridgeImage },
		{ "IO_CassetteAudioFilters", ATTestIOCassetteAudioFilters },
		{ "IO_CassetteDecoder", ATTestIOCassetteDecoder },
		{ "IO_FLACAccel", ATTestIOFLACAccel },
		{ "Kasumi_Resolve4x", ATTestKasumiResolve4x },
		{ "Kasumi_ResampleStages", ATTestKasumiResampleStages },
		{ "Kasumi_YCbCr709", ATTestKasumiYCbCr709 },
		{ "Kasumi_YUVReference", ATTestKasumiYUVReference },
		{ "System_Binary", ATTestSystemBinary },
		{ "System_BitMath", ATTestSystemBitMath },
		{ "System_Cache", ATTestSystemCache },
		{ "System_CommandLine", ATTestSystemCommandLine },
		{ "System_Constexpr", ATTestSystemConstexpr },
		{ "System_CPUAccel", ATTestSystemCPUAccel },
		{ "System_Date", ATTestSystemDate },
		{ "System_Debug", ATTestSystemDebug },
		{ "System_Error", ATTestSystemError },
		{ "System_Event", ATTestSystemEvent },
		{ "System_File", ATTestSystemFile },
		{ "System_FileAsync", ATTestSystemFileAsync },
		{ "System_FileStream", ATTestSystemFileStream },
		{ "System_FileWatcher", ATTestSystemFileWatcher },
		{ "System_FileSys", ATTestSystemFileSys },
		{ "System_Fraction", ATTestSystemFraction },
		{ "System_Hash", ATTestSystemHash },
		{ "System_HalfFloat", ATTestSystemHalfFloat },
		{ "System_Int128", ATTestSystemInt128 },
		{ "System_LinearAlloc", ATTestSystemLinearAlloc },
		{ "System_Math", ATTestSystemMath },
		{ "System_Memory", ATTestSystemMemory },
		{ "System_Process", ATTestSystemProcess },
		{ "System_Registry", ATTestSystemRegistry },
		{ "System_RegistryMemory", ATTestSystemRegistryMemory },
		{ "System_RefCount", ATTestSystemRefCount },
		{ "System_StrUtil", ATTestSystemStrUtil },
		{ "System_Text", ATTestSystemText },
		{ "System_Thread", ATTestSystemThread },
		{ "System_Thunk", ATTestSystemThunk },
		{ "System_Time", ATTestSystemTime },
		{ "System_TLS", ATTestSystemTLS },
		{ "System_VDAlloc", ATTestSystemVDAlloc },
		{ "System_VDFunction", ATTestSystemVDFunction },
		{ "System_VDString", ATTestSystemVDString },
		{ "System_VDSTL", ATTestSystemVDSTL },
		{ "System_VDSTLHash", ATTestSystemVDSTLHash },
		{ "System_VDSTLHashTable", ATTestSystemVDSTLHashTable },
		{ "System_Vectors", ATTestSystemVectors },
		{ "System_Zip", ATTestSystemZip },
	};

	count = sizeof kTests / sizeof kTests[0];
	return kTests;
}
