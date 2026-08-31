/* SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wave Diffraction BSDF: Fraunhofer free-space diffraction of a rectangular
 * slit aperture.
 *
 * The mathematics are ported from wave_tracer
 * (https://github.com/ssteinberg/wave_tracer), Copyright Shlomi Steinberg,
 * licensed under CC BY-NC 4.0. See src/waveoptics/README.md.
 *
 * The closure models the angular scattering function (ASF) of a thin screen
 * with a rectangular slit: light transmitted through the slit is diffracted
 * into a pattern that depends on the slit width/height and the wavelength.
 * Following wave_tracer, the closure's value is defined as the probability
 * density of the scattering distribution (f = pdf), so sampling is exact and
 * energy is conserved by construction.
 *
 * GPU note: the lobe sampling tables are built on the host (CPU only). On GPU
 * devices the closure evaluates to zero. See src/waveoptics/wav_fsd_sampler.h.
 */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/types.h"

#include "kernel/sample/mapping.h"

#include "waveoptics/wav_fsd.h"
#include "waveoptics/wav_fsd_sampler.h"

CCL_NAMESPACE_BEGIN

using namespace waveoptics;

struct WaveDiffractionBsdf {
  SHADER_CLOSURE_BASE;
  float3 T;         /* tangent on the surface (slit axis) */
  float width;      /* slit width, scene units (meters) */
  float height;     /* slit height, scene units (meters) */
  float wavelength; /* wavelength, nanometers */
};

static_assert(sizeof(ShaderClosure) >= sizeof(WaveDiffractionBsdf),
              "WaveDiffractionBsdf is too large!");

/* 1 mm expressed in scene units (Cycles scenes use meters). */
ccl_device_inline float wave_fsd_unit()
{
  return 1e-3f;
}

/* Builds the two aperture edges of a rectangular slit (uniform field
 * amplitude: a_b = 0, iab_2 = i), in the canonical mm-scaled frame used by
 * the FSD mathematics. The edges are parallel to the tangent direction. */
ccl_device_inline int wave_aperture_build(const float width,
                                          const float height,
                                          ccl_private wav_fsd_edge *edges)
{
  const float fsd_unit = wave_fsd_unit();
  const float w_mm = width / fsd_unit;
  const float h_mm = fmaxf(height, 0.0f) / fsd_unit;

  if (w_mm <= 0.0f || h_mm <= 0.0f)
    return 0;

  edges[0].e = make_float2(0.0f, h_mm);
  edges[0].v = make_float2(-w_mm * 0.5f, 0.0f);
  edges[0].a_b = wav_make_complex(0.0f, 0.0f);
  edges[0].iab_2 = wav_make_complex(0.0f, 1.0f);

  edges[1].e = make_float2(0.0f, -h_mm);
  edges[1].v = make_float2(w_mm * 0.5f, 0.0f);
  edges[1].a_b = wav_make_complex(0.0f, 0.0f);
  edges[1].iab_2 = wav_make_complex(0.0f, 1.0f);

  return 2;
}

/* psi02: integrated 0-th order field amplitude over the aperture opening,
 * via the same 8-point approximation as wave_tracer. */
ccl_device_inline float wave_aperture_psi02(const ccl_private wav_fsd_edge *edges,
                                            const int num_edges)
{
  const float p0s = wav_fsd_p0_sigma;
  const float2 pts[8] = {
      make_float2(-0.70710678f, -0.70710678f), make_float2(-1.0f, 0.0f),
      make_float2(-0.70710678f, 0.70710678f), make_float2(0.0f, 1.0f),
      make_float2(0.70710678f, 0.70710678f), make_float2(1.0f, 0.0f),
      make_float2(0.70710678f, -0.70710678f), make_float2(0.0f, -1.0f)};

  float psi02 = 0.0f;
  for (int i = 0; i < 8; ++i) {
    psi02 += wav_fsd_asf_unclamped(
        edges, num_edges, make_float2(pts[i].x * 3.0f * p0s, pts[i].y * 3.0f * p0s));
  }
  return psi02 / 8.0f;
}

/* FSD aperture data of the slit. */
struct WaveAperture {
  wav_fsd_edge edges[2];
  int num_edges;
  float psi02;
  float P0;
  float recp_I;
  float P0_pdf;
  float edge_pdfs[2];
  float scale; /* k * fsd_unit */
};

ccl_device_inline void wave_aperture_setup(const float width,
                                           const float height,
                                           const float wavelength_nm,
                                           ccl_private WaveAperture *ap)
{
  const float lambda = wavelength_nm * 1e-9f;
  const float k = WAV_TWO_PI_F / fmaxf(lambda, 1e-12f);
  ap->scale = k * wave_fsd_unit();

  ap->num_edges = wave_aperture_build(width, height, ap->edges);
  if (ap->num_edges == 0) {
    ap->psi02 = 0.0f;
    ap->P0 = 0.0f;
    ap->recp_I = 0.0f;
    ap->P0_pdf = 1.0f;
    ap->edge_pdfs[0] = 0.0f;
    ap->edge_pdfs[1] = 0.0f;
    return;
  }

  ap->psi02 = wave_aperture_psi02(ap->edges, ap->num_edges);
  ap->P0 = wav_fsd_p0(ap->psi02);

  const float pj0 = wav_fsd_pj(ap->edges[0]);
  const float pj1 = wav_fsd_pj(ap->edges[1]);
  const float total = ap->P0 + pj0 + pj1;
  ap->recp_I = (total > 0.0f) ? 1.0f / total : 0.0f;
  ap->P0_pdf = ap->P0 * ap->recp_I;
  ap->edge_pdfs[0] = pj0 * ap->recp_I;
  ap->edge_pdfs[1] = pj1 * ap->recp_I;
}

/* ASF of the aperture at the canonical coordinate xi. */
ccl_device_inline float wave_aperture_asf(const ccl_private WaveAperture *ap, const float2 xi)
{
  return wav_fsd_asf(ap->edges, ap->num_edges, ap->psi02, xi);
}

/* Probability density (solid-angle measure, wave_tracer convention). */
ccl_device_inline float wave_aperture_pdf(const ccl_private WaveAperture *ap, const float2 xi)
{
  return wave_aperture_asf(ap, xi) * ap->recp_I;
}

ccl_device_inline void bsdf_wave_diffraction_setup(ccl_private ShaderData *sd,
                                                   const float3 N,
                                                   const float3 T,
                                                   const float width,
                                                   const float height,
                                                   const float wavelength,
                                                   const Spectrum weight)
{
  ccl_private WaveDiffractionBsdf *bsdf = (ccl_private WaveDiffractionBsdf *)bsdf_alloc(
      sd, sizeof(WaveDiffractionBsdf), weight);
  if (bsdf) {
    bsdf->N = N;
    bsdf->T = normalize(T);
    bsdf->width = fmaxf(width, 0.0f);
    bsdf->height = fmaxf(height, 0.0f);
    bsdf->wavelength = fmaxf(wavelength, 0.0f);
    bsdf->type = CLOSURE_BSDF_WAVE_DIFFRACTION_ID;
    sd->runtime_flag |= (SR_BSDF | SR_BSDF_HAS_EVAL | SR_BSDF_HAS_TRANSMISSION);
  }
}

/* Direction into the aperture frame (z = N). */
ccl_device_inline float3 wave_to_local(const ccl_private WaveDiffractionBsdf *bsdf,
                                       const float3 d)
{
  const float3 B = cross(bsdf->N, bsdf->T);
  return make_float3(dot(d, bsdf->T), dot(d, B), dot(d, bsdf->N));
}

/* Evaluates the diffraction pattern. Returns the density (f = pdf). */
ccl_device_inline float wave_diffraction_eval_pdf(const ccl_private WaveDiffractionBsdf *bsdf,
                                                  const float3 wi,
                                                  const float3 wo)
{
  WaveAperture ap;
  wave_aperture_setup(bsdf->width, bsdf->height, bsdf->wavelength, &ap);
  if (ap.num_edges == 0 || ap.recp_I <= 0.0f)
    return 0.0f;

  const float s_in = dot(wi, bsdf->N);
  const float s_out = dot(wo, bsdf->N);
  /* The aperture transmits: incoming and outgoing on opposite sides. */
  if (s_in * s_out >= 0.0f)
    return 0.0f;

  const float3 B = cross(bsdf->N, bsdf->T);
  const float2 wi_proj = make_float2(dot(wi, bsdf->T), dot(wi, B));
  const float2 wo_proj = make_float2(dot(wo, bsdf->T), dot(wo, B));

  /* Straight-through (specular) direction: -wi. */
  const float2 spec_proj = -wi_proj;
  const float2 dev = wo_proj - spec_proj;

  const float dev2 = dot(dev, dev);
  if (dev2 >= 1.0f)
    return 0.0f;

  const float wo2 = dot(wo_proj, wo_proj);
  if (wo2 >= 0.85f)
    return 0.0f;

  /* sin -> tan mapping of the deviation. */
  const float2 zeta = dev * (1.0f / sqrtf(1.0f - dev2));
  const float2 xi = make_float2(zeta.x * ap.scale, zeta.y * ap.scale);

  return wave_aperture_pdf(&ap, xi);
}

ccl_device Spectrum bsdf_wave_diffraction_eval(const ccl_private ShaderClosure *sc,
                                               const float3 wi,
                                               const float3 wo,
                                               ccl_private float *pdf)
{
  const ccl_private WaveDiffractionBsdf *bsdf = (const ccl_private WaveDiffractionBsdf *)sc;
  const float p = wave_diffraction_eval_pdf(bsdf, wi, wo);
  *pdf = p;
  return make_spectrum(p);
}

ccl_device int bsdf_wave_diffraction_sample(const ccl_private ShaderClosure *sc,
                                            const float3 Ng,
                                            const float3 wi,
                                            const float2 rand,
                                            ccl_private Spectrum *eval,
                                            ccl_private float3 *wo,
                                            ccl_private float *pdf)
{
  const ccl_private WaveDiffractionBsdf *bsdf = (const ccl_private WaveDiffractionBsdf *)sc;

  WaveAperture ap;
  wave_aperture_setup(bsdf->width, bsdf->height, bsdf->wavelength, &ap);
  if (ap.num_edges == 0 || ap.recp_I <= 0.0f) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  wav_fsd_aperture sapa;
  sapa.edges = ap.edges;
  sapa.num_edges = ap.num_edges;
  sapa.psi02 = ap.psi02;
  sapa.P0 = ap.P0;
  sapa.recp_I = ap.recp_I;
  sapa.P0_pdf = ap.P0_pdf;
  sapa.edge_pdfs = ap.edge_pdfs;

  const uint32_t seed = __float_as_uint(rand.x) ^ (__float_as_uint(rand.y) * 2654435761u);

  wav_fsd_sample_ret ret;
  if (!wav_fsd_sample(sapa, seed, &ret)) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  const float2 zeta = make_float2(ret.xi.x / ap.scale, ret.xi.y / ap.scale);

  /* tan -> sin mapping of the deviation. */
  const float zeta2 = dot(zeta, zeta);
  const float2 dev = zeta * (1.0f / sqrtf(1.0f + zeta2));

  const float3 B = cross(bsdf->N, bsdf->T);
  const float2 wi_proj = make_float2(dot(wi, bsdf->T), dot(wi, B));
  const float2 spec_proj = -wi_proj;
  const float2 wo_proj = spec_proj + dev;

  const float wo2 = dot(wo_proj, wo_proj);
  if (wo2 >= 0.85f) {
    *pdf = 0.0f;
    *eval = zero_spectrum();
    return LABEL_NONE;
  }

  const float s_in = dot(wi, bsdf->N);
  const float wo_z = (s_in >= 0.0f ? -1.0f : 1.0f) * sqrtf(1.0f - wo2);

  *wo = normalize(bsdf->T * wo_proj.x + B * wo_proj.y + bsdf->N * wo_z);
  *pdf = ret.pdf;
  /* f = pdf convention (wave_tracer), so eval/pdf = 1. */
  *eval = make_spectrum(ret.pdf);
  return LABEL_TRANSMIT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END
