/* SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thin-film (single-layer) interference reflectance.
 *
 * A single dielectric film of thickness d and refractive index n1 between an
 * ambient medium (n0) and a substrate (n2). The total reflected amplitude is
 * the Airy sum over multiple reflections inside the film; the round-trip
 * phase delta = 2*k*n1*d*cos(theta_t) makes the reflectance wavelength-
 * dependent, producing iridescent colours under broadband light.
 *
 * Standard thin-film optics (Hecht, "Optics"): the total reflection
 * coefficient of the stack is
 *
 *   r = (r01 + r12 * e^(i*2*delta)) / (1 + r01 * r12 * e^(i*2*delta))
 *
 * with r01, r12 the single-interface Fresnel coefficients (s and p), and
 * delta the film round-trip phase. Reimplemented from the formula; the
 * Fresnel coefficients come from wav_fresnel.h. */

#pragma once

#include "util/math_float2.h"
#include "util/math_float3.h"

#include "waveoptics/wav_complex.h"
#include "waveoptics/wav_fresnel.h"
#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Reflectance of a single thin film between two media.
 *
 * n0: ambient refractive index (real)
 * n1: film refractive index (real)
 * n2: substrate refractive index (real)
 * d:  film thickness (metres)
 * lambda: vacuum wavelength (metres)
 * cosi: |cos(theta_incident)| (>= 0)
 *
 * Returns (Rs, Rp) power reflectances for s and p polarization. */
ccl_device_inline float2 wav_thin_film_reflectance(const float n0,
                                                   const float n1,
                                                   const float n2,
                                                   const float d,
                                                   const float lambda,
                                                   const float cosi)
{
  if (n1 <= 0.0f || cosi <= 0.0f || lambda <= 0.0f || d < 0.0f) {
    return make_float2(0.0f, 0.0f);
  }

  const float3 n = make_float3(0.0f, 0.0f, 1.0f);

  /* Snell angles. */
  const float sini = sqrtf(fmaxf(0.0f, 1.0f - cosi * cosi));
  const float sint1 = fminf(1.0f, n0 / n1 * sini);
  const float cost1 = sqrtf(fmaxf(0.0f, 1.0f - sint1 * sint1));

  /* Interface Fresnel coefficients with the actual incidence angle. */
  const float3 w0 = make_float3(-sini, 0.0f, -cosi);
  const float3 w1 = make_float3(-sint1, 0.0f, -cost1);
  const wav_complex eta01 = wav_make_complex(n1 / n0, 0.0f);
  const wav_complex eta12 = wav_make_complex(n2 / n1, 0.0f);
  const wav_fresnel_ret f01 = wav_fresnel(eta01, w0, n);
  const wav_fresnel_ret f12 = wav_fresnel(eta12, w1, n);

  /* Round-trip phase inside the film. */
  const float k = WAV_TWO_PI_F / fmaxf(lambda, 1e-12f);
  const float delta = 2.0f * k * n1 * d * cost1;
  const wav_complex e2id = wav_complex_polar(1.0f, 2.0f * delta);

  wav_complex rs_total, rp_total;
  for (int pol = 0; pol < 2; ++pol) {
    const wav_complex r01 = (pol == 0) ? f01.rs : f01.rp;
    const wav_complex r12 = (pol == 0) ? f12.rs : f12.rp;
    /* r = (r01 + r12*e2id) / (1 + r01*r12*e2id) */
    const wav_complex one = wav_make_complex(1.0f, 0.0f);
    const wav_complex num = wav_complex_add(r01, wav_complex_mul(r12, e2id));
    const wav_complex den = wav_complex_add(one, wav_complex_mul(wav_complex_mul(r01, r12), e2id));
    const wav_complex r = wav_complex_div(num, den);
    if (pol == 0) {
      rs_total = r;
    }
    else {
      rp_total = r;
    }
  }

  return make_float2(wav_complex_norm(rs_total), wav_complex_norm(rp_total));
}

/* Thin-film reflectance averaged over polarization (unpolarized light). */
ccl_device_inline float wav_thin_film_reflectance_unpolarized(const float n0,
                                                              const float n1,
                                                              const float n2,
                                                              const float d,
                                                              const float lambda,
                                                              const float cosi)
{
  const float2 R = wav_thin_film_reflectance(n0, n1, n2, d, lambda, cosi);
  return 0.5f * (R.x + R.y);
}

}  // namespace waveoptics

CCL_NAMESPACE_END
