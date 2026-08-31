/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Complex complementary error function, needed by the UTD transition
 * function F(x) of the wave optics module.
 *
 * Implemented with the Taylor series for small |z| and the Gauss
 * continued fraction for large |z| (DLMF 7.5/7.9), so no external
 * dependency (wave_tracer uses libcerf here).
 *
 * Ported from wave_tracer include/wt/interaction/fsd/utd.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 */

#pragma once

#include "util/math_float2.h"

#include "waveoptics/wav_complex.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* erfc(z) for complex z. Series for |z| < 8, continued fraction otherwise. */
ccl_device_inline wav_complex wav_cerfc(const wav_complex z)
{
  /* Reflect to the right half plane: erfc(-z) = 2 - erfc(z). */
  const bool reflect = (z.re < 0.0f);
  const wav_complex zz = reflect ? wav_make_complex(-z.re, -z.im) : z;

  const float z2 = wav_complex_norm(zz);
  const float absz = sqrtf(z2);

  wav_complex erfc;

  if (absz < 8.0f) {
    /* Taylor series: erf(z) = (2/sqrt(pi)) * sum_n (-1)^n z^(2n+1) / (n! (2n+1)).
     * Iterate term by term. */
    const float inv_sqrt_pi = 0.56418958354775628694f;
    wav_complex term = zz; /* z^(2n+1) / (n! (2n+1)) for n=0 */
    wav_complex sum = term;
    int n = 0;
    /* term_{n+1} = term_n * z^2 * (2n+1) / ((n+1)*(2n+3)). */
    while (n < 256) {
      const float fn = (float)n;
      const float scale = (2.0f * fn + 1.0f) / ((fn + 1.0f) * (2.0f * fn + 3.0f));
      term = wav_complex_mul(term, zz);
      term = wav_complex_mul(term, zz);
      term = wav_complex_mul_real(term, scale);
      const int nn = n + 1;
      sum = wav_complex_add(sum, (nn & 1) ? wav_complex_mul_real(term, -1.0f) : term);
      if (wav_complex_norm(term) < 1e-18f * wav_complex_norm(sum))
        break;
      ++n;
    }
    const wav_complex erf = wav_complex_mul_real(sum, 2.0f * inv_sqrt_pi);
    erfc = wav_make_complex(1.0f - erf.re, -erf.im);
  }
  else {
    /* Gauss continued fraction (DLMF 7.9.2):
     * erfc(z) = (1/sqrt(pi)) e^{-z^2} / ( z + (1/2)/(z + 1/(z + (3/2)/(z + ...))) )
     * Evaluate with backward recurrence. */
    const float inv_sqrt_pi = 0.56418958354775628694f;
    const int iters = (int)(2.0f * absz + 10.0f);
    /* Backward recurrence: start from the tail. */
    wav_complex f = wav_make_complex(0.0f, 0.0f); /* tail value */
    for (int i = iters; i >= 1; --i) {
      const float an = 0.5f * (float)i; /* a_i = i/2 */
      /* f = an / (z + f) */
      const wav_complex denom = wav_complex_add(zz, f);
      f = wav_complex_mul_real(wav_make_complex(
                                   denom.re, denom.im),
                               1.0f);
      /* complex reciprocal */
      const float d2 = wav_complex_norm(denom);
      f = wav_make_complex(an * denom.re / d2, -an * denom.im / d2);
    }
    /* erfc = (1/sqrt(pi)) e^{-z^2} / (z + f) */
    const wav_complex denom = wav_complex_add(zz, f);
    const float d2 = wav_complex_norm(denom);
    const wav_complex inv_denom = wav_make_complex(denom.re / d2, -denom.im / d2);
    const wav_complex ez2 = wav_complex_exp(wav_complex_mul_real(
        wav_complex_mul(zz, zz), -1.0f));
    erfc = wav_complex_mul_real(wav_complex_mul(ez2, inv_denom), inv_sqrt_pi);
  }

  return reflect ? wav_make_complex(2.0f - erfc.re, -erfc.im) : erfc;
}

}  // namespace waveoptics

CCL_NAMESPACE_END
