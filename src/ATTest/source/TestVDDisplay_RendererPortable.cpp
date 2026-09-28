// Portable VDDisplay renderer state tests.

#include <at/attest/portabletest.h>
#include <vd2/VDDisplay/renderer.h>

namespace {
	class TestCache final : public IVDRefUnknown {
	public:
		void *AsInterface(uint32) override { return nullptr; }
		int AddRef() override { return ++mRefCount; }
		int Release() override { return --mRefCount; }

		int mRefCount = 0;
	};
}

bool ATTestVDDisplayRenderer(ATPortableTestContext& context) {
	VDDisplaySubRenderCache subRenderCache;
	AT_PORTABLE_TEST_ASSERT(context, subRenderCache.GetCache() == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, subRenderCache.GetUniquenessCounter() == 0);
	subRenderCache.Invalidate();
	AT_PORTABLE_TEST_ASSERT(context, subRenderCache.GetUniquenessCounter() == 1);

	TestCache subCache;
	subRenderCache.SetCache(&subCache);
	AT_PORTABLE_TEST_ASSERT(context, subRenderCache.GetCache() == &subCache);
	AT_PORTABLE_TEST_ASSERT(context, subCache.mRefCount == 1);
	subRenderCache.SetCache(nullptr);
	AT_PORTABLE_TEST_ASSERT(context, subCache.mRefCount == 0);

	TestCache cache1;
	TestCache cache2;
	TestCache cache3;
	VDDisplayImageView imageView;
	const VDPixmap& emptyImage = imageView.GetImage();
	AT_PORTABLE_TEST_ASSERT(context, !imageView.IsDynamic());
	AT_PORTABLE_TEST_ASSERT(context, emptyImage.data == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, emptyImage.w == 0);
	AT_PORTABLE_TEST_ASSERT(context, emptyImage.h == 0);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetUniquenessCounter() == 0);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetDirtyListSize() == 0);

	imageView.SetVirtualImage(320, 240);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetImage().w == 320);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetImage().h == 240);
	AT_PORTABLE_TEST_ASSERT(context, !imageView.IsDynamic());

	VDPixmap px = {};
	uint32 pixels[4] = {};
	px.data = pixels;
	px.w = 2;
	px.h = 2;
	px.pitch = 8;
	px.format = nsVDPixmap::kPixFormat_XRGB8888;
	imageView.SetImage(px, true);
	AT_PORTABLE_TEST_ASSERT(context, imageView.IsDynamic());
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetImage().data == pixels);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetImage().pitch == 8);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetImage().format == nsVDPixmap::kPixFormat_XRGB8888);

	const vdrect32 dirtyRects[] = {
		vdrect32(1, 2, 3, 4),
		vdrect32(5, 6, 7, 8)
	};
	imageView.Invalidate(dirtyRects, 2);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetUniquenessCounter() == 1);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetDirtyListSize() == 2);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetDirtyList()[0] == dirtyRects[0]);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetDirtyList()[1] == dirtyRects[1]);

	imageView.Invalidate(dirtyRects, 0);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetUniquenessCounter() == 1);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetDirtyListSize() == 2);
	imageView.Invalidate();
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetUniquenessCounter() == 2);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetDirtyListSize() == 0);

	imageView.SetCachedImage(11, &cache1);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(11) == &cache1);
	AT_PORTABLE_TEST_ASSERT(context, cache1.mRefCount == 1);
	imageView.SetCachedImage(22, &cache2);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(11) == &cache1);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(22) == &cache2);
	imageView.SetCachedImage(33, &cache3);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(11) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(22) == &cache2);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(33) == &cache3);
	AT_PORTABLE_TEST_ASSERT(context, cache1.mRefCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, cache2.mRefCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, cache3.mRefCount == 1);
	imageView.SetCachedImage(33, &cache1);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(22) == &cache2);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(33) == &cache1);
	AT_PORTABLE_TEST_ASSERT(context, cache1.mRefCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, cache2.mRefCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, cache3.mRefCount == 0);
	imageView.SetCachedImage(22, &cache3);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(22) == &cache3);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(33) == &cache1);
	AT_PORTABLE_TEST_ASSERT(context, cache1.mRefCount == 1);
	AT_PORTABLE_TEST_ASSERT(context, cache2.mRefCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, cache3.mRefCount == 1);

	imageView.SetImage();
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(22) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetCachedImage(33) == nullptr);
	AT_PORTABLE_TEST_ASSERT(context, cache1.mRefCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, cache3.mRefCount == 0);
	AT_PORTABLE_TEST_ASSERT(context, !imageView.IsDynamic());
	AT_PORTABLE_TEST_ASSERT(context, imageView.GetImage().data == nullptr);
	return true;
}
