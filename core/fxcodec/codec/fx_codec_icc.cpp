// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <algorithm>
#include <memory>
#include <vector>

#include "core/fxcodec/codec/ccodec_iccmodule.h"
#include "core/fxcodec/codec/codec_int.h"

namespace {

bool Check3Components(cmsColorSpaceSignature cs) {
  switch (cs) {
    case cmsSigGrayData:
    case cmsSigCmykData:
      return false;
    default:
      return true;
  }
}

}  // namespace

CLcmsCmm::CLcmsCmm(int srcComponents,
                   cmsHTRANSFORM hTransform,
                   bool isLab,
                   bool bNormal)
    : m_hTransform(hTransform),
      m_nSrcComponents(srcComponents),
      m_bLab(isLab),
      m_bNormal(bNormal) {}

CLcmsCmm::~CLcmsCmm() {
  cmsDeleteTransform(m_hTransform);
}

CCodec_IccModule::CCodec_IccModule() : m_nComponents(0) {}

CCodec_IccModule::~CCodec_IccModule() {}

std::unique_ptr<CLcmsCmm> CCodec_IccModule::CreateTransform_sRGB(
    const unsigned char* pSrcProfileData,
    uint32_t dwSrcProfileSize,
    uint32_t* nSrcComponents) {
  *nSrcComponents = 0;
  cmsHPROFILE srcProfile =
      cmsOpenProfileFromMem(pSrcProfileData, dwSrcProfileSize);
  if (!srcProfile)
    return nullptr;

  cmsHPROFILE dstProfile;
  dstProfile = cmsCreate_sRGBProfile();
  if (!dstProfile) {
    cmsCloseProfile(srcProfile);
    return nullptr;
  }
  cmsColorSpaceSignature srcCS = cmsGetColorSpace(srcProfile);

  *nSrcComponents = cmsChannelsOf(srcCS);
  // According to PDF spec, number of components must be 1, 3, or 4.
  if (*nSrcComponents != 1 && *nSrcComponents != 3 && *nSrcComponents != 4) {
    cmsCloseProfile(srcProfile);
    cmsCloseProfile(dstProfile);
    return nullptr;
  }

  int srcFormat;
  bool bLab = false;
  bool bNormal = false;
  if (srcCS == cmsSigLabData) {
    srcFormat =
        COLORSPACE_SH(PT_Lab) | CHANNELS_SH(*nSrcComponents) | BYTES_SH(0);
    bLab = true;
  } else {
    srcFormat =
        COLORSPACE_SH(PT_ANY) | CHANNELS_SH(*nSrcComponents) | BYTES_SH(1);
    // TODO(thestig): Check to see if lcms2 supports more colorspaces that can
    // be considered normal.
    bNormal = srcCS == cmsSigGrayData || srcCS == cmsSigRgbData ||
              srcCS == cmsSigCmykData;
  }
  cmsColorSpaceSignature dstCS = cmsGetColorSpace(dstProfile);
  if (!Check3Components(dstCS)) {
    cmsCloseProfile(srcProfile);
    cmsCloseProfile(dstProfile);
    return nullptr;
  }

  cmsHTRANSFORM hTransform = nullptr;
  const int intent = 0;
  switch (dstCS) {
    case cmsSigRgbData:
      hTransform = cmsCreateTransform(srcProfile, srcFormat, dstProfile,
                                      TYPE_BGR_8, intent, 0);
      break;
    case cmsSigGrayData:
    case cmsSigCmykData:
      // Check3Components() already filtered these types.
      NOTREACHED();
      break;
    default:
      break;
  }
  if (!hTransform) {
    cmsCloseProfile(srcProfile);
    cmsCloseProfile(dstProfile);
    return nullptr;
  }
  auto pCmm =
      pdfium::MakeUnique<CLcmsCmm>(*nSrcComponents, hTransform, bLab, bNormal);
  cmsCloseProfile(srcProfile);
  cmsCloseProfile(dstProfile);
  return pCmm;
}

void CCodec_IccModule::Translate(CLcmsCmm* pTransform,
                                 const float* pSrcValues,
                                 float* pDestValues) {
  if (!pTransform)
    return;

  uint32_t nSrcComponents = m_nComponents;
  uint8_t output[4];
  // TODO(npm): Currently the CmsDoTransform method is part of LCMS and it will
  // apply some member of m_hTransform to the input. We need to go over all the
  // places which set transform to verify that only nSrcComponents are used.
  if (pTransform->m_bLab) {
    std::vector<double> inputs(std::max(nSrcComponents, 16u));
    for (uint32_t i = 0; i < nSrcComponents; ++i)
      inputs[i] = pSrcValues[i];
    cmsDoTransform(pTransform->m_hTransform, inputs.data(), output, 1);
  } else {
    std::vector<uint8_t> inputs(std::max(nSrcComponents, 16u));
    for (uint32_t i = 0; i < nSrcComponents; ++i) {
      inputs[i] =
          pdfium::clamp(static_cast<int>(pSrcValues[i] * 255.0f), 0, 255);
    }
    cmsDoTransform(pTransform->m_hTransform, inputs.data(), output, 1);
  }
  pDestValues[0] = output[2] / 255.0f;
  pDestValues[1] = output[1] / 255.0f;
  pDestValues[2] = output[0] / 255.0f;
}

void CCodec_IccModule::TranslateScanline(CLcmsCmm* pTransform,
                                         unsigned char* pDest,
                                         const unsigned char* pSrc,
                                         int32_t pixels) {
  if (pTransform)
    cmsDoTransform(pTransform->m_hTransform, pSrc, pDest, pixels);
}
