// Keep architecture-specific kernels out of the shared GTIA renderer.
#ifdef VD_CPU_X86
extern "C" void VDCDECL atasm_gtia_render_lores_fast_ssse3(
	void *dst, const uint8 *src, uint32 n, const uint8 *colorTable);
extern "C" void VDCDECL atasm_gtia_render_mode8_fast_ssse3(
	void *dst, const uint8 *src, const uint8 *lumaSrc, uint32 n, const uint8 *colorTable);
#elif defined(VD_CPU_X64)
#include "gtiarenderer_ssse3_intrin.inl"
#elif defined(VD_CPU_ARM64)
#include "gtiarenderer_neon.inl"
#endif
