/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Scalar/vector math helpers for the wave optics module.
 *
 * Ported from wave_tracer include/wt/math/common.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 */

#pragma once

#include "util/math_float2.h"
#include "util/math_float3.h"

#include "waveoptics/wav_complex.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

#define WAV_PI_F 3.14159265358979323846f
#define WAV_PI_2_F 1.57079632679489661923f
#define WAV_TWO_PI_F 6.28318530717958647692f
#define WAV_INV_TWO_PI_F 0.15915494309189533577f
#define WAV_SQRT_TWO_F 1.41421356237309504880f
#define WAV_INV_SQRT_TWO_F 0.70710678118654752440f
#define WAV_SQRT_PI_2_F 1.25331413731550025121f
#define WAV_INV_SQRT_TWO_PI_F 0.39894228040143267794f
#define WAV_INV_PI_F 0.31830988618379067154f
#define WAV_PI_4_F 0.78539816339744830962f
#define WAV_INV_SQRT_12_F 0.28867513459481288225f

ccl_device_inline float wav_sqr(const float x)
{
  return x * x;
}

/* Normalized sinc: sin(x)/x. */
ccl_device_inline float wav_sinc(const float x)
{
  if (fabsf(x) < 1e-6f)
    return 1.0f;
  return sinf(x) / x;
}

ccl_device_inline float wav_clamp01(const float x)
{
  return clamp(x, 0.0f, 1.0f);
}

ccl_device_inline float wav_lerp(const float a, const float b, const float t)
{
  return (1.0f - t) * a + t * b;
}

ccl_device_inline float2 wav_mix2(const float2 a, const float2 b, const float t)
{
  return make_float2(wav_lerp(a.x, b.x, t), wav_lerp(a.y, b.y, t));
}

ccl_device_inline float wav_length2(const float2 v)
{
  return dot(v, v);
}

ccl_device_inline float wav_length2(const float3 v)
{
  return dot(v, v);
}

ccl_device_inline float wav_mod(const float x, const float y)
{
  const float r = fmodf(x, y);
  return r < 0.0f ? r + y : r;
}

/* 2x2 matrix stored by columns. */
struct wav_mat2 {
  float2 c0, c1;
};

ccl_device_inline float2 wav_mat2_mul(const wav_mat2 m, const float2 v)
{
  return make_float2(m.c0.x * v.x + m.c1.x * v.y, m.c0.y * v.x + m.c1.y * v.y);
}

ccl_device_inline wav_mat2 wav_mat2_inverse(const wav_mat2 m)
{
  const float det = m.c0.x * m.c1.y - m.c0.y * m.c1.x;
  wav_mat2 r;
  r.c0 = make_float2(m.c1.y / det, -m.c0.y / det);
  r.c1 = make_float2(-m.c1.x / det, m.c0.x / det);
  return r;
}

}  // namespace waveoptics

CCL_NAMESPACE_END
