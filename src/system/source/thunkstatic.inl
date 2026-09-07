// Platform-neutral static thunk implementation for targets that do not
// support the x86/x64 dynamic thunk generator.

#include <cstddef>
#include <cstring>
#include <utility>

#include <vd2/system/bitmath.h>
#include <vd2/system/thread.h>
#include <vd2/system/thunk.h>

#if defined(_MSC_VER)
	#define VD_THUNK_STDCALL __stdcall
#else
	#define VD_THUNK_STDCALL
#endif

bool VDInitThunkAllocator() {
	return true;
}

void VDShutdownThunkAllocator() {
}

template<unsigned IdBase, unsigned N, typename T_Fn>
struct VDThunkTable {
	typedef void *Thunk;

	static void *spThis[N];
	alignas(std::max_align_t) static unsigned char sData[N][sizeof(void *) * 4];
	static T_Fn spFns[N];
	static uint32 sBitField[N / 32];

	VDCriticalSection mMutex;

	static_assert(N % 32 == 0);

	template<unsigned Index, typename T_Ret, typename... T_Args>
	static constexpr T_Ret (VD_THUNK_STDCALL *GetThunk(T_Ret (*)(void *, const void *, T_Args...)))(T_Args...) {
		return [](T_Args... args) {
			return spFns[Index](spThis[Index], sData[Index], args...);
		};
	}

	template<unsigned... T_Indices>
	static const Thunk *GetThunks(std::integer_sequence<unsigned, T_Indices...>) {
		// This generates a unique table of thunk functions, each specialized to use a
		// specific index. Thus, we are constrained in the non-dynamic mode to have a fixed
		// size pool of thunks.
#if VD_COMPILER_MSVC
		static constexpr Thunk kThunks[]={
			GetThunk<T_Indices>((T_Fn)nullptr)...
		};
#else
		static const Thunk kThunks[]={
			reinterpret_cast<Thunk>(GetThunk<T_Indices>((T_Fn)nullptr))...
		};
#endif

		return kThunks;
	}

	template<typename T_Indices = std::make_integer_sequence<unsigned, N>>
	static const Thunk& GetThunk(unsigned index) {
		const Thunk *kThunks = GetThunks(T_Indices{});
		return kThunks[index];
	}

	VDFunctionThunkInfo *AllocThunk(void *pThis, const void *pData, size_t nData, T_Fn fn) {
		if (!fn || nData > sizeof sData[0] || (nData && !pData))
			return nullptr;

		VDCriticalSection::AutoLock lock(mMutex);

		uint32 index = UINT32_MAX;

		for(uint32 i = 0; i < N / 32; ++i) {
			const uint32 freeBits = ~sBitField[i];

			if (freeBits) {
				const uint32 bitPos = VDFindLowestSetBitFast(freeBits);

				sBitField[i] |= UINT32_C(1) << bitPos;
				index = (i << 5) + bitPos;
				break;
			}
		}

		if (index == UINT32_MAX)
			return nullptr;

		spThis[index] = pThis;
		if (nData)
			memcpy(sData[index], pData, nData);
		spFns[index] = fn;

		return reinterpret_cast<VDFunctionThunkInfo *>(
			const_cast<Thunk *>(&GetThunk(index)));
	}

	bool FreeThunk(VDFunctionThunkInfo *thunk) {
		if (!thunk)
			return true;

		const uintptr first = reinterpret_cast<uintptr>(&GetThunk(0));
		const uintptr candidate = reinterpret_cast<uintptr>(thunk);
		if (candidate < first)
			return false;

		const uintptr offset = candidate - first;
		if (offset >= sizeof(Thunk) * N || offset % sizeof(Thunk))
			return false;

		VDCriticalSection::AutoLock lock(mMutex);

		const uint32 index = static_cast<uint32>(offset / sizeof(Thunk));
		const uint32 mask = UINT32_C(1) << (index & 31);
		if (!(sBitField[index >> 5] & mask))
			return false;

		sBitField[index >> 5] &= ~mask;
		spThis[index] = nullptr;
		spFns[index] = nullptr;
		memset(sData[index], 0, sizeof sData[index]);
		return true;
	}

	static VDThunkTable& GetInstance() {
		static VDThunkTable s;

		return s;
	}
};

template<unsigned IdBase, unsigned N, typename T_Fn>
void *VDThunkTable<IdBase, N, T_Fn>::spThis[N];

template<unsigned IdBase, unsigned N, typename T_Fn>
alignas(std::max_align_t) unsigned char VDThunkTable<IdBase, N, T_Fn>::sData[N][sizeof(void *) * 4];

template<unsigned IdBase, unsigned N, typename T_Fn>
T_Fn VDThunkTable<IdBase, N, T_Fn>::spFns[N];

template<unsigned IdBase, unsigned N, typename T_Fn>
uint32 VDThunkTable<IdBase, N, T_Fn>::sBitField[N / 32];

typedef VDThunkTable<0, 64, VDThunkTypeT> VDThunkT;
typedef VDThunkTable<1, 512, VDThunkTypeW> VDThunkW;
typedef VDThunkTable<2, 64, VDThunkTypeH> VDThunkH;

VDFunctionThunkInfo *VDCreateFunctionThunkFromMethod(void *pThis, void *pData, size_t nData, VDThunkTypeT pfn) {
	return VDThunkT::GetInstance().AllocThunk(pThis, pData, nData, pfn);
}

VDFunctionThunkInfo *VDCreateFunctionThunkFromMethod(void *pThis, void *pData, size_t nData, VDThunkTypeW pfn) {
	return VDThunkW::GetInstance().AllocThunk(pThis, pData, nData, pfn);
}

VDFunctionThunkInfo *VDCreateFunctionThunkFromMethod(void *pThis, void *pData, size_t nData, VDThunkTypeH pfn) {
	return VDThunkH::GetInstance().AllocThunk(pThis, pData, nData, pfn);
}

void VDDestroyFunctionThunk(VDFunctionThunkInfo *thunk) {
	VDVERIFY(VDThunkT::GetInstance().FreeThunk(thunk)
		|| VDThunkW::GetInstance().FreeThunk(thunk)
		|| VDThunkH::GetInstance().FreeThunk(thunk));
}

#undef VD_THUNK_STDCALL
