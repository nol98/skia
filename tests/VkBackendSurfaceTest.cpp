/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This is a GPU-backend specific test. It relies on static intializers to work

#include "SkTypes.h"

#if SK_SUPPORT_GPU && defined(SK_VULKAN)

#include "GrTest.h"
#include "Test.h"

#include "GrBackendSurface.h"
#include "GrContextPriv.h"
#include "GrTextureProxy.h"
#include "GrTexture.h"
#include "SkImage.h"
#include "SkImage_Base.h"
#include "vk/GrVkGpu.h"
#include "vk/GrVkImageLayout.h"
#include "vk/GrVkTexture.h"
#include "vk/GrVkTypes.h"

DEF_GPUTEST_FOR_VULKAN_CONTEXT(VkImageLayoutTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();
    GrVkGpu* gpu = static_cast<GrVkGpu*>(context->contextPriv().getGpu());

    GrBackendTexture backendTex = gpu->createTestingOnlyBackendTexture(nullptr, 1, 1,
                                                                       kRGBA_8888_GrPixelConfig,
                                                                       false,
                                                                       GrMipMapped::kNo);
    REPORTER_ASSERT(reporter, backendTex.isValid());

    GrVkImageInfo info;
    REPORTER_ASSERT(reporter, backendTex.getVkImageInfo(&info));
    VkImageLayout initLayout = info.fImageLayout;

    // Verify that setting that layout via a copy of a backendTexture is reflected in all the
    // backendTextures.
    GrBackendTexture backendTexCopy = backendTex;
    REPORTER_ASSERT(reporter, backendTexCopy.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, initLayout == info.fImageLayout);

    backendTexCopy.setVkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    REPORTER_ASSERT(reporter, backendTex.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == info.fImageLayout);

    REPORTER_ASSERT(reporter, backendTexCopy.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == info.fImageLayout);

    // Setting back the layout since we didn't actually change it
    backendTex.setVkImageLayout(initLayout);

    sk_sp<SkImage> wrappedImage = SkImage::MakeFromTexture(context, backendTex,
                                                           kTopLeft_GrSurfaceOrigin,
                                                           kRGBA_8888_SkColorType,
                                                           kPremul_SkAlphaType, nullptr);
    REPORTER_ASSERT(reporter, wrappedImage.get());

    sk_sp<GrTextureProxy> texProxy = as_IB(wrappedImage)->asTextureProxyRef();
    REPORTER_ASSERT(reporter, texProxy.get());
    REPORTER_ASSERT(reporter, texProxy->priv().isInstantiated());
    GrTexture* texture = texProxy->priv().peekTexture();
    REPORTER_ASSERT(reporter, texture);

    // Verify that modifying the layout via the GrVkTexture is reflected in the GrBackendTexture
    GrVkTexture* vkTexture = static_cast<GrVkTexture*>(texture);
    REPORTER_ASSERT(reporter, initLayout == vkTexture->currentLayout());
    vkTexture->updateImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    REPORTER_ASSERT(reporter, backendTex.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL == info.fImageLayout);

    GrBackendTexture backendTexImage = wrappedImage->getBackendTexture(false);
    REPORTER_ASSERT(reporter, backendTexImage.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL == info.fImageLayout);

    // Verify that modifying the layout via the GrBackendTexutre is reflected in the GrVkTexture
    backendTexImage.setVkImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == vkTexture->currentLayout());

#ifdef SK_SUPPORT_LEGACY_BACKEND_OBJECTS
    // Verify that modifying the layout via the old textureHandle sitll works in is reflected in the
    // GrVkTexture and GrBackendTexture.
    GrVkImageInfo* backendInfo = (GrVkImageInfo*)wrappedImage->getTextureHandle(false);
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == backendInfo->fImageLayout);

    backendInfo->updateImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    REPORTER_ASSERT(reporter,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL == vkTexture->currentLayout());
    REPORTER_ASSERT(reporter, backendTexImage.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL == info.fImageLayout);
#endif

    vkTexture->updateImageLayout(initLayout);

    REPORTER_ASSERT(reporter, backendTex.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, initLayout == info.fImageLayout);

    REPORTER_ASSERT(reporter, backendTexCopy.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, initLayout == info.fImageLayout);

    REPORTER_ASSERT(reporter, backendTexImage.getVkImageInfo(&info));
    REPORTER_ASSERT(reporter, initLayout == info.fImageLayout);

    // Check that we can do things like assigning the backend texture to invalid one, assign an
    // invalid one, assin a backend texture to inself etc. Success here is that we don't hit any of
    // our ref counting asserts.
    REPORTER_ASSERT(reporter, GrBackendTexture::TestingOnly_Equals(backendTex, backendTexCopy));

    GrBackendTexture invalidTexture;
    REPORTER_ASSERT(reporter, !invalidTexture.isValid());
    REPORTER_ASSERT(reporter, !GrBackendTexture::TestingOnly_Equals(invalidTexture, backendTexCopy));

    backendTexCopy = invalidTexture;
    REPORTER_ASSERT(reporter, !backendTexCopy.isValid());
    REPORTER_ASSERT(reporter, !GrBackendTexture::TestingOnly_Equals(invalidTexture, backendTexCopy));

    invalidTexture = backendTex;
    REPORTER_ASSERT(reporter, invalidTexture.isValid());
    REPORTER_ASSERT(reporter, GrBackendTexture::TestingOnly_Equals(invalidTexture, backendTex));

    invalidTexture = invalidTexture;
    REPORTER_ASSERT(reporter, invalidTexture.isValid());
    REPORTER_ASSERT(reporter, GrBackendTexture::TestingOnly_Equals(invalidTexture, invalidTexture));

    gpu->deleteTestingOnlyBackendTexture(backendTex);
}

#endif
