/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontMgr.h"
#include "SkFontMgr_custom.h"

#ifndef SK_FONT_FILE_PREFIX
#    define SK_FONT_FILE_PREFIX "/usr/share/fonts/"
#endif

#ifdef SK_LEGACY_FONTMGR_FACTORY
SkFontMgr* SkFontMgr::Factory() {
#else
sk_sp<SkFontMgr> SkFontMgr::Factory() {
#endif
    return SkFontMgr_New_Custom_Directory(SK_FONT_FILE_PREFIX);
}