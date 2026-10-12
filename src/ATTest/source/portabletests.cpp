// Altirra portable test manifest

#include <at/attest/portabletest.h>

bool ATTestAltirraArtifactingFilters(ATPortableTestContext& context);
bool ATTestAltirraArtifactingParams(ATPortableTestContext& context);
bool ATTestAltirraArtifactingEngine(ATPortableTestContext& context);
bool ATTestAltirraConsoleText(ATPortableTestContext& context);
bool ATTestAltirraArtifactingColorCorrect(ATPortableTestContext& context);
bool ATTestAltirraArtifactingNEON(ATPortableTestContext& context);
bool ATTestAltirraArtifactingNTSCNEON(ATPortableTestContext& context);
bool ATTestAltirraArtifactingPALNEON(ATPortableTestContext& context);
bool ATTestAltirraArtifactingPALScalar(ATPortableTestContext& context);
bool ATTestAltirraAudioRawSource(ATPortableTestContext& context);
bool ATTestAltirraAudioStockSamples(ATPortableTestContext& context);
bool ATTestAltirraBlockDevDiskAdapter(ATPortableTestContext& context);
bool ATTestAltirraCallback(ATPortableTestContext& context);
bool ATTestAltirraCartridgePort(ATPortableTestContext& context);
bool ATTestAltirraCheatEngine(ATPortableTestContext& context);
bool ATTestAltirraConstants(ATPortableTestContext& context);
bool ATTestAltirraCompatDB(ATPortableTestContext& context);
bool ATTestAltirraCPUMemory(ATPortableTestContext& context);
bool ATTestAltirraCPUStates(ATPortableTestContext& context);
bool ATTestAltirraMemoryManager(ATPortableTestContext& context);
bool ATTestAltirraRTime8(ATPortableTestContext& context);
bool ATTestAltirraRVerter(ATPortableTestContext& context);
bool ATTestAltirraSCSIDisk(ATPortableTestContext& context);
bool ATTestAltirraThePill(ATPortableTestContext& context);
bool ATTestAltirraDebuggerSettings(ATPortableTestContext& context);
bool ATTestAltirraDiskTrace(ATPortableTestContext& context);
bool ATTestAltirraDiskProfile(ATPortableTestContext& context);
bool ATTestAltirraDiskVirtImageBase(ATPortableTestContext& context);
bool ATTestAltirraDiskVirtImage(ATPortableTestContext& context);
bool ATTestAltirraDiskVirtImageSDFS(ATPortableTestContext& context);
bool ATTestAltirraDirectoryWatcher(ATPortableTestContext& context);
bool ATTestAltirraErrorDecode(ATPortableTestContext& context);
bool ATTestAltirraFirmwareDetect(ATPortableTestContext& context);
bool ATTestAltirraInputDefs(ATPortableTestContext& context);
bool ATTestAltirraInputMap(ATPortableTestContext& context);
bool ATTestAltirraCPUHookManager(ATPortableTestContext& context);
bool ATTestAltirraIRQController(ATPortableTestContext& context);
bool ATTestAltirraHLEUtils(ATPortableTestContext& context);
bool ATTestAltirraGTIATables(ATPortableTestContext& context);
bool ATTestAltirraGTIARenderer(ATPortableTestContext& context);
bool ATTestAltirraHostDeviceUtils(ATPortableTestContext& context);
bool ATTestAltirraIDERawImage(ATPortableTestContext& context);
bool ATTestAltirraIDEVHDImage(ATPortableTestContext& context);
bool ATTestAltirraKernelDB(ATPortableTestContext& context);
bool ATTestAltirraOSTracing(ATPortableTestContext& context);
bool ATTestAltirraOSHelper(ATPortableTestContext& context);
bool ATTestAltirraPNG(ATPortableTestContext& context);
bool ATTestAltirraPNGEncoder(ATPortableTestContext& context);
bool ATTestAltirraPBI(ATPortableTestContext& context);
bool ATTestAltirraPCLinkError(ATPortableTestContext& context);
bool ATTestAltirraPaletteGenerator(ATPortableTestContext& context);
bool ATTestAltirraPaletteSolver(ATPortableTestContext& context);
bool ATTestAltirraSaveStateTypes(ATPortableTestContext& context);
bool ATTestAltirraSaveStateReader(ATPortableTestContext& context);
bool ATTestAltirraSaveStateIO(ATPortableTestContext& context);
bool ATTestAltirraPrinter1020Font(ATPortableTestContext& context);
bool ATTestAltirraPrinter1020(ATPortableTestContext& context);
bool ATTestAltirraPrinterBase(ATPortableTestContext& context);
bool ATTestAltirraPrinterFonts(ATPortableTestContext& context);
bool ATTestAltirraPrinterOutput(ATPortableTestContext& context);
bool ATTestAltirraPrinters(ATPortableTestContext& context);
bool ATTestAltirraPrinterTTFEncoder(ATPortableTestContext& context);
bool ATTestAltirraSAPConverter(ATPortableTestContext& context);
bool ATTestAltirraPokeyTrace(ATPortableTestContext& context);
bool ATTestAltirraPokeySaveCompat(ATPortableTestContext& context);
bool ATTestAltirraTrace(ATPortableTestContext& context);
bool ATTestAltirraTraceCPU(ATPortableTestContext& context);
bool ATTestAltirraTraceVideo(ATPortableTestContext& context);
bool ATTestAltirraTraceFileEncoding(ATPortableTestContext& context);
bool ATTestAltirraTraceFileFormat(ATPortableTestContext& context);
bool ATTestAltirraTraceTape(ATPortableTestContext& context);
bool ATTestAltirraUpdateFeed(ATPortableTestContext& context);
bool ATTestAltirraUIAccessors(ATPortableTestContext& context);
bool ATTestAltirraUIFileFilters(ATPortableTestContext& context);
bool ATTestAltirraSimEventManager(ATPortableTestContext& context);
bool ATTestAltirraStartupLogger(ATPortableTestContext& context);
bool ATTestAltirraTextDOM(ATPortableTestContext& context);
bool ATTestAltirraVideoStillImage(ATPortableTestContext& context);
bool ATTestAltirraVideoManager(ATPortableTestContext& context);
bool ATTestAltirraVersionInfo(ATPortableTestContext& context);
bool ATTestAltirraVBXEState(ATPortableTestContext& context);
bool ATTestAltirraVBXEMemory(ATPortableTestContext& context);
bool ATTestAltirraVBXEBlitter(ATPortableTestContext& context);
bool ATTestAltirraVBXEVideo(ATPortableTestContext& context);
bool ATTestAltirraVBXEDevice(ATPortableTestContext& context);
bool ATTestAltirraGTIARegisters(ATPortableTestContext& context);
bool ATTestAltirraGTIASnapshot(ATPortableTestContext& context);
bool ATTestAltirraGTIAPlayfield(ATPortableTestContext& context);
bool ATTestAltirraANTICRegisters(ATPortableTestContext& context);
bool ATTestAltirraANTICTiming(ATPortableTestContext& context);
bool ATTestAltirraANTICSnapshot(ATPortableTestContext& context);
bool ATTestAltirraANTICKernels(ATPortableTestContext& context);
bool ATTestAltirraPIAInputs(ATPortableTestContext& context);
bool ATTestAltirraProfileBuilder(ATPortableTestContext& context);
bool ATTestAltirraCPUHeatMapMemory(ATPortableTestContext& context);
bool ATTestAltirraCPUHeatMapPropagation(ATPortableTestContext& context);
bool ATTestAltirraCPUHeatMapStack(ATPortableTestContext& context);
bool ATTestAltirraCPUHeatMapTraps(ATPortableTestContext& context);
bool ATTestAltirraCPUHeatMapDiagnostics(ATPortableTestContext& context);
bool ATTestAltirraProfileCounters(ATPortableTestContext& context);
bool ATTestAltirraProfileFrames(ATPortableTestContext& context);
bool ATTestAltirraProfileCallGraph(ATPortableTestContext& context);
bool ATTestAltirraPIARegisters(ATPortableTestContext& context);
bool ATTestAltirraPIAIRQ(ATPortableTestContext& context);
bool ATTestAltirraPIAFloating(ATPortableTestContext& context);
bool ATTestAltirraPIASnapshot(ATPortableTestContext& context);
bool ATTestAltirraPIACountedInputs(ATPortableTestContext& context);
bool ATTestAudioPokeyRegisters(ATPortableTestContext& context);
bool ATTestAudioPokeyTimers(ATPortableTestContext& context);
bool ATTestAudioFilters(ATPortableTestContext& context);
bool ATTestAudioEdgePlayer(ATPortableTestContext& context);
bool ATTestAudioNativeOutput(ATPortableTestContext& context);
bool ATTestAudioNativeBackend(ATPortableTestContext& context);
bool ATTestAudioOutput(ATPortableTestContext& context);
bool ATTestAudioRingBuffer(ATPortableTestContext& context);
bool ATTestCoreChecksum(ATPortableTestContext& context);
bool ATTestCoreFFT(ATPortableTestContext& context);
bool ATTestCoreTimerService(ATPortableTestContext& context);
bool ATTestIOCartridgeImage(ATPortableTestContext& context);
bool ATTestIOCassetteAudioFilters(ATPortableTestContext& context);
bool ATTestIOCassetteDecoder(ATPortableTestContext& context);
bool ATTestIOFLACAccel(ATPortableTestContext& context);
bool ATTestIOVorbis(ATPortableTestContext& context);
bool ATTestKasumiResolve4x(ATPortableTestContext& context);
bool ATTestKasumiResampleStages(ATPortableTestContext& context);
bool ATTestKasumiYCbCr709(ATPortableTestContext& context);
bool ATTestKasumiYUVReference(ATPortableTestContext& context);
bool ATTestNetNativeAddress(ATPortableTestContext& context);
bool ATTestNetLookupWorker(ATPortableTestContext& context);
bool ATTestNetNativeSockets(ATPortableTestContext& context);
bool ATTestNetSocketWorker(ATPortableTestContext& context);
bool ATTestNetVxlanTunnel(ATPortableTestContext& context);
bool ATTestTessaConfig(ATPortableTestContext& context);
bool ATTestTessaContext(ATPortableTestContext& context);
bool ATTestTessaFormat(ATPortableTestContext& context);
bool ATTestTessaProgram(ATPortableTestContext& context);
bool ATTestVDDisplayBicubic(ATPortableTestContext& context);
bool ATTestVDDisplayBloom(ATPortableTestContext& context);
bool ATTestVDDisplayCustomEffect(ATPortableTestContext& context);
bool ATTestVDDisplayDisplayTypes(ATPortableTestContext& context);
bool ATTestVDDisplayFontBitmap(ATPortableTestContext& context);
bool ATTestVDDisplayFrameQueue(ATPortableTestContext& context);
bool ATTestVDDisplayPresentationBuffer(ATPortableTestContext& context);
bool ATTestVDDisplayLogging(ATPortableTestContext& context);
bool ATTestVDDisplayRenderCache(ATPortableTestContext& context);
bool ATTestVDDisplayRenderer(ATPortableTestContext& context);
bool ATTestVDDisplayRendererSoft(ATPortableTestContext& context);
bool ATTestVDDisplayScreenFX(ATPortableTestContext& context);
bool ATTestVDDisplaySoftwareComposition(ATPortableTestContext& context);
bool ATTestVDDisplayTextRenderer(ATPortableTestContext& context);
#if defined(__APPLE__)
bool ATTestVDDisplayViewMac(ATPortableTestContext& context);
bool ATTestVDDisplayAdapterMac(ATPortableTestContext& context);
bool ATTestVDDisplayCompositionMac(ATPortableTestContext& context);
bool ATTestVDDisplayNativeViewMac(ATPortableTestContext& context);
bool ATTestAltirraGTIADisplayMac(ATPortableTestContext& context);
bool ATTestAltirraGTIAFrameMac(ATPortableTestContext& context);
bool ATTestAltirraANTICFrameMac(ATPortableTestContext& context);
bool ATTestAltirraConsoleMac(ATPortableTestContext& context);
bool ATTestNativeUIWindowProxyMac(ATPortableTestContext& context);
bool ATTestNativeUIWindowEventsMac(ATPortableTestContext& context);
#endif
bool ATTestNativeUIWindowProxy(ATPortableTestContext& context);
bool ATTestNativeUIWindowLifecycle(ATPortableTestContext& context);
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
		{ "Altirra_ArtifactingParams", ATTestAltirraArtifactingParams },
		{ "Altirra_ArtifactingEngine", ATTestAltirraArtifactingEngine },
		{ "Altirra_ConsoleText", ATTestAltirraConsoleText },
		{ "Altirra_ArtifactingColorCorrect", ATTestAltirraArtifactingColorCorrect },
		{ "Altirra_ArtifactingNEON", ATTestAltirraArtifactingNEON },
		{ "Altirra_ArtifactingNTSCNEON", ATTestAltirraArtifactingNTSCNEON },
		{ "Altirra_ArtifactingPALNEON", ATTestAltirraArtifactingPALNEON },
		{ "Altirra_ArtifactingPALScalar", ATTestAltirraArtifactingPALScalar },
		{ "Altirra_AudioRawSource", ATTestAltirraAudioRawSource },
		{ "Altirra_AudioStockSamples", ATTestAltirraAudioStockSamples },
		{ "Altirra_BlockDevDiskAdapter", ATTestAltirraBlockDevDiskAdapter },
		{ "Altirra_Callback", ATTestAltirraCallback },
		{ "Altirra_CartridgePort", ATTestAltirraCartridgePort },
		{ "Altirra_CheatEngine", ATTestAltirraCheatEngine },
		{ "Altirra_Constants", ATTestAltirraConstants },
		{ "Altirra_CompatDB", ATTestAltirraCompatDB },
		{ "Altirra_CPUMemory", ATTestAltirraCPUMemory },
		{ "Altirra_CPUStates", ATTestAltirraCPUStates },
		{ "Altirra_MemoryManager", ATTestAltirraMemoryManager },
		{ "Altirra_RTime8", ATTestAltirraRTime8 },
		{ "Altirra_RVerter", ATTestAltirraRVerter },
		{ "Altirra_SCSIDisk", ATTestAltirraSCSIDisk },
		{ "Altirra_ThePill", ATTestAltirraThePill },
		{ "Altirra_DebuggerSettings", ATTestAltirraDebuggerSettings },
		{ "Altirra_DiskTrace", ATTestAltirraDiskTrace },
		{ "Altirra_DiskProfile", ATTestAltirraDiskProfile },
		{ "Altirra_DiskVirtImageBase", ATTestAltirraDiskVirtImageBase },
		{ "Altirra_DiskVirtImage", ATTestAltirraDiskVirtImage },
		{ "Altirra_DiskVirtImageSDFS", ATTestAltirraDiskVirtImageSDFS },
		{ "Altirra_DirectoryWatcher", ATTestAltirraDirectoryWatcher },
		{ "Altirra_ErrorDecode", ATTestAltirraErrorDecode },
		{ "Altirra_FirmwareDetect", ATTestAltirraFirmwareDetect },
		{ "Altirra_InputDefs", ATTestAltirraInputDefs },
		{ "Altirra_InputMap", ATTestAltirraInputMap },
		{ "Altirra_CPUHookManager", ATTestAltirraCPUHookManager },
		{ "Altirra_IRQController", ATTestAltirraIRQController },
		{ "Altirra_HLEUtils", ATTestAltirraHLEUtils },
		{ "Altirra_GTIATables", ATTestAltirraGTIATables },
		{ "Altirra_GTIARenderer", ATTestAltirraGTIARenderer },
		{ "Altirra_HostDeviceUtils", ATTestAltirraHostDeviceUtils },
		{ "Altirra_IDERawImage", ATTestAltirraIDERawImage },
		{ "Altirra_IDEVHDImage", ATTestAltirraIDEVHDImage },
		{ "Altirra_KernelDB", ATTestAltirraKernelDB },
		{ "Altirra_OSTracing", ATTestAltirraOSTracing },
		{ "Altirra_OSHelper", ATTestAltirraOSHelper },
		{ "Altirra_PNG", ATTestAltirraPNG },
		{ "Altirra_PNGEncoder", ATTestAltirraPNGEncoder },
		{ "Altirra_PBI", ATTestAltirraPBI },
		{ "Altirra_PCLinkError", ATTestAltirraPCLinkError },
		{ "Altirra_PaletteGenerator", ATTestAltirraPaletteGenerator },
		{ "Altirra_PaletteSolver", ATTestAltirraPaletteSolver },
		{ "Altirra_SaveStateTypes", ATTestAltirraSaveStateTypes },
		{ "Altirra_SaveStateReader", ATTestAltirraSaveStateReader },
		{ "Altirra_SaveStateIO", ATTestAltirraSaveStateIO },
		{ "Altirra_Printer1020Font", ATTestAltirraPrinter1020Font },
		{ "Altirra_Printer1020", ATTestAltirraPrinter1020 },
		{ "Altirra_PrinterBase", ATTestAltirraPrinterBase },
		{ "Altirra_PrinterFonts", ATTestAltirraPrinterFonts },
		{ "Altirra_PrinterOutput", ATTestAltirraPrinterOutput },
		{ "Altirra_Printers", ATTestAltirraPrinters },
		{ "Altirra_PrinterTTFEncoder", ATTestAltirraPrinterTTFEncoder },
		{ "Altirra_SAPConverter", ATTestAltirraSAPConverter },
		{ "Altirra_PokeyTrace", ATTestAltirraPokeyTrace },
		{ "Altirra_PokeySaveCompat", ATTestAltirraPokeySaveCompat },
		{ "Altirra_Trace", ATTestAltirraTrace },
		{ "Altirra_TraceCPU", ATTestAltirraTraceCPU },
		{ "Altirra_TraceVideo", ATTestAltirraTraceVideo },
		{ "Altirra_TraceFileEncoding", ATTestAltirraTraceFileEncoding },
		{ "Altirra_TraceFileFormat", ATTestAltirraTraceFileFormat },
		{ "Altirra_TraceTape", ATTestAltirraTraceTape },
		{ "Altirra_UpdateFeed", ATTestAltirraUpdateFeed },
		{ "Altirra_UIAccessors", ATTestAltirraUIAccessors },
		{ "Altirra_UIFileFilters", ATTestAltirraUIFileFilters },
		{ "Altirra_SimEventManager", ATTestAltirraSimEventManager },
		{ "Altirra_StartupLogger", ATTestAltirraStartupLogger },
		{ "Altirra_TextDOM", ATTestAltirraTextDOM },
		{ "Altirra_VideoStillImage", ATTestAltirraVideoStillImage },
		{ "Altirra_VideoManager", ATTestAltirraVideoManager },
		{ "Altirra_VersionInfo", ATTestAltirraVersionInfo },
		{ "Altirra_VBXEState", ATTestAltirraVBXEState },
		{ "Altirra_VBXEMemory", ATTestAltirraVBXEMemory },
		{ "Altirra_VBXEBlitter", ATTestAltirraVBXEBlitter },
		{ "Altirra_VBXEVideo", ATTestAltirraVBXEVideo },
		{ "Altirra_VBXEDevice", ATTestAltirraVBXEDevice },
		{ "Altirra_GTIARegisters", ATTestAltirraGTIARegisters },
		{ "Altirra_GTIASnapshot", ATTestAltirraGTIASnapshot },
		{ "Altirra_GTIAPlayfield", ATTestAltirraGTIAPlayfield },
		{ "Altirra_ANTICRegisters", ATTestAltirraANTICRegisters },
		{ "Altirra_ANTICTiming", ATTestAltirraANTICTiming },
		{ "Altirra_ANTICSnapshot", ATTestAltirraANTICSnapshot },
		{ "Altirra_ANTICKernels", ATTestAltirraANTICKernels },
		{ "Altirra_PIAInputs", ATTestAltirraPIAInputs },
		{ "Altirra_ProfileBuilder", ATTestAltirraProfileBuilder },
		{ "Altirra_CPUHeatMapMemory", ATTestAltirraCPUHeatMapMemory },
		{ "Altirra_CPUHeatMapPropagation", ATTestAltirraCPUHeatMapPropagation },
		{ "Altirra_CPUHeatMapStack", ATTestAltirraCPUHeatMapStack },
		{ "Altirra_CPUHeatMapTraps", ATTestAltirraCPUHeatMapTraps },
		{ "Altirra_CPUHeatMapDiagnostics", ATTestAltirraCPUHeatMapDiagnostics },
		{ "Altirra_ProfileCounters", ATTestAltirraProfileCounters },
		{ "Altirra_ProfileFrames", ATTestAltirraProfileFrames },
		{ "Altirra_ProfileCallGraph", ATTestAltirraProfileCallGraph },
		{ "Altirra_PIARegisters", ATTestAltirraPIARegisters },
		{ "Altirra_PIAIRQ", ATTestAltirraPIAIRQ },
		{ "Altirra_PIAFloating", ATTestAltirraPIAFloating },
		{ "Altirra_PIASnapshot", ATTestAltirraPIASnapshot },
		{ "Altirra_PIACountedInputs", ATTestAltirraPIACountedInputs },
		{ "Audio_PokeyRegisters", ATTestAudioPokeyRegisters },
		{ "Audio_PokeyTimers", ATTestAudioPokeyTimers },
		{ "Audio_Filters", ATTestAudioFilters },
		{ "Audio_EdgePlayer", ATTestAudioEdgePlayer },
		{ "Audio_NativeOutput", ATTestAudioNativeOutput },
		{ "Audio_NativeBackend", ATTestAudioNativeBackend },
		{ "Audio_Output", ATTestAudioOutput },
		{ "Audio_RingBuffer", ATTestAudioRingBuffer },
		{ "Core_Checksum", ATTestCoreChecksum },
		{ "Core_FFT", ATTestCoreFFT },
		{ "Core_TimerService", ATTestCoreTimerService },
		{ "IO_CartridgeImage", ATTestIOCartridgeImage },
		{ "IO_CassetteAudioFilters", ATTestIOCassetteAudioFilters },
		{ "IO_CassetteDecoder", ATTestIOCassetteDecoder },
		{ "IO_FLACAccel", ATTestIOFLACAccel },
		{ "IO_Vorbis", ATTestIOVorbis },
		{ "Kasumi_Resolve4x", ATTestKasumiResolve4x },
		{ "Kasumi_ResampleStages", ATTestKasumiResampleStages },
		{ "Kasumi_YCbCr709", ATTestKasumiYCbCr709 },
		{ "Kasumi_YUVReference", ATTestKasumiYUVReference },
		{ "Net_NativeAddress", ATTestNetNativeAddress },
		{ "Net_LookupWorker", ATTestNetLookupWorker },
		{ "Net_NativeSockets", ATTestNetNativeSockets },
		{ "Net_SocketWorker", ATTestNetSocketWorker },
		{ "Net_VxlanTunnel", ATTestNetVxlanTunnel },
		{ "Tessa_Config", ATTestTessaConfig },
		{ "Tessa_Context", ATTestTessaContext },
		{ "Tessa_Format", ATTestTessaFormat },
		{ "Tessa_Program", ATTestTessaProgram },
		{ "VDDisplay_Bicubic", ATTestVDDisplayBicubic },
		{ "VDDisplay_Bloom", ATTestVDDisplayBloom },
		{ "VDDisplay_CustomEffect", ATTestVDDisplayCustomEffect },
		{ "VDDisplay_DisplayTypes", ATTestVDDisplayDisplayTypes },
		{ "VDDisplay_FontBitmap", ATTestVDDisplayFontBitmap },
		{ "VDDisplay_FrameQueue", ATTestVDDisplayFrameQueue },
		{ "VDDisplay_PresentationBuffer", ATTestVDDisplayPresentationBuffer },
		{ "VDDisplay_Logging", ATTestVDDisplayLogging },
		{ "VDDisplay_RenderCache", ATTestVDDisplayRenderCache },
		{ "VDDisplay_Renderer", ATTestVDDisplayRenderer },
		{ "VDDisplay_RendererSoft", ATTestVDDisplayRendererSoft },
		{ "VDDisplay_ScreenFX", ATTestVDDisplayScreenFX },
		{ "VDDisplay_SoftwareComposition", ATTestVDDisplaySoftwareComposition },
		{ "VDDisplay_TextRenderer", ATTestVDDisplayTextRenderer },
#if defined(__APPLE__)
		{ "VDDisplay_ViewMac", ATTestVDDisplayViewMac },
		{ "VDDisplay_AdapterMac", ATTestVDDisplayAdapterMac },
		{ "VDDisplay_CompositionMac", ATTestVDDisplayCompositionMac },
		{ "VDDisplay_NativeViewMac", ATTestVDDisplayNativeViewMac },
		{ "Altirra_GTIADisplayMac", ATTestAltirraGTIADisplayMac },
		{ "Altirra_GTIAFrameMac", ATTestAltirraGTIAFrameMac },
		{ "Altirra_ANTICFrameMac", ATTestAltirraANTICFrameMac },
		{ "Altirra_ConsoleMac", ATTestAltirraConsoleMac },
		{ "NativeUI_WindowProxyMac", ATTestNativeUIWindowProxyMac },
		{ "NativeUI_WindowEventsMac", ATTestNativeUIWindowEventsMac },
#endif
		{ "NativeUI_WindowProxy", ATTestNativeUIWindowProxy },
		{ "NativeUI_WindowLifecycle", ATTestNativeUIWindowLifecycle },
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
