/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrDualIntervalGradientColorizer.fp; do not modify.
 **************************************************************************************************/
#include "GrDualIntervalGradientColorizer.h"
#include "glsl/GrGLSLFragmentProcessor.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLProgramBuilder.h"
#include "GrTexture.h"
#include "SkSLCPP.h"
#include "SkSLUtil.h"
class GrGLSLDualIntervalGradientColorizer : public GrGLSLFragmentProcessor {
public:
    GrGLSLDualIntervalGradientColorizer() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrDualIntervalGradientColorizer& _outer =
                args.fFp.cast<GrDualIntervalGradientColorizer>();
        (void)_outer;
        auto scale01 = _outer.scale01();
        (void)scale01;
        auto bias01 = _outer.bias01();
        (void)bias01;
        auto scale23 = _outer.scale23();
        (void)scale23;
        auto bias23 = _outer.bias23();
        (void)bias23;
        auto threshold = _outer.threshold();
        (void)threshold;
        fScale01Var = args.fUniformHandler->addUniform(kFragment_GrShaderFlag, kFloat4_GrSLType,
                                                       kDefault_GrSLPrecision, "scale01");
        fBias01Var = args.fUniformHandler->addUniform(kFragment_GrShaderFlag, kFloat4_GrSLType,
                                                      kDefault_GrSLPrecision, "bias01");
        fScale23Var = args.fUniformHandler->addUniform(kFragment_GrShaderFlag, kFloat4_GrSLType,
                                                       kDefault_GrSLPrecision, "scale23");
        fBias23Var = args.fUniformHandler->addUniform(kFragment_GrShaderFlag, kFloat4_GrSLType,
                                                      kDefault_GrSLPrecision, "bias23");
        fThresholdVar = args.fUniformHandler->addUniform(kFragment_GrShaderFlag, kHalf_GrSLType,
                                                         kDefault_GrSLPrecision, "threshold");
        fragBuilder->codeAppendf(
                "half t = %s.x;\nfloat4 scale, bias;\nif (t < %s) {\n    scale = %s;\n    bias = "
                "%s;\n} else {\n    scale = %s;\n    bias = %s;\n}\n%s = half4(float(t) * scale + "
                "bias);\n",
                args.fInputColor, args.fUniformHandler->getUniformCStr(fThresholdVar),
                args.fUniformHandler->getUniformCStr(fScale01Var),
                args.fUniformHandler->getUniformCStr(fBias01Var),
                args.fUniformHandler->getUniformCStr(fScale23Var),
                args.fUniformHandler->getUniformCStr(fBias23Var), args.fOutputColor);
    }

private:
    void onSetData(const GrGLSLProgramDataManager& pdman,
                   const GrFragmentProcessor& _proc) override {
        const GrDualIntervalGradientColorizer& _outer =
                _proc.cast<GrDualIntervalGradientColorizer>();
        {
            const GrColor4f& scale01Value = _outer.scale01();
            if (fScale01Prev != scale01Value) {
                fScale01Prev = scale01Value;
                pdman.set4fv(fScale01Var, 1, scale01Value.fRGBA);
            }
            const GrColor4f& bias01Value = _outer.bias01();
            if (fBias01Prev != bias01Value) {
                fBias01Prev = bias01Value;
                pdman.set4fv(fBias01Var, 1, bias01Value.fRGBA);
            }
            const GrColor4f& scale23Value = _outer.scale23();
            if (fScale23Prev != scale23Value) {
                fScale23Prev = scale23Value;
                pdman.set4fv(fScale23Var, 1, scale23Value.fRGBA);
            }
            const GrColor4f& bias23Value = _outer.bias23();
            if (fBias23Prev != bias23Value) {
                fBias23Prev = bias23Value;
                pdman.set4fv(fBias23Var, 1, bias23Value.fRGBA);
            }
            float thresholdValue = _outer.threshold();
            if (fThresholdPrev != thresholdValue) {
                fThresholdPrev = thresholdValue;
                pdman.set1f(fThresholdVar, thresholdValue);
            }
        }
    }
    GrColor4f fScale01Prev = GrColor4f::kIllegalConstructor;
    GrColor4f fBias01Prev = GrColor4f::kIllegalConstructor;
    GrColor4f fScale23Prev = GrColor4f::kIllegalConstructor;
    GrColor4f fBias23Prev = GrColor4f::kIllegalConstructor;
    float fThresholdPrev = SK_FloatNaN;
    UniformHandle fScale01Var;
    UniformHandle fBias01Var;
    UniformHandle fScale23Var;
    UniformHandle fBias23Var;
    UniformHandle fThresholdVar;
};
GrGLSLFragmentProcessor* GrDualIntervalGradientColorizer::onCreateGLSLInstance() const {
    return new GrGLSLDualIntervalGradientColorizer();
}
void GrDualIntervalGradientColorizer::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                            GrProcessorKeyBuilder* b) const {}
bool GrDualIntervalGradientColorizer::onIsEqual(const GrFragmentProcessor& other) const {
    const GrDualIntervalGradientColorizer& that = other.cast<GrDualIntervalGradientColorizer>();
    (void)that;
    if (fScale01 != that.fScale01) return false;
    if (fBias01 != that.fBias01) return false;
    if (fScale23 != that.fScale23) return false;
    if (fBias23 != that.fBias23) return false;
    if (fThreshold != that.fThreshold) return false;
    return true;
}
GrDualIntervalGradientColorizer::GrDualIntervalGradientColorizer(
        const GrDualIntervalGradientColorizer& src)
        : INHERITED(kGrDualIntervalGradientColorizer_ClassID, src.optimizationFlags())
        , fScale01(src.fScale01)
        , fBias01(src.fBias01)
        , fScale23(src.fScale23)
        , fBias23(src.fBias23)
        , fThreshold(src.fThreshold) {}
std::unique_ptr<GrFragmentProcessor> GrDualIntervalGradientColorizer::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrDualIntervalGradientColorizer(*this));
}

std::unique_ptr<GrFragmentProcessor> GrDualIntervalGradientColorizer::Make(const GrColor4f& c0,
                                                                           const GrColor4f& c1,
                                                                           const GrColor4f& c2,
                                                                           const GrColor4f& c3,
                                                                           float threshold) {
    // Derive scale and biases from the 4 colors and threshold
    auto vc0 = Sk4f::Load(c0.fRGBA);
    auto vc1 = Sk4f::Load(c1.fRGBA);
    auto scale01 = (vc1 - vc0) / threshold;
    // bias01 = c0

    auto vc2 = Sk4f::Load(c2.fRGBA);
    auto vc3 = Sk4f::Load(c3.fRGBA);
    auto scale23 = (vc3 - vc2) / (1 - threshold);
    auto bias23 = vc2 - threshold * scale23;

    return std::unique_ptr<GrFragmentProcessor>(new GrDualIntervalGradientColorizer(
            GrColor4f(scale01[0], scale01[1], scale01[2], scale01[3]), c0,
            GrColor4f(scale23[0], scale23[1], scale23[2], scale23[3]),
            GrColor4f(bias23[0], bias23[1], bias23[2], bias23[3]), threshold));
}