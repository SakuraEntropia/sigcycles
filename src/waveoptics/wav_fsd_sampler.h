/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Importance sampler for the Fraunhofer FSD ASF.
 *
 * Ported from wave_tracer src/interaction/fsd/fraunhofer/fsd_sampler.cpp
 * and include/wt/interaction/fsd/fraunhofer/fsd_lut.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 *
 * The original uses large precomputed LUT files (2048/3072 samples). Here the
 * inverse-CDF tables for the alpha1/alpha2 lobes are built numerically at
 * module load (host side) at a compact 256x128 resolution, over the canonical
 * domain |zeta| <= WAV_FSD_RMAX (the domain implied by the sampling cutoff
 * |wolocal|^2 < 0.85 in the original BSDF).
 *
 * This header is kernel-safe (no STL, no mutable globals). Table building
 * lives in wav_fsd_tables.h (host only). On GPU devices the tables are
 * unavailable and sampling returns zero (documented limitation).
 */

#pragma once

#include "util/math_float2.h"
#include "util/math_float3.h"

#include "waveoptics/wav_complex.h"
#include "waveoptics/wav_fsd.h"
#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Canonical radius domain of the lobe tables. */
static constexpr int wav_fsd_lut_n_theta = 256;
static constexpr int wav_fsd_lut_n_r = 256;
static constexpr float wav_fsd_rmax = 64.0f;

struct wav_fsd_lut {
  float iCDFtheta[wav_fsd_lut_n_theta];
  float iCDF[wav_fsd_lut_n_theta][wav_fsd_lut_n_r];
};

#ifdef __KERNEL_GPU__
ccl_device_inline const wav_fsd_lut *wav_fsd_lut_a1()
{
  return nullptr;
}
ccl_device_inline const wav_fsd_lut *wav_fsd_lut_a2()
{
  return nullptr;
}
#else
/* Host-populated tables (see wav_fsd_tables.h). */
const wav_fsd_lut *wav_fsd_lut_a1();
const wav_fsd_lut *wav_fsd_lut_a2();
#endif

/* Tiny hash-based RNG for the rejection loop (deterministic per seed). */
ccl_device_inline uint32_t wav_rng_hash(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}
ccl_device_inline float wav_rng_unit(uint32_t *state)
{
  *state = wav_rng_hash(*state + 0x9e3779b9u);
  return (float)(*state & 0x00ffffffu) * (1.0f / 16777216.0f);
}

/* Sample a point from the |alpha1|^2 or |alpha2|^2 distribution (canonical
 * space, first quadrant + random quadrant). Returns false if the LUT is
 * unavailable (GPU). */
ccl_device_inline bool wav_fsd_lut_sample(const wav_fsd_lut *lut,
                                          uint32_t *rng,
                                          float2 *out)
{
  if (lut == nullptr)
    return false;

  const float u_theta = wav_rng_unit(rng);
  const float u_r = wav_rng_unit(rng);

  /* Invert marginal angle CDF (theta in [0, pi/2]). */
  const float th = u_theta * (float)(wav_fsd_lut_n_theta - 1);
  const int ti = min((int)th, wav_fsd_lut_n_theta - 1);
  const int ti1 = min(ti + 1, wav_fsd_lut_n_theta - 1);
  const float tf = th - (float)ti;
  const float theta = wav_lerp(lut->iCDFtheta[ti], lut->iCDFtheta[ti1], tf) *
                      WAV_PI_2_F;

  /* Invert conditional radius CDF (bilinear in (theta, u_r)). */
  const float rf_pos = clamp(u_r, 0.0f, 1.0f) * (float)(wav_fsd_lut_n_r - 1);
  const int ri = min((int)rf_pos, wav_fsd_lut_n_r - 1);
  const int ri1 = min(ri + 1, wav_fsd_lut_n_r - 1);
  const float rff = rf_pos - (float)ri;
  const float r_lo = wav_lerp(lut->iCDF[ti][ri], lut->iCDF[ti][ri1], rff);
  const float r_hi = wav_lerp(lut->iCDF[ti1][ri], lut->iCDF[ti1][ri1], rff);
  const float r = fmaxf(0.0f, wav_lerp(r_lo, r_hi, tf));

  float2 zeta = make_float2(r * cosf(theta), r * sinf(theta));

  /* Random quadrant. */
  const int q = min(3, (int)(wav_rng_unit(rng) * 4.0f));
  zeta.x *= (((q + 1) / 2) % 2 == 0) ? 1.0f : -1.0f;
  zeta.y *= ((q / 2) % 2 == 0) ? 1.0f : -1.0f;

  *out = zeta;
  return true;
}

/* Sample the 0-th order lobe (canonical space). */
ccl_device_inline float2 wav_fsd_sample_p0(uint32_t *rng)
{
  /* Box-Muller via two uniforms. */
  const float u1 = fmaxf(wav_rng_unit(rng), 1e-8f);
  const float u2 = wav_rng_unit(rng);
  const float rr = sqrtf(-2.0f * logf(u1));
  const float th = WAV_TWO_PI_F * u2;
  return make_float2(wav_fsd_p0_sigma * rr * cosf(th),
                     wav_fsd_p0_sigma * rr * sinf(th));
}

/* Sample one edge's lobes (alpha1 or alpha2 mixture). Returns canonical xi in
 * the aperture's mm-scaled frame (as in wave_tracer). */
ccl_device_inline bool wav_fsd_sample_edge(const wav_fsd_edge e,
                                           uint32_t *rng,
                                           float2 *out)
{
  /* Sample one of the alpha1/alpha2 functions of the diffracted lobes. */
  const float A = wav_complex_norm(e.a_b);
  const float B = wav_complex_norm(e.iab_2);
  const float sum = A + B;
  const bool sample_a1 = (sum > 0.0f) ? (wav_rng_unit(rng) * sum < A) : true;

  float2 zeta;
  const wav_fsd_lut *lut = sample_a1 ? wav_fsd_lut_a1() : wav_fsd_lut_a2();
  if (!wav_fsd_lut_sample(lut, rng, &zeta))
    return false;

  const wav_mat2 invXi = wav_mat2_inverse(wav_fsd_edge_xi(e));
  *out = wav_mat2_mul_row(invXi, zeta);
  return true;
}

/* Aperture: edges + normalization data, in canonical (mm-scaled) frame. */
struct wav_fsd_aperture {
  const wav_fsd_edge *edges;
  int num_edges;
  float psi02;
  float P0;
  float recp_I;
  float P0_pdf;
  const float *edge_pdfs;
};

/* Sampling density (proposal) of the aperture. */
ccl_device_inline float wav_fsd_sampling_density(const wav_fsd_aperture &aperture,
                                                 const float2 xi)
{
  float diffracted = 0.0f;
  for (int i = 0; i < aperture.num_edges; ++i)
    diffracted += wav_fsd_psi2(aperture.edges[i], xi);
  return diffracted * wav_fsd_chi_e(xi) +
         aperture.P0 * WAV_INV_TWO_PI_F / wav_sqr(wav_fsd_p0_sigma) *
             wav_fsd_chi_0(xi);
}

/* Target ASF of the aperture. */
ccl_device_inline float wav_fsd_asf_aperture(const wav_fsd_aperture &aperture,
                                             const float2 xi)
{
  return wav_fsd_asf(aperture.edges, aperture.num_edges, aperture.psi02, xi);
}

struct wav_fsd_sample_ret {
  float2 xi;
  float pdf;
  float weight;
};

/* Sample the aperture ASF with rejection sampling (target f = ASF, proposal
 * g = per-edge lobe mixture + P0). Single-edge apertures are exact.
 * The returned weight is 1 (the closure value is defined as the pdf).
 * Returns false if no valid sample was produced (e.g. GPU, or degenerate
 * aperture). */
ccl_device_inline bool wav_fsd_sample(const wav_fsd_aperture &aperture,
                                      uint32_t seed,
                                      wav_fsd_sample_ret *ret)
{
  if (aperture.num_edges == 0 || aperture.recp_I <= 0.0f)
    return false;

  const int M = aperture.num_edges;
  const int max_tries = M * 1024;
  const float recp_M = 1.0f / (float)M;
  uint32_t rng = seed | 1u;

  for (int tr = 0; tr < max_tries; ++tr) {
    /* Select component: P0 (index M) or one of the edges, with
     * probabilities P0_pdf / edge_pdfs (matching the proposal density). */
    const float u = wav_rng_unit(&rng);
    float cum = 0.0f;
    int comp = M;
    for (int i = 0; i < M; ++i) {
      cum += aperture.edge_pdfs[i];
      if (u < cum) {
        comp = i;
        break;
      }
    }

    float2 xi;
    if (comp == M) {
      xi = wav_fsd_sample_p0(&rng);
    }
    else {
      if (!wav_fsd_sample_edge(aperture.edges[comp], &rng, &xi))
        return false;
    }

    const float g = wav_fsd_sampling_density(aperture, xi);
    const float f = wav_fsd_asf_aperture(aperture, xi);

    /* Rejection: accept with probability f / (M * g). Single-edge: exact. */
    const bool done = (M > 1) ? (wav_rng_unit(&rng) * g < f * recp_M) : true;

    if (done) {
      ret->xi = xi;
      ret->pdf = f * aperture.recp_I;
      ret->weight = 1.0f;
      return true;
    }
  }

  return false;
}

}  // namespace waveoptics

CCL_NAMESPACE_END
