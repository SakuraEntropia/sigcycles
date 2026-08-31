/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Gaussian wavefront (beam cross-section amplitude model).
 *
 * Ported (units stripped) from wave_tracer include/wt/beam/gaussian_wavefront.hpp
 * and include/wt/math/distribution/gaussian2d.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 */

#pragma once

#include "util/math_float2.h"

#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* 2D Gaussian intensity distribution. stddev in scene units. */
struct wav_gaussian2 {
  float2 stddev;
};

ccl_device_inline bool wav_gaussian_is_dirac(const wav_gaussian2 g)
{
  return g.stddev.x == 0.0f || g.stddev.y == 0.0f;
}

/* PDF of the intensity distribution at x (scene units). */
ccl_device_inline float wav_gaussian_pdf(const wav_gaussian2 g, const float2 x)
{
  if (wav_gaussian_is_dirac(g))
    return 1.0f;
  const float sx = g.stddev.x, sy = g.stddev.y;
  const float ex = -0.5f * wav_sqr(x.x / sx) - 0.5f * wav_sqr(x.y / sy);
  return expf(ex) / (WAV_TWO_PI_F * sx * sy);
}

/* Field amplitude magnitude at x: sqrt of the intensity PDF. */
ccl_device_inline float wav_gaussian_amplitude(const wav_gaussian2 g, const float2 x)
{
  return sqrtf(wav_gaussian_pdf(g, x));
}

/* Beam envelope half-extents (3 sigma). */
ccl_device_inline float2 wav_gaussian_envelope(const wav_gaussian2 g)
{
  return make_float2(g.stddev.x * 3.0f, g.stddev.y * 3.0f);
}

}  // namespace waveoptics

CCL_NAMESPACE_END
