/* SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thin-film interference BSDF closure.
 *
 * A diffuse-like reflector whose reflectance is computed from a single
 * dielectric thin film (thickness d, film index n1) between an ambient
 * medium (n0) and a substrate (n2). The wavelength-dependent Airy
 * reflectance (see waveoptics/wav_thin_film.h) makes the surface
 * iridescent under broadband light.
 *
 * The closure samples the reflection hemisphere with a cosine-weighted
 * distribution and evaluates the thin-film reflectance at the incoming
 * angle on the three sRGB primary wavelengths (nm converted to metres),
 * producing wavelength-dependent colours. */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/sample/mapping.h"
#include "kernel/svm/types.h"

#include "waveoptics/wav_spectrum.h"
#include "waveoptics/wav_thin_film.h"

CCL_NAMESPACE_BEGIN

using namespace waveoptics;

/* Thin-film interference BSDF. */
struct WaveThinFilmBsdf {
  SHADER_CLOSURE_BASE;
  float n0;        /* ambient index */
  float n1;        /* film index */
  float n2;        /* substrate index */
  float thickness; /* film thickness (m) */
};

static_assert(sizeof(WaveThinFilmBsdf) <= sizeof(ShaderClosure),
              "WaveThinFilmBsdf must fit in ShaderClosure");

/* 3-band evaluation of the thin-film reflectance at the incoming angle. */
ccl_device Spectrum bsdf_wave_thin_film_eval(const ccl_private ShaderClosure *sc,
                                             const float3 wi,
                                             const float3 wo,
                                             ccl_private float *pdf)
{
  const ccl_private WaveThinFilmBsdf *bsdf = (const ccl_private WaveThinFilmBsdf *)sc;

  /* Reflection hemisphere: both directions on the same side of the surface. */
  const float cosi = dot(wi, bsdf->N);
  const float coso = dot(wo, bsdf->N);
  if (cosi <= 0.0f || coso <= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float wavelengths[3];
  wav_spectrum_three_band(wavelengths);
  /* The spectrum bands are stored in nm; convert to metres. */
  wavelengths[0] *= 1e-9f;
  wavelengths[1] *= 1e-9f;
  wavelengths[2] *= 1e-9f;

  Spectrum result;
  result.x = wav_thin_film_reflectance_unpolarized(
      bsdf->n0, bsdf->n1, bsdf->n2, bsdf->thickness, wavelengths[0], cosi);
  result.y = wav_thin_film_reflectance_unpolarized(
      bsdf->n0, bsdf->n1, bsdf->n2, bsdf->thickness, wavelengths[1], cosi);
  result.z = wav_thin_film_reflectance_unpolarized(
      bsdf->n0, bsdf->n1, bsdf->n2, bsdf->thickness, wavelengths[2], cosi);

  /* Lambertian factor: BRDF = albedo/pi, pdf = cos/pi. */
  result *= M_1_PI_F;
  *pdf = coso * M_1_PI_F;
  return result;
}

ccl_device int bsdf_wave_thin_film_sample(const ccl_private ShaderClosure *sc,
                                          const float3 Ng,
                                          const float3 wi,
                                          const float2 rand,
                                          ccl_private Spectrum *eval,
                                          ccl_private float3 *wo,
                                          ccl_private float *pdf)
{
  const ccl_private WaveThinFilmBsdf *bsdf = (const ccl_private WaveThinFilmBsdf *)sc;

  /* Cosine-weighted hemisphere sampling around the normal. */
  const float2 disk = sample_uniform_disk(rand);
  const float3 T = normalize(make_float3(-bsdf->N.y, bsdf->N.x, 0.0f));
  const float3 B = cross(bsdf->N, T);
  const float z = sqrtf(fmaxf(0.0f, 1.0f - dot(disk, disk)));
  *wo = normalize(T * disk.x + B * disk.y + bsdf->N * z);
  if (!isfinite(wo->x) || !isfinite(wo->y) || !isfinite(wo->z)) {
    /* Degenerate tangent for axis-aligned normals: fall back to +x. */
    *wo = normalize(make_float3(disk.x, disk.y, z));
  }

  const float coso = dot(*wo, bsdf->N);
  if (coso <= 0.0f) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  *pdf = coso * M_1_PI_F;
  *eval = bsdf_wave_thin_film_eval(sc, wi, *wo, pdf);
  return LABEL_REFLECT | LABEL_DIFFUSE;
}

ccl_device_inline void bsdf_wave_thin_film_setup(ccl_private ShaderData *sd,
                                                 const float3 N,
                                                 const float n0,
                                                 const float n1,
                                                 const float n2,
                                                 const float thickness,
                                                 const Spectrum weight)
{
  ccl_private WaveThinFilmBsdf *bsdf = (ccl_private WaveThinFilmBsdf *)bsdf_alloc(
      sd, sizeof(WaveThinFilmBsdf), weight);
  if (bsdf) {
    bsdf->N = N;
    bsdf->n0 = fmaxf(n0, 1.0f);
    bsdf->n1 = fmaxf(n1, 1.0f);
    bsdf->n2 = fmaxf(n2, 1.0f);
    bsdf->thickness = fmaxf(thickness, 0.0f);
    bsdf->type = CLOSURE_BSDF_WAVE_THIN_FILM_ID;
    sd->runtime_flag |= (SR_BSDF | SR_BSDF_HAS_EVAL);
  }
}

CCL_NAMESPACE_END
