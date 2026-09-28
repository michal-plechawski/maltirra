// Portable VDDisplay generic render-cache tests.

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/rendercache.h>

bool ATTestVDDisplayRenderCache(ATPortableTestContext& context) {
	VDDisplaySubRenderCache subRenderCache;
	VDDisplayRenderCacheGeneric renderCache;

	AT_PORTABLE_TEST_ASSERT(context, renderCache.AsInterface(VDDisplayRenderCacheGeneric::kTypeID) == &renderCache);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.AsInterface(0) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.Init(subRenderCache, 13, 7, nsVDPixmap::kPixFormat_XRGB8888));
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.data != nullptr);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.w == 13);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.h == 7);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.pitch >= 13 * 4);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.format == nsVDPixmap::kPixFormat_XRGB8888);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mImageView.GetImage().data == renderCache.mBuffer.data);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mImageView.GetImage().w == 13);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mImageView.GetImage().h == 7);
	AT_PORTABLE_TEST_ASSERT(context, !renderCache.mImageView.IsDynamic());
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mUniquenessCounter == subRenderCache.GetUniquenessCounter() - 1);

	renderCache.Update(subRenderCache);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mUniquenessCounter == 0);
	subRenderCache.Invalidate();
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mUniquenessCounter != subRenderCache.GetUniquenessCounter());
	renderCache.Update(subRenderCache);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mUniquenessCounter == 1);

	AT_PORTABLE_TEST_ASSERT(context, renderCache.Init(subRenderCache, 8, 5, nsVDPixmap::kPixFormat_Y8));
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.w == 8);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.h == 5);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mBuffer.format == nsVDPixmap::kPixFormat_Y8);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mImageView.GetImage().data == renderCache.mBuffer.data);
	AT_PORTABLE_TEST_ASSERT(context, renderCache.mUniquenessCounter == 0);
	return true;
}
