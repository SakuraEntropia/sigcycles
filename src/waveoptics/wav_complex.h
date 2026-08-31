/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Complex number type used by the wave optics module.
 *
 * The wave optics module ports mathematics from wave_tracer
 * (https://github.com/ssteinberg/wave_tracer), Copyright Shlomi Steinberg,
 * licensed under CC BY-NC 4.0. See README.md in this directory for the
 * licensing note and attribution.
 */

#pragma once

#include "util/math_float2.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Minimal complex number type, GPU friendly (no std::complex). */
struct wav_complex {
  float re, im;
};

ccl_device_inline wav_complex wav_make_complex(float re, float im)
{
  wav_complex c;
  c.re = re;
  c.im = im;
  return c;
}

ccl_device_inline wav_complex wav_complex_add(const wav_complex a, const wav_complex b)
{
  return wav_make_complex(a.re + b.re, a.im + b.im);
}

ccl_device_inline wav_complex wav_complex_sub(const wav_complex a, const wav_complex b)
{
  return wav_make_complex(a.re - b.re, a.im - b.im);
}

ccl_device_inline wav_complex wav_complex_mul(const wav_complex a, const wav_complex b)
{
  return wav_make_complex(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

ccl_device_inline wav_complex wav_complex_mul_real(const wav_complex a, const float s)
{
  return wav_make_complex(a.re * s, a.im * s);
}

ccl_device_inline wav_complex wav_complex_conj(const wav_complex a)
{
  return wav_make_complex(a.re, -a.im);
}

/* |z|^2 */
ccl_device_inline float wav_complex_norm(const wav_complex a)
{
  return a.re * a.re + a.im * a.im;
}

ccl_device_inline wav_complex wav_complex_add_real(const wav_complex a, const float s)
{
  return wav_make_complex(a.re + s, a.im);
}

ccl_device_inline wav_complex wav_complex_sub_real(const wav_complex a, const float s)
{
  return wav_make_complex(a.re - s, a.im);
}

ccl_device_inline wav_complex wav_complex_div_real(const wav_complex a, const float s)
{
  return wav_make_complex(a.re / s, a.im / s);
}

/* complex division a/b */
ccl_device_inline wav_complex wav_complex_div(const wav_complex a, const wav_complex b)
{
  const float d = wav_complex_norm(b);
  return wav_make_complex((a.re * b.re + a.im * b.im) / d,
                          (a.im * b.re - a.re * b.im) / d);
}

ccl_device_inline float wav_complex_abs(const wav_complex a)
{
  return sqrtf(wav_complex_norm(a));
}

/* exp(z) */
ccl_device_inline wav_complex wav_complex_exp(const wav_complex a)
{
  const float e = expf(a.re);
  return wav_make_complex(e * cosf(a.im), e * sinf(a.im));
}

/* polar(r, theta) = r * (cos theta + i sin theta) */
ccl_device_inline wav_complex wav_complex_polar(const float r, const float theta)
{
  return wav_make_complex(r * cosf(theta), r * sinf(theta));
}

/* complex sqrt, principal branch. */
ccl_device_inline wav_complex wav_complex_sqrt(const wav_complex a)
{
  const float m = wav_complex_abs(a);
  const float t = sqrtf(fmaxf(0.0f, 0.5f * (m + a.re)));
  const float u = sqrtf(fmaxf(0.0f, 0.5f * (m - a.re)));
  return wav_make_complex(t, a.im < 0.0f ? -u : u);
}

}  // namespace waveoptics

CCL_NAMESPACE_END
