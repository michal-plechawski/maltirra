// Altirra cassette decoder implementation internals

#ifndef f_AT_ATIO_INTERNAL_CASSETTEDECODER_H
#define f_AT_ATIO_INTERNAL_CASSETTEDECODER_H

#include <vd2/system/vdtypes.h>

struct ATCassetteDecoderFIRState {
	alignas(16) float mValues[16];
};

alignas(16) inline constexpr float kATCassetteDecoderHPFKernel[16] {
	-0.0123454f, -0.0246906f, -0.0370356f, -0.0493802f,
	-0.0617247f, -0.0740689f, -0.0864128f, 0.901243f,
	-0.0864091f, -0.074062f, -0.061715f, -0.0493684f,
	-0.037022f, -0.0246758f, -0.0123299f, 0
};

void ATCassetteDecoderFIRInit(
	ATCassetteDecoderFIRState& state, const float history[16]);
float ATCassetteDecoderFIRProcess(
	ATCassetteDecoderFIRState& state, float sample);
void ATCassetteDecoderFIRStore(
	float history[16], const ATCassetteDecoderFIRState& state);

#if VD_CPU_X86 || VD_CPU_X64
void ATCassetteDecoderFSKProcessSSE2(
	const sint16 *samples, uint32 n, uint32 *bitfield, uint32 bitoffset,
	float *analysis, bool enableAnalysis,
	sint32 accumulator[4], uint32& index, sint16 history[24],
	float markGain, const sint16 rotationTable[24][4]);
#endif

#endif
