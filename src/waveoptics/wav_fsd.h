/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Free-space diffraction (FSD) angular scattering function mathematics,
 * under the Fraunhofer approximation.
 *
 * Ported verbatim (units stripped) from wave_tracer
 * include/wt/interaction/fsd/fraunhofer/fsd.hpp and fsd_sampler.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 */

#pragma once

#include "util/math_float2.h"
#include "util/math_float3.h"

#include "waveoptics/wav_complex.h"
#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Edge parametrizing a free-space diffraction angular scattering function.
 * All lengths are in the canonical wavenumber-scaled frame. */
struct wav_fsd_edge {
  float2 e;         /* edge vector */
  float2 v;         /* mid point */
  wav_complex a_b;  /* beam amplitude difference (ca - cb) */
  wav_complex iab_2; /* i * (ca + cb) / 2 */
};

/* Row-vector times matrix: (dot(v, m.c0), dot(v, m.c1)). */
ccl_device_inline float2 wav_mat2_mul_row(const wav_mat2 m, const float2 v)
{
  return make_float2(dot(v, m.c0), dot(v, m.c1));
}

/* Tangent vector (premultiplied by wavenumber). */
ccl_device_inline float2 wav_fsd_edge_m(const wav_fsd_edge e)
{
  return make_float2(e.e.y, -e.e.x);
}

/* Xi matrix (premultiplied by wavenumber). */
ccl_device_inline wav_mat2 wav_fsd_edge_xi(const wav_fsd_edge e)
{
  wav_mat2 m;
  m.c0 = e.e;
  m.c1 = wav_fsd_edge_m(e);
  return m;
}

/* Power contained in chi_e x |alpha1|^2 */
static constexpr float wav_fsd_power_a1 = 0.0049361075794549872500f;
/* Power contained in chi_e x |alpha2|^2 */
static constexpr float wav_fsd_power_a2 = 0.21899789398059305541f;

static constexpr float wav_fsd_p0_sigma = 0.288675134594813f / 4.0f; /* 1/sqrt(12)/4 */

ccl_device_inline float wav_fsd_alpha1(const float x, const float y)
{
  if (x == 0.0f)
    return 0.0f;
  return WAV_INV_TWO_PI_F * y / (x * (x * x + y * y)) *
         (cosf(x / 2.0f) - wav_sinc(x / 2.0f));
}
ccl_device_inline float wav_fsd_alpha1(const float2 zeta)
{
  return wav_fsd_alpha1(zeta.x, zeta.y);
}
ccl_device_inline float wav_fsd_alpha2(const float x, const float y)
{
  if (x == 0.0f)
    return 0.0f;
  return WAV_INV_TWO_PI_F * y / (x * x + y * y) * wav_sinc(x / 2.0f);
}
ccl_device_inline float wav_fsd_alpha2(const float2 zeta)
{
  return wav_fsd_alpha2(zeta.x, zeta.y);
}

/* Masking function for the diffracted lobes. */
ccl_device_inline float wav_fsd_chi_e(const float2 xi)
{
  constexpr float chi = 0.830092714835359f;
  const float xi2 = dot(xi, xi);
  const float t = 1.0f + chi * xi2;
  const float t2 = t * t;
  const float t3 = t2 * t;
  return fmaxf(0.0f, 1.0f - (3.0f / t2 - 2.0f / t3));
}

/* Masking function for the 0-th order lobe. */
ccl_device_inline float wav_fsd_chi_0(const float2 xi)
{
  const float2 x = make_float2(xi.x / wav_fsd_p0_sigma, xi.y / wav_fsd_p0_sigma);
  const float xi2 = dot(x, x);
  return expf(-0.5f * xi2);
}

/* Psi function of the FSD diffraction function (without the 0-th order lobe). */
ccl_device_inline wav_complex wav_fsd_psi(const wav_fsd_edge e, const float2 xi)
{
  const float2 zeta = wav_mat2_mul_row(wav_fsd_edge_xi(e), xi);

  const wav_complex a1 = wav_complex_mul_real(e.a_b, wav_fsd_alpha1(zeta));
  const wav_complex a2 = wav_complex_mul_real(e.iab_2, wav_fsd_alpha2(zeta));
  const wav_complex sum = wav_complex_add(a1, a2);

  const float ee2 = wav_length2(e.e);
  const float vxi = dot(e.v, xi);

  return wav_complex_mul(wav_complex_polar(ee2, -vxi), sum);
}

/* Approximates the |Psi|^2 scattering function (without the 0-th order lobe). */
ccl_device_inline float wav_fsd_psi2(const wav_fsd_edge e, const float2 xi)
{
  const float2 zeta = wav_mat2_mul_row(wav_fsd_edge_xi(e), xi);

  const wav_complex a1 = wav_complex_mul_real(e.a_b, wav_fsd_alpha1(zeta));
  const wav_complex a2 = wav_complex_mul_real(e.iab_2, wav_fsd_alpha2(zeta));
  const wav_complex sum = wav_complex_add(a1, a2);

  const float ee2 = wav_length2(e.e);
  return wav_sqr(ee2) * wav_complex_norm(sum);
}

/* Evaluates the free-space diffraction ASF (without the 0-th order lobe). */
ccl_device_inline float wav_fsd_asf_unclamped(const wav_fsd_edge *edges,
                                              const int num_edges,
                                              const float2 xi)
{
  wav_complex amplitude = wav_make_complex(0.0f, 0.0f);
  for (int i = 0; i < num_edges; ++i)
    amplitude = wav_complex_add(amplitude, wav_fsd_psi(edges[i], xi));
  return wav_complex_norm(amplitude);
}

/* Evaluates the free-space diffraction ASF. */
ccl_device_inline float wav_fsd_asf(const wav_fsd_edge *edges,
                                    const int num_edges,
                                    const float psi02,
                                    const float2 xi)
{
  const float diffracted = wav_fsd_asf_unclamped(edges, num_edges, xi);
  return diffracted * wav_fsd_chi_e(xi) + psi02 * wav_fsd_chi_0(xi);
}

/* Power in the 0-th order lobe. */
ccl_device_inline float wav_fsd_p0(const float psi02)
{
  return WAV_TWO_PI_F * wav_sqr(wav_fsd_p0_sigma) * psi02;
}

/* Power in the edge's chi_e x |alpha1|^2 lobe (0-th order lobe removed). */
ccl_device_inline float wav_fsd_pa1(const wav_fsd_edge edge)
{
  return wav_sqr(wav_length2(edge.e)) * wav_fsd_power_a1 * wav_complex_norm(edge.a_b);
}
/* Power in the edge's chi_e x |alpha2|^2 lobe (0-th order lobe removed). */
ccl_device_inline float wav_fsd_pa2(const wav_fsd_edge edge)
{
  return wav_sqr(wav_length2(edge.e)) * wav_fsd_power_a2 * wav_complex_norm(edge.iab_2);
}
/* Approximates the scattered power contained in an edge. */
ccl_device_inline float wav_fsd_pj(const wav_fsd_edge edge)
{
  return wav_fsd_pa1(edge) + wav_fsd_pa2(edge);
}

}  // namespace waveoptics

CCL_NAMESPACE_END
