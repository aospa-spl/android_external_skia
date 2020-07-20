/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImageEncoder.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkImageGenerator.h"
#include "include/core/SkPicture.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "src/core/SkBitmapCache.h"
#include "src/core/SkCachedData.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkImageFilterCache.h"
#include "src/core/SkImageFilter_Base.h"
#include "src/core/SkImagePriv.h"
#include "src/core/SkMipmap.h"
#include "src/core/SkNextID.h"
#include "src/core/SkSpecialImage.h"
#include "src/image/SkImage_Base.h"
#include "src/image/SkReadPixelsRec.h"
#include "src/image/SkRescaleAndReadPixels.h"
#include "src/shaders/SkImageShader.h"

#if SK_SUPPORT_GPU
#include "include/gpu/GrDirectContext.h"
#include "src/gpu/GrContextPriv.h"
#include "src/image/SkImage_Gpu.h"
#endif
#include "include/gpu/GrBackendSurface.h"

SkImage::SkImage(const SkImageInfo& info, uint32_t uniqueID)
        : fInfo(info)
        , fUniqueID(kNeedNewImageUniqueID == uniqueID ? SkNextID::ImageID() : uniqueID) {
    SkASSERT(info.width() > 0);
    SkASSERT(info.height() > 0);
}

bool SkImage::peekPixels(SkPixmap* pm) const {
    SkPixmap tmp;
    if (!pm) {
        pm = &tmp;
    }
    return as_IB(this)->onPeekPixels(pm);
}

bool SkImage::readPixels(const SkImageInfo& dstInfo, void* dstPixels, size_t dstRowBytes, int srcX,
                         int srcY, CachingHint chint) const {
    return as_IB(this)->onReadPixels(dstInfo, dstPixels, dstRowBytes, srcX, srcY, chint);
}

void SkImage::asyncRescaleAndReadPixels(const SkImageInfo& info,
                                        const SkIRect& srcRect,
                                        RescaleGamma rescaleGamma,
                                        SkFilterQuality rescaleQuality,
                                        ReadPixelsCallback callback,
                                        ReadPixelsContext context) {
    if (!SkIRect::MakeWH(this->width(), this->height()).contains(srcRect) ||
        !SkImageInfoIsValid(info)) {
        callback(context, nullptr);
        return;
    }
    as_IB(this)->onAsyncRescaleAndReadPixels(
            info, srcRect, rescaleGamma, rescaleQuality, callback, context);
}

void SkImage::asyncRescaleAndReadPixelsYUV420(SkYUVColorSpace yuvColorSpace,
                                              sk_sp<SkColorSpace> dstColorSpace,
                                              const SkIRect& srcRect,
                                              const SkISize& dstSize,
                                              RescaleGamma rescaleGamma,
                                              SkFilterQuality rescaleQuality,
                                              ReadPixelsCallback callback,
                                              ReadPixelsContext context) {
    if (!SkIRect::MakeWH(this->width(), this->height()).contains(srcRect) || dstSize.isZero() ||
        (dstSize.width() & 0b1) || (dstSize.height() & 0b1)) {
        callback(context, nullptr);
        return;
    }
    as_IB(this)->onAsyncRescaleAndReadPixelsYUV420(yuvColorSpace,
                                                   std::move(dstColorSpace),
                                                   srcRect,
                                                   dstSize,
                                                   rescaleGamma,
                                                   rescaleQuality,
                                                   callback,
                                                   context);
}

bool SkImage::scalePixels(const SkPixmap& dst, SkFilterQuality quality, CachingHint chint) const {
    if (this->width() == dst.width() && this->height() == dst.height()) {
        return this->readPixels(dst, 0, 0, chint);
    }

    // Idea: If/when SkImageGenerator supports a native-scaling API (where the generator itself
    //       can scale more efficiently) we should take advantage of it here.
    //
    SkBitmap bm;
    if (as_IB(this)->getROPixels(&bm, chint)) {
        SkPixmap pmap;
        // Note: By calling the pixmap scaler, we never cache the final result, so the chint
        //       is (currently) only being applied to the getROPixels. If we get a request to
        //       also attempt to cache the final (scaled) result, we would add that logic here.
        //
        return bm.peekPixels(&pmap) && pmap.scalePixels(dst, quality);
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SkColorType SkImage::colorType() const { return fInfo.colorType(); }

SkAlphaType SkImage::alphaType() const { return fInfo.alphaType(); }

SkColorSpace* SkImage::colorSpace() const { return fInfo.colorSpace(); }

sk_sp<SkColorSpace> SkImage::refColorSpace() const { return fInfo.refColorSpace(); }

sk_sp<SkShader> SkImage::makeShader(SkTileMode tmx, SkTileMode tmy,
                                    const SkMatrix* localMatrix) const {
    return SkImageShader::Make(sk_ref_sp(const_cast<SkImage*>(this)), tmx, tmy, localMatrix,
                               SkImageShader::kInheritFromPaint);
}

sk_sp<SkShader> SkImage::makeShader(SkTileMode tmx, SkTileMode tmy,
                                    const SkFilterOptions& options,
                                    const SkMatrix* localMatrix) const {
    return SkImageShader::Make(sk_ref_sp(const_cast<SkImage*>(this)), tmx, tmy,
                               options, localMatrix);
}

sk_sp<SkShader> SkImage::makeShader(SkTileMode tmx, SkTileMode tmy,
                                    const SkMatrix* localMatrix, SkFilterQuality filtering) const {
    return SkImageShader::Make(sk_ref_sp(const_cast<SkImage*>(this)), tmx, tmy, localMatrix,
                               SkImageShader::FilterEnum(filtering));
}

sk_sp<SkData> SkImage::encodeToData(SkEncodedImageFormat type, int quality) const {
    SkBitmap bm;
    if (as_IB(this)->getROPixels(&bm)) {
        return SkEncodeBitmap(bm, type, quality);
    }
    return nullptr;
}

sk_sp<SkData> SkImage::encodeToData() const {
    if (auto encoded = this->refEncodedData()) {
        return encoded;
    }

    return this->encodeToData(SkEncodedImageFormat::kPNG, 100);
}

sk_sp<SkData> SkImage::refEncodedData() const {
    return sk_sp<SkData>(as_IB(this)->onRefEncoded());
}

sk_sp<SkImage> SkImage::MakeFromEncoded(sk_sp<SkData> encoded, const SkIRect* subset) {
    if (nullptr == encoded || 0 == encoded->size()) {
        return nullptr;
    }
    return SkImage::MakeFromGenerator(SkImageGenerator::MakeFromEncoded(std::move(encoded)),
                                      subset);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::makeSubset(const SkIRect& subset, GrDirectContext* direct) const {
    if (subset.isEmpty()) {
        return nullptr;
    }

    const SkIRect bounds = SkIRect::MakeWH(this->width(), this->height());
    if (!bounds.contains(subset)) {
        return nullptr;
    }

#if SK_SUPPORT_GPU
    auto myContext = as_IB(this)->context();
    // This check is also performed in the subclass, but we do it here for the short-circuit below.
    if (myContext && !myContext->priv().matches(direct)) {
        return nullptr;
    }
#endif

    // optimization : return self if the subset == our bounds
    if (bounds == subset) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }

    return as_IB(this)->onMakeSubset(subset, direct);
}

#if SK_SUPPORT_GPU

bool SkImage::isTextureBacked() const { return as_IB(this)->onIsTextureBacked(); }

GrBackendTexture SkImage::getBackendTexture(bool flushPendingGrContextIO,
                                            GrSurfaceOrigin* origin) const {
    return as_IB(this)->onGetBackendTexture(flushPendingGrContextIO, origin);
}

bool SkImage::isValid(GrRecordingContext* context) const {
    if (context && context->abandoned()) {
        return false;
    }
    return as_IB(this)->onIsValid(context);
}

bool SkImage::isValid(GrContext* context) const {
    return this->isValid(static_cast<GrRecordingContext*>(context));
}

GrSemaphoresSubmitted SkImage::flush(GrContext* context, const GrFlushInfo& flushInfo) {
    return as_IB(this)->onFlush(context, flushInfo);
}

void SkImage::flushAndSubmit(GrContext* context) {
    this->flush(context, {});
    context->submit();
}

#else

bool SkImage::isTextureBacked() const { return false; }

GrBackendTexture SkImage::getBackendTexture(bool flushPendingGrContextIO,
                                            GrSurfaceOrigin* origin) const {
    return GrBackendTexture(); // invalid
}

bool SkImage::isValid(GrRecordingContext* context) const {
    if (context) {
        return false;
    }
    return as_IB(this)->onIsValid(nullptr);
}

bool SkImage::isValid(GrContext* context) const {
    if (context) {
        return false;
    }
    return as_IB(this)->onIsValid(nullptr);
}

GrSemaphoresSubmitted SkImage::flush(GrContext*, const GrFlushInfo&) {
    return GrSemaphoresSubmitted::kNo;
}

void SkImage::flushAndSubmit(GrContext*) {}

#endif

///////////////////////////////////////////////////////////////////////////////

SkImage_Base::SkImage_Base(const SkImageInfo& info, uint32_t uniqueID)
        : INHERITED(info, uniqueID), fAddedToRasterCache(false) {}

SkImage_Base::~SkImage_Base() {
    if (fAddedToRasterCache.load()) {
        SkNotifyBitmapGenIDIsStale(this->uniqueID());
    }
}

void SkImage_Base::onAsyncRescaleAndReadPixels(const SkImageInfo& info,
                                               const SkIRect& origSrcRect,
                                               RescaleGamma rescaleGamma,
                                               SkFilterQuality rescaleQuality,
                                               ReadPixelsCallback callback,
                                               ReadPixelsContext context) {
    SkBitmap src;
    SkPixmap peek;
    SkIRect srcRect;
    if (this->peekPixels(&peek)) {
        src.installPixels(peek);
        srcRect = origSrcRect;
    } else {
        src.setInfo(this->imageInfo().makeDimensions(origSrcRect.size()));
        src.allocPixels();
        if (!this->readPixels(src.pixmap(), origSrcRect.x(), origSrcRect.y())) {
            callback(context, nullptr);
            return;
        }
        srcRect = SkIRect::MakeSize(src.dimensions());
    }
    return SkRescaleAndReadPixels(
            src, info, srcRect, rescaleGamma, rescaleQuality, callback, context);
}

void SkImage_Base::onAsyncRescaleAndReadPixelsYUV420(SkYUVColorSpace,
                                                     sk_sp<SkColorSpace> dstColorSpace,
                                                     const SkIRect& srcRect,
                                                     const SkISize& dstSize,
                                                     RescaleGamma,
                                                     SkFilterQuality,
                                                     ReadPixelsCallback callback,
                                                     ReadPixelsContext context) {
    // TODO: Call non-YUV asyncRescaleAndReadPixels and then make our callback convert to YUV and
    // call client's callback.
    callback(context, nullptr);
}

GrBackendTexture SkImage_Base::onGetBackendTexture(bool flushPendingGrContextIO,
                                                   GrSurfaceOrigin* origin) const {
    return GrBackendTexture(); // invalid
}

bool SkImage::readPixels(const SkPixmap& pmap, int srcX, int srcY, CachingHint chint) const {
    return this->readPixels(pmap.info(), pmap.writable_addr(), pmap.rowBytes(), srcX, srcY, chint);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::MakeFromBitmap(const SkBitmap& bm) {
    if (!bm.pixelRef()) {
        return nullptr;
    }

    return SkMakeImageFromRasterBitmap(bm, kIfMutable_SkCopyPixelsMode);
}

bool SkImage::asLegacyBitmap(SkBitmap* bitmap, LegacyBitmapMode ) const {
    return as_IB(this)->onAsLegacyBitmap(bitmap);
}

sk_sp<SkCachedData> SkImage_Base::getPlanes(SkYUVASizeInfo*, SkYUVAIndex[4],
                                            SkYUVColorSpace*, const void*[4]) {
    return nullptr;
}

bool SkImage_Base::onAsLegacyBitmap(SkBitmap* bitmap) const {
    // As the base-class, all we can do is make a copy (regardless of mode).
    // Subclasses that want to be more optimal should override.
    SkImageInfo info = fInfo.makeColorType(kN32_SkColorType).makeColorSpace(nullptr);
    if (!bitmap->tryAllocPixels(info)) {
        return false;
    }
    if (!this->readPixels(bitmap->info(), bitmap->getPixels(), bitmap->rowBytes(), 0, 0)) {
        bitmap->reset();
        return false;
    }

    bitmap->setImmutable();
    return true;
}

sk_sp<SkImage> SkImage::MakeFromPicture(sk_sp<SkPicture> picture, const SkISize& dimensions,
                                        const SkMatrix* matrix, const SkPaint* paint,
                                        BitDepth bitDepth, sk_sp<SkColorSpace> colorSpace) {
    return MakeFromGenerator(SkImageGenerator::MakeFromPicture(dimensions, std::move(picture),
                                                               matrix, paint, bitDepth,
                                                               std::move(colorSpace)));
}

sk_sp<SkImage> SkImage::makeWithFilter(const SkImageFilter* filter, const SkIRect& subset,
                                       const SkIRect& clipBounds, SkIRect* outSubset,
                                       SkIPoint* offset) const {
    GrContext* context = as_IB(this)->context();

    return this->makeWithFilter(context, filter, subset, clipBounds, outSubset, offset);
}

sk_sp<SkImage> SkImage::makeWithFilter(GrContext* grContext,
                                       const SkImageFilter* filter, const SkIRect& subset,
                                       const SkIRect& clipBounds, SkIRect* outSubset,
                                       SkIPoint* offset) const {
    if (!filter || !outSubset || !offset || !this->bounds().contains(subset)) {
        return nullptr;
    }
    sk_sp<SkSpecialImage> srcSpecialImage =
#if SK_SUPPORT_GPU
        SkSpecialImage::MakeFromImage(grContext, subset, sk_ref_sp(const_cast<SkImage*>(this)));
#else
        SkSpecialImage::MakeFromImage(nullptr, subset, sk_ref_sp(const_cast<SkImage*>(this)));
#endif
    if (!srcSpecialImage) {
        return nullptr;
    }

    sk_sp<SkImageFilterCache> cache(
        SkImageFilterCache::Create(SkImageFilterCache::kDefaultTransientSize));

    // The filters operate in the local space of the src image, where (0,0) corresponds to the
    // subset's top left corner. But the clip bounds and any crop rects on the filters are in the
    // original coordinate system, so configure the CTM to correct crop rects and explicitly adjust
    // the clip bounds (since it is assumed to already be in image space).
    SkImageFilter_Base::Context context(SkMatrix::Translate(-subset.x(), -subset.y()),
                                        clipBounds.makeOffset(-subset.topLeft()),
                                        cache.get(), fInfo.colorType(), fInfo.colorSpace(),
                                        srcSpecialImage.get());

    sk_sp<SkSpecialImage> result = as_IFB(filter)->filterImage(context).imageAndOffset(offset);
    if (!result) {
        return nullptr;
    }

    // The output image and offset are relative to the subset rectangle, so the offset needs to
    // be shifted to put it in the correct spot with respect to the original coordinate system
    offset->fX += subset.x();
    offset->fY += subset.y();

    // Final clip against the exact clipBounds (the clip provided in the context gets adjusted
    // to account for pixel-moving filters so doesn't always exactly match when finished). The
    // clipBounds are translated into the clippedDstRect coordinate space, including the
    // result->subset() ensures that the result's image pixel origin does not affect results.
    SkIRect dstRect = result->subset();
    SkIRect clippedDstRect = dstRect;
    if (!clippedDstRect.intersect(clipBounds.makeOffset(result->subset().topLeft() - *offset))) {
        return nullptr;
    }

    // Adjust the geometric offset if the top-left corner moved as well
    offset->fX += (clippedDstRect.x() - dstRect.x());
    offset->fY += (clippedDstRect.y() - dstRect.y());
    *outSubset = clippedDstRect;
    return result->asImage();
}

bool SkImage::isLazyGenerated() const {
    return as_IB(this)->onIsLazyGenerated();
}

bool SkImage::isAlphaOnly() const { return SkColorTypeIsAlphaOnly(fInfo.colorType()); }

sk_sp<SkImage> SkImage::makeColorSpace(sk_sp<SkColorSpace> target, GrDirectContext* direct) const {
    return this->makeColorTypeAndColorSpace(this->colorType(), std::move(target), direct);
}

sk_sp<SkImage> SkImage::makeColorTypeAndColorSpace(SkColorType targetColorType,
                                                   sk_sp<SkColorSpace> targetColorSpace,
                                                   GrDirectContext* direct) const {
    if (kUnknown_SkColorType == targetColorType || !targetColorSpace) {
        return nullptr;
    }

#if SK_SUPPORT_GPU
    auto myContext = as_IB(this)->context();
#ifdef SK_IMAGE_MAKE_COLOR_TYPE_AND_SPACE_USE_SOURCE_CONTEXT
    if (!direct) {
        direct = GrAsDirectContext(myContext);
    }
#endif
    // This check is also performed in the subclass, but we do it here for the short-circuit below.
    if (myContext && !myContext->priv().matches(direct)) {
        return nullptr;
    }
#endif

    SkColorType colorType = this->colorType();
    SkColorSpace* colorSpace = this->colorSpace();
    if (!colorSpace) {
        colorSpace = sk_srgb_singleton();
    }
    if (colorType == targetColorType &&
        (SkColorSpace::Equals(colorSpace, targetColorSpace.get()) || this->isAlphaOnly())) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }

    return as_IB(this)->onMakeColorTypeAndColorSpace(targetColorType,
                                                     std::move(targetColorSpace), direct);
}

sk_sp<SkImage> SkImage::reinterpretColorSpace(sk_sp<SkColorSpace> target) const {
    if (!target) {
        return nullptr;
    }

    // No need to create a new image if:
    // (1) The color spaces are equal.
    // (2) The color type is kAlpha8.
    SkColorSpace* colorSpace = this->colorSpace();
    if (!colorSpace) {
        colorSpace = sk_srgb_singleton();
    }
    if (SkColorSpace::Equals(colorSpace, target.get()) || this->isAlphaOnly()) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }

    return as_IB(this)->onReinterpretColorSpace(std::move(target));
}

sk_sp<SkImage> SkImage::makeNonTextureImage() const {
    if (!this->isTextureBacked()) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }
    return this->makeRasterImage();
}

sk_sp<SkImage> SkImage::makeRasterImage(CachingHint chint) const {
    SkPixmap pm;
    if (this->peekPixels(&pm)) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }

    const size_t rowBytes = fInfo.minRowBytes();
    size_t size = fInfo.computeByteSize(rowBytes);
    if (SkImageInfo::ByteSizeOverflowed(size)) {
        return nullptr;
    }

    sk_sp<SkData> data = SkData::MakeUninitialized(size);
    pm = {fInfo.makeColorSpace(nullptr), data->writable_data(), fInfo.minRowBytes()};
    if (!this->readPixels(pm, 0, 0, chint)) {
        return nullptr;
    }

    return SkImage::MakeRasterData(fInfo, std::move(data), rowBytes);
}

//////////////////////////////////////////////////////////////////////////////////////

#if !SK_SUPPORT_GPU

sk_sp<SkImage> SkImage::DecodeToTexture(GrDirectContext*, const void*, size_t, const SkIRect*) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromTexture(GrContext* ctx,
                                        const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                        SkColorType ct, SkAlphaType at, sk_sp<SkColorSpace> cs,
                                        TextureReleaseProc releaseP, ReleaseContext releaseC) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromCompressedTexture(GrContext* ctx,
                                                  const GrBackendTexture& tex,
                                                  GrSurfaceOrigin origin,
                                                  SkAlphaType at,
                                                  sk_sp<SkColorSpace> cs,
                                                  TextureReleaseProc releaseP,
                                                  ReleaseContext releaseC) {
    return nullptr;
}

bool SkImage::MakeBackendTextureFromSkImage(GrContext*,
                                            sk_sp<SkImage>,
                                            GrBackendTexture*,
                                            BackendTextureReleaseProc*) {
    return false;
}

sk_sp<SkImage> SkImage::MakeFromAdoptedTexture(GrContext* ctx,
                                               const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                               SkColorType ct, SkAlphaType at,
                                               sk_sp<SkColorSpace> cs) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopy(GrContext* context,
                                                 SkYUVColorSpace yuvColorSpace,
                                                 const GrBackendTexture yuvaTextures[],
                                                 const SkYUVAIndex yuvaIndices[4],
                                                 SkISize imageSize,
                                                 GrSurfaceOrigin imageOrigin,
                                                 sk_sp<SkColorSpace> imageColorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopyWithExternalBackend(
        GrContext* context,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture yuvaTextures[],
        const SkYUVAIndex yuvaIndices[4],
        SkISize imageSize,
        GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> imageColorSpace,
        TextureReleaseProc textureReleaseProc,
        ReleaseContext releaseContext) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVTexturesCopyWithExternalBackend(
        GrContext* context, SkYUVColorSpace yuvColorSpace, const GrBackendTexture yuvTextures[3],
        GrSurfaceOrigin surfaceOrigin, const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> colorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromNV12TexturesCopy(GrContext* ctx, SkYUVColorSpace space,
                                                const GrBackendTexture[2],
                                                GrSurfaceOrigin origin,
                                                sk_sp<SkColorSpace> imageColorSpace) {
    return nullptr;
}

#ifndef SK_IMAGE_MAKE_TEXTURE_IMAGE_ALLOW_GR_CONTEXT
sk_sp<SkImage> SkImage::makeTextureImage(GrDirectContext*, GrMipMapped, SkBudgeted) const {
    return nullptr;
}
#else
sk_sp<SkImage> SkImage::makeTextureImage(GrContext*, GrMipMapped, SkBudgeted) const {
    return nullptr;
}
#endif

sk_sp<SkImage> SkImage::MakeFromNV12TexturesCopyWithExternalBackend(
                                                           GrContext* context,
                                                           SkYUVColorSpace yuvColorSpace,
                                                           const GrBackendTexture nv12Textures[2],
                                                           GrSurfaceOrigin imageOrigin,
                                                           const GrBackendTexture& backendTexture,
                                                           sk_sp<SkColorSpace> imageColorSpace,
                                                           TextureReleaseProc textureReleaseProc,
                                                           ReleaseContext releaseContext) {
    return nullptr;
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkImage_pinAsTexture(const SkImage* image, GrRecordingContext* rContext) {
    SkASSERT(image);
    SkASSERT(rContext);
    return as_IB(image)->onPinAsTexture(rContext);
}

void SkImage_unpinAsTexture(const SkImage* image, GrRecordingContext* rContext) {
    SkASSERT(image);
    SkASSERT(rContext);
    as_IB(image)->onUnpinAsTexture(rContext);
}

SkIRect SkImage_getSubset(const SkImage* image) {
    SkASSERT(image);
    return as_IB(image)->onGetSubset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SkMipmapBuilder::SkMipmapBuilder(const SkImageInfo& info) {
    fMM = sk_sp<SkMipmap>(SkMipmap::Build({info, nullptr, 0}, nullptr, false));
}

SkMipmapBuilder::~SkMipmapBuilder() {}

int SkMipmapBuilder::countLevels() const {
    return fMM ? fMM->countLevels() : 0;
}

SkPixmap SkMipmapBuilder::level(int index) const {
    SkPixmap pm;

    SkMipmap::Level level;
    if (fMM && fMM->getLevel(index, &level)) {
        pm = level.fPixmap;
    }
    return pm;
}

sk_sp<SkMipmap> SkMipmapBuilder::detach() {
    return std::move(fMM);
}

bool SkImage::hasMipmaps() const {
    return as_IB(this)->onPeekMips() != nullptr;
}

sk_sp<SkImage> SkImage::withMipmaps(sk_sp<SkMipmap> data) const {
    auto result = as_IB(this)->onMakeWithMipmaps(std::move(data));
    if (!result) {
        result = sk_ref_sp((const_cast<SkImage*>(this)));
    }
    return result;
}
