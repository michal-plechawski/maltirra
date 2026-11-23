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
bool ATTestAltirraCIORegistration(ATPortableTestContext& context);
bool ATTestAltirraCIOFullTable(ATPortableTestContext& context);
bool ATTestAltirraCIOMemory(ATPortableTestContext& context);
bool ATTestAltirraCIOCommands(ATPortableTestContext& context);
bool ATTestAltirraCIOAsync(ATPortableTestContext& context);
bool ATTestAltirraCIOLifecycle(ATPortableTestContext& context);
bool ATTestAltirraCIOBytes(ATPortableTestContext& context);
bool ATTestAltirraCIOBurst(ATPortableTestContext& context);
bool ATTestAltirraCIOWriteAccel(ATPortableTestContext& context);
bool ATTestAltirraCIOWriteGuards(ATPortableTestContext& context);
bool ATTestAltirraCIOCPU(ATPortableTestContext& context);
bool ATTestAltirraCIOPBI(ATPortableTestContext& context);
bool ATTestAltirraCIOROM(ATPortableTestContext& context);
bool ATTestAltirraCIOPrinter(ATPortableTestContext& context);
bool ATTestAltirraCIOEditor(ATPortableTestContext& context);
bool ATTestAltirraCIOCassette(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenOutput(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenControls(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenEditing(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenScroll(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenResize(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenInput(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenKeyboard(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenModes(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenReset(ATPortableTestContext& context);
bool ATTestAltirraVirtualScreenCPU(ATPortableTestContext& context);
bool ATTestAltirraBasicLoaderCommand(ATPortableTestContext& context);
bool ATTestAltirraBasicLoaderIO(ATPortableTestContext& context);
bool ATTestAltirraBasicLoaderTape(ATPortableTestContext& context);
bool ATTestAltirraBasicLoaderLifecycle(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderSegments(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderCPU(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderValidation(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderLifecycle(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderRandomization(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderSymbols(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderDisk(ATPortableTestContext& context);
bool ATTestAltirraProgramLoaderType3(ATPortableTestContext& context);
bool ATTestAltirraDeviceRegistration(ATPortableTestContext& context);
bool ATTestAltirraDeviceRollback(ATPortableTestContext& context);
bool ATTestAltirraDeviceCallbacks(ATPortableTestContext& context);
bool ATTestAltirraDeviceServices(ATPortableTestContext& context);
bool ATTestAltirraDeviceReconfigure(ATPortableTestContext& context);
bool ATTestAltirraDeviceSweep(ATPortableTestContext& context);
bool ATTestAltirraDevicePaths(ATPortableTestContext& context);
bool ATTestAltirraDeviceJSON(ATPortableTestContext& context);
bool ATTestAltirraDeviceAttachment(ATPortableTestContext& context);
bool ATTestAltirraDeviceSnapshots(ATPortableTestContext& context);
bool ATTestAltirraDeviceNVRAM(ATPortableTestContext& context);
bool ATTestAltirraDeviceStorage(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceHistory(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceModes(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceContexts(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceIdle(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceBASIC(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceLive(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceOverflow(ATPortableTestContext& context);
bool ATTestAltirraCPUTraceLifecycle(ATPortableTestContext& context);
bool ATTestAltirraMathPackIntegers(ATPortableTestContext& context);
bool ATTestAltirraMathPackStrings(ATPortableTestContext& context);
bool ATTestAltirraMathPackArithmetic(ATPortableTestContext& context);
bool ATTestAltirraMathPackFunctions(ATPortableTestContext& context);
bool ATTestAltirraMathPackMemory(ATPortableTestContext& context);
bool ATTestAltirraMathPackHooks(ATPortableTestContext& context);
bool ATTestAltirraFastBootClear(ATPortableTestContext& context);
bool ATTestAltirraFastBootMemCheck(ATPortableTestContext& context);
bool ATTestAltirraFastBootChecksum(ATPortableTestContext& context);
bool ATTestAltirraFastBootLifecycle(ATPortableTestContext& context);
bool ATTestAltirraCartridgeMapping(ATPortableTestContext& context);
bool ATTestAltirraCartridgeDataBanking(ATPortableTestContext& context);
bool ATTestAltirraCartridgeAddressBanking(ATPortableTestContext& context);
bool ATTestAltirraCartridgeBountyBob(ATPortableTestContext& context);
bool ATTestAltirraCartridgeSIC(ATPortableTestContext& context);
bool ATTestAltirraCartridgeSC3D(ATPortableTestContext& context);
bool ATTestAltirraCartridgeCPU(ATPortableTestContext& context);
bool ATTestAltirraCartridgeFlash(ATPortableTestContext& context);
bool ATTestAltirraCartridgeRAM(ATPortableTestContext& context);
bool ATTestAltirraCartridgeTheCart(ATPortableTestContext& context);
bool ATTestAltirraCartridgeSnapshot(ATPortableTestContext& context);
bool ATTestAltirraCartridgeImages(ATPortableTestContext& context);
bool ATTestAltirraCartridgeLifecycle(ATPortableTestContext& context);
bool ATTestAltirraCartridgeModes(ATPortableTestContext& context);
bool ATTestAltirraCartridgeOSS(ATPortableTestContext& context);
bool ATTestAltirraCartridgeSmallROM(ATPortableTestContext& context);
bool ATTestAltirraCheatEngine(ATPortableTestContext& context);
bool ATTestAltirraConstants(ATPortableTestContext& context);
bool ATTestAltirraCompatDB(ATPortableTestContext& context);
bool ATTestAltirraCPUMemory(ATPortableTestContext& context);
bool ATTestAltirraCPUStates(ATPortableTestContext& context);
bool ATTestAltirraDisasm6502(ATPortableTestContext& context);
bool ATTestAltirraDisasmOtherCPUs(ATPortableTestContext& context);
bool ATTestAltirraDisasmContext(ATPortableTestContext& context);
bool ATTestAltirraDisasmReferences(ATPortableTestContext& context);
bool ATTestAltirraDisasmSymbols(ATPortableTestContext& context);
bool ATTestAltirraDisasmPrediction(ATPortableTestContext& context);
bool ATTestAltirraCPUVerifierAddressing(ATPortableTestContext& context);
bool ATTestAltirraCPUVerifierStack(ATPortableTestContext& context);
bool ATTestAltirraCPUVerifierInterrupts(ATPortableTestContext& context);
bool ATTestAltirraCPUVerifierKernel(ATPortableTestContext& context);
bool ATTestAltirraCPUVerifierMachine(ATPortableTestContext& context);
bool ATTestAltirraBreakpointPC(ATPortableTestContext& context);
bool ATTestAltirraBreakpointInstructions(ATPortableTestContext& context);
bool ATTestAltirraBreakpointTargets(ATPortableTestContext& context);
bool ATTestAltirraBreakpointMemory(ATPortableTestContext& context);
bool ATTestAltirraBreakpointRanges(ATPortableTestContext& context);
bool ATTestAltirraMainCPUBreakpoints(ATPortableTestContext& context);
bool ATTestAltirraMainCPU6502(ATPortableTestContext& context);
bool ATTestAltirraMainCPU65C02(ATPortableTestContext& context);
bool ATTestAltirraMainCPU816(ATPortableTestContext& context);
bool ATTestAltirraMainCPUAcceleration(ATPortableTestContext& context);
bool ATTestAltirraMainCPUHistory(ATPortableTestContext& context);
bool ATTestAltirraMainCPUSnapshot(ATPortableTestContext& context);
bool ATTestAltirraMainCPUDebug(ATPortableTestContext& context);
bool ATTestAltirraMainCPUDiagnostics(ATPortableTestContext& context);
bool ATTestAltirraCPULiveProfiler(ATPortableTestContext& context);
bool ATTestAltirraCovoxMemory(ATPortableTestContext& context);
bool ATTestAltirraCovoxAudio(ATPortableTestContext& context);
bool ATTestAltirraCovoxSnapshot(ATPortableTestContext& context);
bool ATTestAltirraCovoxDevice(ATPortableTestContext& context);
bool ATTestAltirraSimCovox(ATPortableTestContext& context);
bool ATTestAltirraSlightSIDTiming(ATPortableTestContext& context);
bool ATTestAltirraSlightSIDEnvelopes(ATPortableTestContext& context);
bool ATTestAltirraSlightSIDWaveforms(ATPortableTestContext& context);
bool ATTestAltirraSlightSIDFilters(ATPortableTestContext& context);
bool ATTestAltirraSlightSIDDevice(ATPortableTestContext& context);
bool ATTestAltirraMMUROM(ATPortableTestContext& context);
bool ATTestAltirraMMUBanking(ATPortableTestContext& context);
bool ATTestAltirraMMUConnections(ATPortableTestContext& context);
bool ATTestAltirraMMUAxlon(ATPortableTestContext& context);
bool ATTestAltirraMMUHighMemory(ATPortableTestContext& context);
bool ATTestAltirraMMULifecycle(ATPortableTestContext& context);
bool ATTestAltirraPortDirections(ATPortableTestContext& context);
bool ATTestAltirraPortTriggers(ATPortableTestContext& context);
bool ATTestAltirraPortPots(ATPortableTestContext& context);
bool ATTestAltirraPortTiming(ATPortableTestContext& context);
bool ATTestAltirraPortLifecycle(ATPortableTestContext& context);
bool ATTestAltirraLightPen(ATPortableTestContext& context);
bool ATTestAltirraInputRouting(ATPortableTestContext& context);
bool ATTestAltirraInput5200(ATPortableTestContext& context);
bool ATTestAltirraInputLightPen(ATPortableTestContext& context);
bool ATTestAltirraInputLifecycle(ATPortableTestContext& context);
bool ATTestAltirraInputMouse(ATPortableTestContext& context);
bool ATTestAltirraInputTrackball(ATPortableTestContext& context);
bool ATTestAltirraInputDriving(ATPortableTestContext& context);
bool ATTestAltirraInputPaddle(ATPortableTestContext& context);
bool ATTestAltirraInputJoystick(ATPortableTestContext& context);
bool ATTestAltirraInputTablet(ATPortableTestContext& context);
bool ATTestAltirraInputKeypad(ATPortableTestContext& context);
bool ATTestAltirraInputKeyboard(ATPortableTestContext& context);
bool ATTestAltirraInputPowerPad(ATPortableTestContext& context);
bool ATTestAltirraInputManagerButtons(ATPortableTestContext& context);
bool ATTestAltirraInputManagerModes(ATPortableTestContext& context);
bool ATTestAltirraInputManagerFlags(ATPortableTestContext& context);
bool ATTestAltirraInputManagerMaps(ATPortableTestContext& context);
bool ATTestAltirraInputManagerUnits(ATPortableTestContext& context);
bool ATTestAltirraInputManagerPersistence(ATPortableTestContext& context);
bool ATTestAltirraInputManagerAnalog(ATPortableTestContext& context);
bool ATTestAltirraInputManagerPointers(ATPortableTestContext& context);
bool ATTestAltirraInputManagerWheel(ATPortableTestContext& context);
bool ATTestAltirraInputKeyNames(ATPortableTestContext& context);
bool ATTestAltirraFirmwareTypes(ATPortableTestContext& context);
bool ATTestAltirraFirmwarePathIds(ATPortableTestContext& context);
bool ATTestAltirraFirmwareInternal(ATPortableTestContext& context);
bool ATTestAltirraFirmwareCatalog(ATPortableTestContext& context);
bool ATTestAltirraFirmwareFiles(ATPortableTestContext& context);
bool ATTestAltirraFirmwareVFS(ATPortableTestContext& context);
bool ATTestAltirraCassetteTransport(ATPortableTestContext& context);
bool ATTestAltirraCassetteLifecycle(ATPortableTestContext& context);
bool ATTestAltirraCassetteBlockRead(ATPortableTestContext& context);
bool ATTestAltirraCassetteBlockWrite(ATPortableTestContext& context);
bool ATTestAltirraCassetteSerial(ATPortableTestContext& context);
bool ATTestAltirraCassetteTurbo(ATPortableTestContext& context);
bool ATTestAltirraCassetteAudio(ATPortableTestContext& context);
bool ATTestAltirraCassetteDetection(ATPortableTestContext& context);
bool ATTestAltirraSIOCommandFrames(ATPortableTestContext& context);
bool ATTestAltirraSIOTimedSend(ATPortableTestContext& context);
bool ATTestAltirraSIOReceive(ATPortableTestContext& context);
bool ATTestAltirraSIORawLines(ATPortableTestContext& context);
bool ATTestAltirraSIORawReentry(ATPortableTestContext& context);
bool ATTestAltirraSIOAcceleration(ATPortableTestContext& context);
bool ATTestAltirraSIOAccelGuards(ATPortableTestContext& context);
bool ATTestAltirraSIOHooksCassette(ATPortableTestContext& context);
bool ATTestAltirraSIOSnapshot(ATPortableTestContext& context);
bool ATTestAltirraSIOLifecycle(ATPortableTestContext& context);
bool ATTestAltirraSIOClocks(ATPortableTestContext& context);
bool ATTestAltirraSIOSerialWire(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceMedia(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceClients(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceSwap(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceIndicators(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceReentry(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceFiles(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceFolders(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceAutoFlush(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceErrors(ATPortableTestContext& context);
bool ATTestAltirraDiskInterfaceLifecycle(ATPortableTestContext& context);
bool ATTestAltirraDiskStatus(ATPortableTestContext& context);
bool ATTestAltirraDiskReadWrite(ATPortableTestContext& context);
bool ATTestAltirraDiskProtection(ATPortableTestContext& context);
bool ATTestAltirraDiskPERCOM(ATPortableTestContext& context);
bool ATTestAltirraDiskFormat(ATPortableTestContext& context);
bool ATTestAltirraDiskHighSpeed(ATPortableTestContext& context);
bool ATTestAltirraDiskWireRead(ATPortableTestContext& context);
bool ATTestAltirraDiskWireWrite(ATPortableTestContext& context);
bool ATTestAltirraDiskPhantoms(ATPortableTestContext& context);
bool ATTestAltirraDiskWeakSectors(ATPortableTestContext& context);
bool ATTestAltirraDiskHappyRAM(ATPortableTestContext& context);
bool ATTestAltirraDiskSnapshot(ATPortableTestContext& context);
bool ATTestAltirraDiskAudioTrace(ATPortableTestContext& context);
bool ATTestAltirraDiskLifecycle(ATPortableTestContext& context);
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
bool ATTestAltirraHostParsing(ATPortableTestContext& context);
bool ATTestAltirraHostWildcards(ATPortableTestContext& context);
bool ATTestAltirraHostSettings(ATPortableTestContext& context);
bool ATTestAltirraHostBinary(ATPortableTestContext& context);
bool ATTestAltirraHostPermissions(ATPortableTestContext& context);
bool ATTestAltirraHostText(ATPortableTestContext& context);
bool ATTestAltirraHostDirectory(ATPortableTestContext& context);
bool ATTestAltirraHostNavigation(ATPortableTestContext& context);
bool ATTestAltirraHostMutation(ATPortableTestContext& context);
bool ATTestAltirraHostLongNames(ATPortableTestContext& context);
bool ATTestAltirraHostAliases(ATPortableTestContext& context);
bool ATTestAltirraHostPositions(ATPortableTestContext& context);
bool ATTestAltirraHostOpenFailures(ATPortableTestContext& context);
bool ATTestAltirraHostNativeErrors(ATPortableTestContext& context);
bool ATTestAltirraHostLifecycle(ATPortableTestContext& context);
bool ATTestAltirraHostCPU(ATPortableTestContext& context);
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
		{ "Altirra_CIORegistration", ATTestAltirraCIORegistration },
		{ "Altirra_CIOFullTable", ATTestAltirraCIOFullTable },
		{ "Altirra_CIOMemory", ATTestAltirraCIOMemory },
		{ "Altirra_CIOCommands", ATTestAltirraCIOCommands },
		{ "Altirra_CIOAsync", ATTestAltirraCIOAsync },
		{ "Altirra_CIOLifecycle", ATTestAltirraCIOLifecycle },
		{ "Altirra_CIOBytes", ATTestAltirraCIOBytes },
		{ "Altirra_CIOBurst", ATTestAltirraCIOBurst },
		{ "Altirra_CIOWriteAccel", ATTestAltirraCIOWriteAccel },
		{ "Altirra_CIOWriteGuards", ATTestAltirraCIOWriteGuards },
		{ "Altirra_CIOCPU", ATTestAltirraCIOCPU },
		{ "Altirra_CIOPBI", ATTestAltirraCIOPBI },
		{ "Altirra_CIOROM", ATTestAltirraCIOROM },
		{ "Altirra_CIOPrinter", ATTestAltirraCIOPrinter },
		{ "Altirra_CIOEditor", ATTestAltirraCIOEditor },
		{ "Altirra_CIOCassette", ATTestAltirraCIOCassette },
		{ "Altirra_VirtualScreenOutput", ATTestAltirraVirtualScreenOutput },
		{ "Altirra_VirtualScreenControls", ATTestAltirraVirtualScreenControls },
		{ "Altirra_VirtualScreenEditing", ATTestAltirraVirtualScreenEditing },
		{ "Altirra_VirtualScreenScroll", ATTestAltirraVirtualScreenScroll },
		{ "Altirra_VirtualScreenResize", ATTestAltirraVirtualScreenResize },
		{ "Altirra_VirtualScreenInput", ATTestAltirraVirtualScreenInput },
		{ "Altirra_VirtualScreenKeyboard", ATTestAltirraVirtualScreenKeyboard },
		{ "Altirra_VirtualScreenModes", ATTestAltirraVirtualScreenModes },
		{ "Altirra_VirtualScreenReset", ATTestAltirraVirtualScreenReset },
		{ "Altirra_VirtualScreenCPU", ATTestAltirraVirtualScreenCPU },
		{ "Altirra_BasicLoaderCommand", ATTestAltirraBasicLoaderCommand },
		{ "Altirra_BasicLoaderIO", ATTestAltirraBasicLoaderIO },
		{ "Altirra_BasicLoaderTape", ATTestAltirraBasicLoaderTape },
		{ "Altirra_BasicLoaderLifecycle", ATTestAltirraBasicLoaderLifecycle },
		{ "Altirra_ProgramLoaderSegments", ATTestAltirraProgramLoaderSegments },
		{ "Altirra_ProgramLoaderCPU", ATTestAltirraProgramLoaderCPU },
		{ "Altirra_ProgramLoaderValidation", ATTestAltirraProgramLoaderValidation },
		{ "Altirra_ProgramLoaderLifecycle", ATTestAltirraProgramLoaderLifecycle },
		{ "Altirra_ProgramLoaderRandomization", ATTestAltirraProgramLoaderRandomization },
		{ "Altirra_ProgramLoaderSymbols", ATTestAltirraProgramLoaderSymbols },
		{ "Altirra_ProgramLoaderDisk", ATTestAltirraProgramLoaderDisk },
		{ "Altirra_ProgramLoaderType3", ATTestAltirraProgramLoaderType3 },
		{ "Altirra_DeviceRegistration", ATTestAltirraDeviceRegistration },
		{ "Altirra_DeviceRollback", ATTestAltirraDeviceRollback },
		{ "Altirra_DeviceCallbacks", ATTestAltirraDeviceCallbacks },
		{ "Altirra_DeviceServices", ATTestAltirraDeviceServices },
		{ "Altirra_DeviceReconfigure", ATTestAltirraDeviceReconfigure },
		{ "Altirra_DeviceSweep", ATTestAltirraDeviceSweep },
		{ "Altirra_DevicePaths", ATTestAltirraDevicePaths },
		{ "Altirra_DeviceJSON", ATTestAltirraDeviceJSON },
		{ "Altirra_DeviceAttachment", ATTestAltirraDeviceAttachment },
		{ "Altirra_DeviceSnapshots", ATTestAltirraDeviceSnapshots },
		{ "Altirra_DeviceNVRAM", ATTestAltirraDeviceNVRAM },
		{ "Altirra_DeviceStorage", ATTestAltirraDeviceStorage },
		{ "Altirra_CPUTraceHistory", ATTestAltirraCPUTraceHistory },
		{ "Altirra_CPUTraceModes", ATTestAltirraCPUTraceModes },
		{ "Altirra_CPUTraceContexts", ATTestAltirraCPUTraceContexts },
		{ "Altirra_CPUTraceIdle", ATTestAltirraCPUTraceIdle },
		{ "Altirra_CPUTraceBASIC", ATTestAltirraCPUTraceBASIC },
		{ "Altirra_CPUTraceLive", ATTestAltirraCPUTraceLive },
		{ "Altirra_CPUTraceOverflow", ATTestAltirraCPUTraceOverflow },
		{ "Altirra_CPUTraceLifecycle", ATTestAltirraCPUTraceLifecycle },
		{ "Altirra_MathPackIntegers", ATTestAltirraMathPackIntegers },
		{ "Altirra_MathPackStrings", ATTestAltirraMathPackStrings },
		{ "Altirra_MathPackArithmetic", ATTestAltirraMathPackArithmetic },
		{ "Altirra_MathPackFunctions", ATTestAltirraMathPackFunctions },
		{ "Altirra_MathPackMemory", ATTestAltirraMathPackMemory },
		{ "Altirra_MathPackHooks", ATTestAltirraMathPackHooks },
		{ "Altirra_FastBootClear", ATTestAltirraFastBootClear },
		{ "Altirra_FastBootMemCheck", ATTestAltirraFastBootMemCheck },
		{ "Altirra_FastBootChecksum", ATTestAltirraFastBootChecksum },
		{ "Altirra_FastBootLifecycle", ATTestAltirraFastBootLifecycle },
		{ "Altirra_CartridgeMapping", ATTestAltirraCartridgeMapping },
		{ "Altirra_CartridgeDataBanking", ATTestAltirraCartridgeDataBanking },
		{ "Altirra_CartridgeAddressBanking", ATTestAltirraCartridgeAddressBanking },
		{ "Altirra_CartridgeBountyBob", ATTestAltirraCartridgeBountyBob },
		{ "Altirra_CartridgeSIC", ATTestAltirraCartridgeSIC },
		{ "Altirra_CartridgeSC3D", ATTestAltirraCartridgeSC3D },
		{ "Altirra_CartridgeCPU", ATTestAltirraCartridgeCPU },
		{ "Altirra_CartridgeFlash", ATTestAltirraCartridgeFlash },
		{ "Altirra_CartridgeRAM", ATTestAltirraCartridgeRAM },
		{ "Altirra_CartridgeTheCart", ATTestAltirraCartridgeTheCart },
		{ "Altirra_CartridgeSnapshot", ATTestAltirraCartridgeSnapshot },
		{ "Altirra_CartridgeImages", ATTestAltirraCartridgeImages },
		{ "Altirra_CartridgeLifecycle", ATTestAltirraCartridgeLifecycle },
		{ "Altirra_CartridgeModes", ATTestAltirraCartridgeModes },
		{ "Altirra_CartridgeOSS", ATTestAltirraCartridgeOSS },
		{ "Altirra_CartridgeSmallROM", ATTestAltirraCartridgeSmallROM },
		{ "Altirra_CheatEngine", ATTestAltirraCheatEngine },
		{ "Altirra_Constants", ATTestAltirraConstants },
		{ "Altirra_CompatDB", ATTestAltirraCompatDB },
		{ "Altirra_CPUMemory", ATTestAltirraCPUMemory },
		{ "Altirra_CPUStates", ATTestAltirraCPUStates },
		{ "Altirra_Disasm6502", ATTestAltirraDisasm6502 },
		{ "Altirra_DisasmOtherCPUs", ATTestAltirraDisasmOtherCPUs },
		{ "Altirra_DisasmContext", ATTestAltirraDisasmContext },
		{ "Altirra_DisasmReferences", ATTestAltirraDisasmReferences },
		{ "Altirra_DisasmSymbols", ATTestAltirraDisasmSymbols },
		{ "Altirra_DisasmPrediction", ATTestAltirraDisasmPrediction },
		{ "Altirra_CPUVerifierAddressing", ATTestAltirraCPUVerifierAddressing },
		{ "Altirra_CPUVerifierStack", ATTestAltirraCPUVerifierStack },
		{ "Altirra_CPUVerifierInterrupts", ATTestAltirraCPUVerifierInterrupts },
		{ "Altirra_CPUVerifierKernel", ATTestAltirraCPUVerifierKernel },
		{ "Altirra_CPUVerifierMachine", ATTestAltirraCPUVerifierMachine },
		{ "Altirra_BreakpointPC", ATTestAltirraBreakpointPC },
		{ "Altirra_BreakpointInstructions", ATTestAltirraBreakpointInstructions },
		{ "Altirra_BreakpointTargets", ATTestAltirraBreakpointTargets },
		{ "Altirra_BreakpointMemory", ATTestAltirraBreakpointMemory },
		{ "Altirra_BreakpointRanges", ATTestAltirraBreakpointRanges },
		{ "Altirra_MainCPUBreakpoints", ATTestAltirraMainCPUBreakpoints },
		{ "Altirra_MainCPU6502", ATTestAltirraMainCPU6502 },
		{ "Altirra_MainCPU65C02", ATTestAltirraMainCPU65C02 },
		{ "Altirra_MainCPU816", ATTestAltirraMainCPU816 },
		{ "Altirra_MainCPUAcceleration", ATTestAltirraMainCPUAcceleration },
		{ "Altirra_MainCPUHistory", ATTestAltirraMainCPUHistory },
		{ "Altirra_MainCPUSnapshot", ATTestAltirraMainCPUSnapshot },
		{ "Altirra_MainCPUDebug", ATTestAltirraMainCPUDebug },
		{ "Altirra_MainCPUDiagnostics", ATTestAltirraMainCPUDiagnostics },
		{ "Altirra_CPULiveProfiler", ATTestAltirraCPULiveProfiler },
		{ "Altirra_CovoxMemory", ATTestAltirraCovoxMemory },
		{ "Altirra_CovoxAudio", ATTestAltirraCovoxAudio },
		{ "Altirra_CovoxSnapshot", ATTestAltirraCovoxSnapshot },
		{ "Altirra_CovoxDevice", ATTestAltirraCovoxDevice },
		{ "Altirra_SimCovox", ATTestAltirraSimCovox },
		{ "Altirra_SlightSIDTiming", ATTestAltirraSlightSIDTiming },
		{ "Altirra_SlightSIDEnvelopes", ATTestAltirraSlightSIDEnvelopes },
		{ "Altirra_SlightSIDWaveforms", ATTestAltirraSlightSIDWaveforms },
		{ "Altirra_SlightSIDFilters", ATTestAltirraSlightSIDFilters },
		{ "Altirra_SlightSIDDevice", ATTestAltirraSlightSIDDevice },
		{ "Altirra_MMUROM", ATTestAltirraMMUROM },
		{ "Altirra_MMUBanking", ATTestAltirraMMUBanking },
		{ "Altirra_MMUConnections", ATTestAltirraMMUConnections },
		{ "Altirra_MMUAxlon", ATTestAltirraMMUAxlon },
		{ "Altirra_MMUHighMemory", ATTestAltirraMMUHighMemory },
		{ "Altirra_MMULifecycle", ATTestAltirraMMULifecycle },
		{ "Altirra_PortDirections", ATTestAltirraPortDirections },
		{ "Altirra_PortTriggers", ATTestAltirraPortTriggers },
		{ "Altirra_PortPots", ATTestAltirraPortPots },
		{ "Altirra_PortTiming", ATTestAltirraPortTiming },
		{ "Altirra_PortLifecycle", ATTestAltirraPortLifecycle },
		{ "Altirra_LightPen", ATTestAltirraLightPen },
		{ "Altirra_InputRouting", ATTestAltirraInputRouting },
		{ "Altirra_Input5200", ATTestAltirraInput5200 },
		{ "Altirra_InputLightPen", ATTestAltirraInputLightPen },
		{ "Altirra_InputLifecycle", ATTestAltirraInputLifecycle },
		{ "Altirra_InputMouse", ATTestAltirraInputMouse },
		{ "Altirra_InputTrackball", ATTestAltirraInputTrackball },
		{ "Altirra_InputDriving", ATTestAltirraInputDriving },
		{ "Altirra_InputPaddle", ATTestAltirraInputPaddle },
		{ "Altirra_InputJoystick", ATTestAltirraInputJoystick },
		{ "Altirra_InputTablet", ATTestAltirraInputTablet },
		{ "Altirra_InputKeypad", ATTestAltirraInputKeypad },
		{ "Altirra_InputKeyboard", ATTestAltirraInputKeyboard },
		{ "Altirra_InputPowerPad", ATTestAltirraInputPowerPad },
		{ "Altirra_InputManagerButtons", ATTestAltirraInputManagerButtons },
		{ "Altirra_InputManagerModes", ATTestAltirraInputManagerModes },
		{ "Altirra_InputManagerFlags", ATTestAltirraInputManagerFlags },
		{ "Altirra_InputManagerMaps", ATTestAltirraInputManagerMaps },
		{ "Altirra_InputManagerUnits", ATTestAltirraInputManagerUnits },
		{ "Altirra_InputManagerPersistence", ATTestAltirraInputManagerPersistence },
		{ "Altirra_InputManagerAnalog", ATTestAltirraInputManagerAnalog },
		{ "Altirra_InputManagerPointers", ATTestAltirraInputManagerPointers },
		{ "Altirra_InputManagerWheel", ATTestAltirraInputManagerWheel },
		{ "Altirra_InputKeyNames", ATTestAltirraInputKeyNames },
		{ "Altirra_FirmwareTypes", ATTestAltirraFirmwareTypes },
		{ "Altirra_FirmwarePathIds", ATTestAltirraFirmwarePathIds },
		{ "Altirra_FirmwareInternal", ATTestAltirraFirmwareInternal },
		{ "Altirra_FirmwareCatalog", ATTestAltirraFirmwareCatalog },
		{ "Altirra_FirmwareFiles", ATTestAltirraFirmwareFiles },
		{ "Altirra_FirmwareVFS", ATTestAltirraFirmwareVFS },
		{ "Altirra_CassetteTransport", ATTestAltirraCassetteTransport },
		{ "Altirra_CassetteLifecycle", ATTestAltirraCassetteLifecycle },
		{ "Altirra_CassetteBlockRead", ATTestAltirraCassetteBlockRead },
		{ "Altirra_CassetteBlockWrite", ATTestAltirraCassetteBlockWrite },
		{ "Altirra_CassetteSerial", ATTestAltirraCassetteSerial },
		{ "Altirra_CassetteTurbo", ATTestAltirraCassetteTurbo },
		{ "Altirra_CassetteAudio", ATTestAltirraCassetteAudio },
		{ "Altirra_CassetteDetection", ATTestAltirraCassetteDetection },
		{ "Altirra_SIOCommandFrames", ATTestAltirraSIOCommandFrames },
		{ "Altirra_SIOTimedSend", ATTestAltirraSIOTimedSend },
		{ "Altirra_SIOReceive", ATTestAltirraSIOReceive },
		{ "Altirra_SIORawLines", ATTestAltirraSIORawLines },
		{ "Altirra_SIORawReentry", ATTestAltirraSIORawReentry },
		{ "Altirra_SIOAcceleration", ATTestAltirraSIOAcceleration },
		{ "Altirra_SIOAccelGuards", ATTestAltirraSIOAccelGuards },
		{ "Altirra_SIOHooksCassette", ATTestAltirraSIOHooksCassette },
		{ "Altirra_SIOSnapshot", ATTestAltirraSIOSnapshot },
		{ "Altirra_SIOLifecycle", ATTestAltirraSIOLifecycle },
		{ "Altirra_SIOClocks", ATTestAltirraSIOClocks },
		{ "Altirra_SIOSerialWire", ATTestAltirraSIOSerialWire },
		{ "Altirra_DiskInterfaceMedia", ATTestAltirraDiskInterfaceMedia },
		{ "Altirra_DiskInterfaceClients", ATTestAltirraDiskInterfaceClients },
		{ "Altirra_DiskInterfaceSwap", ATTestAltirraDiskInterfaceSwap },
		{ "Altirra_DiskInterfaceIndicators", ATTestAltirraDiskInterfaceIndicators },
		{ "Altirra_DiskInterfaceReentry", ATTestAltirraDiskInterfaceReentry },
		{ "Altirra_DiskInterfaceFiles", ATTestAltirraDiskInterfaceFiles },
		{ "Altirra_DiskInterfaceFolders", ATTestAltirraDiskInterfaceFolders },
		{ "Altirra_DiskInterfaceAutoFlush", ATTestAltirraDiskInterfaceAutoFlush },
		{ "Altirra_DiskInterfaceErrors", ATTestAltirraDiskInterfaceErrors },
		{ "Altirra_DiskInterfaceLifecycle", ATTestAltirraDiskInterfaceLifecycle },
		{ "Altirra_DiskStatus", ATTestAltirraDiskStatus },
		{ "Altirra_DiskReadWrite", ATTestAltirraDiskReadWrite },
		{ "Altirra_DiskProtection", ATTestAltirraDiskProtection },
		{ "Altirra_DiskPERCOM", ATTestAltirraDiskPERCOM },
		{ "Altirra_DiskFormat", ATTestAltirraDiskFormat },
		{ "Altirra_DiskHighSpeed", ATTestAltirraDiskHighSpeed },
		{ "Altirra_DiskWireRead", ATTestAltirraDiskWireRead },
		{ "Altirra_DiskWireWrite", ATTestAltirraDiskWireWrite },
		{ "Altirra_DiskPhantoms", ATTestAltirraDiskPhantoms },
		{ "Altirra_DiskWeakSectors", ATTestAltirraDiskWeakSectors },
		{ "Altirra_DiskHappyRAM", ATTestAltirraDiskHappyRAM },
		{ "Altirra_DiskSnapshot", ATTestAltirraDiskSnapshot },
		{ "Altirra_DiskAudioTrace", ATTestAltirraDiskAudioTrace },
		{ "Altirra_DiskLifecycle", ATTestAltirraDiskLifecycle },
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
		{ "Altirra_HostParsing", ATTestAltirraHostParsing },
		{ "Altirra_HostWildcards", ATTestAltirraHostWildcards },
		{ "Altirra_HostSettings", ATTestAltirraHostSettings },
		{ "Altirra_HostBinary", ATTestAltirraHostBinary },
		{ "Altirra_HostPermissions", ATTestAltirraHostPermissions },
		{ "Altirra_HostText", ATTestAltirraHostText },
		{ "Altirra_HostDirectory", ATTestAltirraHostDirectory },
		{ "Altirra_HostNavigation", ATTestAltirraHostNavigation },
		{ "Altirra_HostMutation", ATTestAltirraHostMutation },
		{ "Altirra_HostLongNames", ATTestAltirraHostLongNames },
		{ "Altirra_HostAliases", ATTestAltirraHostAliases },
		{ "Altirra_HostPositions", ATTestAltirraHostPositions },
		{ "Altirra_HostOpenFailures", ATTestAltirraHostOpenFailures },
		{ "Altirra_HostNativeErrors", ATTestAltirraHostNativeErrors },
		{ "Altirra_HostLifecycle", ATTestAltirraHostLifecycle },
		{ "Altirra_HostCPU", ATTestAltirraHostCPU },
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
