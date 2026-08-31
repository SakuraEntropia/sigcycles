/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Uniform Theory of Diffraction (UTD) wedge diffraction.
 *
 * Ported verbatim (units stripped) from wave_tracer
 * include/wt/interaction/fsd/utd.hpp and fsd/common.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 */

#pragma once

#include "util/math_float3.h"

#include "waveoptics/wav_complex.h"
#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

static constexpr float wav_utd_min_sin_beta = 1e-3f;

/* UTD wedge edge (all lengths in scene units, angles in radians). */
struct wav_wedge_edge {
  float3 v;   /* edge midpoint */
  float l;    /* edge length */
  float3 nff; /* front-face normal */
  float3 tff; /* tangent direction pointing into the wedge */
  float3 nbf; /* back-face normal */
  float alpha; /* wedge opening angle (rad) */
  float eta;   /* refractive index (kept for parity; not used by UTD) */
};

/* UTD a+/- functions. */
template<int sgn>
ccl_device_inline float wav_utd_a(const float phi, const float n)
{
  static_assert(sgn == +1 || sgn == -1, "sgn must be +/-1");
  const float N = roundf((float)(sgn * WAV_PI_F + phi) * WAV_INV_TWO_PI_F / n);
  return 2.0f * wav_sqr(cosf(WAV_PI_F * n * N - phi / 2.0f));
}

/* UTD F transition function. */
ccl_device_inline wav_complex wav_utd_f(const float x)
{
  const float absx = fabsf(x);

  wav_complex result;
  if (absx < 6.0f) {
    const float sqrt_x = sqrtf(absx);
    const wav_complex cerf = wav_cerfc(
        wav_complex_mul_real(wav_complex_polar(1.0f, WAV_PI_4_F), sqrt_x));
    result = wav_complex_mul_real(wav_make_complex(1.0f, 1.0f), WAV_SQRT_PI_2_F);
    result = wav_complex_mul(result, wav_make_complex(sqrt_x, 0.0f));
    result = wav_complex_mul(result, wav_complex_exp(wav_make_complex(0.0f, absx)));
    result = wav_complex_mul(result, cerf);
  }
  else {
    /* Fast approximation for large values. */
    const float r = 1.0f / (2.0f * absx);
    const float r2 = r * r;
    const float r3 = r2 * r;
    const float r4 = r2 * r2;
    result = wav_make_complex(1.0f + (-3.0f * r2 + 75.0f * r4),
                              r - 15.0f * r3);
  }

  return x < 0.0f ? wav_complex_conj(result) : result;
}

/* Returns the point on the wedge that satisfies Fermat's principle
 * (src -> dst), if such a point exists. */
ccl_device_inline bool wav_wedge_diffraction_point(const wav_wedge_edge &e,
                                                   const float3 src,
                                                   const float3 dst,
                                                   float3 *p)
{
  const float3 ed = cross(e.nff, e.tff);
  const float sl = sqrtf(wav_length2(make_float2(dot(src - e.v, e.tff), dot(src - e.v, e.nff))));
  const float dl = sqrtf(wav_length2(make_float2(dot(dst - e.v, e.tff), dot(dst - e.v, e.nff))));
  const float dist = dot(ed, src - e.v) + dot(dst - src, ed) * sl / (sl + dl);

  if (fabsf(dist) > e.l / 2.0f)
    return false;

  const float3 pp = e.v + ed * dist;
  if (pp == src || pp == dst)
    return false;
  *p = pp;
  return true;
}

/* Returns the point on the wedge that satisfies Fermat's principle
 * (src -> direction wo), if such a point exists. */
ccl_device_inline bool wav_wedge_diffraction_point_dir(const wav_wedge_edge &e,
                                                       const float3 src,
                                                       const float3 wo,
                                                       float3 *p)
{
  const float3 ed = cross(e.nff, e.tff);
  const float cos_beta = dot(wo, ed);
  const float sin_beta = sqrtf(fmaxf(0.0f, 1.0f - wav_sqr(cos_beta)));

  if (sin_beta < wav_utd_min_sin_beta)
    return false;

  const float sl = sqrtf(wav_length2(make_float2(dot(src - e.v, e.tff), dot(src - e.v, e.nff))));
  const float3 prj_src = e.v + dot(src - e.v, ed) * ed;
  const float3 pp = prj_src + sl * (cos_beta / sin_beta) * ed;

  if (wav_length2(pp - e.v) > wav_sqr(e.l / 2.0f))
    return false;
  if (pp == src)
    return false;
  *p = pp;
  return true;
}

/* UTD result: diffraction coefficients and SH frames. */
struct wav_utd_ret {
  wav_complex Ds, Dh;
  float3 si, hi; /* incident SH frame */
  float3 so, ho; /* scattered SH frame */
};

/* The UTD wedge diffraction function.
 * Does NOT account for the phase term exp(-i*k*ro).
 * k: wavenumber (2*pi/lambda), wi/wo: incident/outgoing directions,
 * ro: distance from the edge point to the receiver. */
ccl_device_inline wav_utd_ret wav_wedge_utd(const wav_wedge_edge &wedge,
                                            const float k,
                                            const float3 wi,
                                            const float3 wo,
                                            const float ro)
{
  const float3 e = cross(wedge.nff, wedge.tff);
  const float n = 2.0f - wedge.alpha * WAV_INV_PI_F;

  /* Build in/out transverse frames. */
  const float3 ti = -normalize(cross(e, -wi));
  const float3 bi = normalize(cross(ti, -wi));
  const float3 to = -normalize(cross(e, wo));
  const float3 bo = normalize(cross(to, wo));

  /* Angles. */
  const float sin_beta2 = fmaxf(0.0f, 1.0f - wav_sqr(dot(wi, e)));
  const float sin_beta = sqrtf(sin_beta2);
  const float phii = atan2f(dot(wedge.nff, wi), dot(wedge.tff, wi));
  const float phio = atan2f(dot(wedge.nff, wo), dot(wedge.tff, wo));

  /* Distance parameters. */
  const float Li = ro * sin_beta2;
  const float Lrn = Li, Lro = Li;

  /* Diffraction coefficients. */
  const float a1 = wav_utd_a<+1>(phii - phio, n);
  const float a2 = wav_utd_a<-1>(phii - phio, n);
  const float a3 = wav_utd_a<+1>(phii + phio, n);
  const float a4 = wav_utd_a<-1>(phii + phio, n);
  const wav_complex F1 = wav_utd_f(k * Li * a1);
  const wav_complex F2 = wav_utd_f(k * Li * a2);
  const wav_complex F3 = wav_utd_f(k * Lrn * a3);
  const wav_complex F4 = wav_utd_f(k * Lro * a4);
  /* -cot((pi + (phii-phio)) / (2n)) * F1 */
  const float c1 = -cosf((WAV_PI_F + (phii - phio)) / (2.0f * n)) /
                   sinf((WAV_PI_F + (phii - phio)) / (2.0f * n));
  const wav_complex D1v = wav_complex_mul_real(F1, c1);
  const float c2 = -cosf((WAV_PI_F - (phii - phio)) / (2.0f * n)) /
                   sinf((WAV_PI_F - (phii - phio)) / (2.0f * n));
  const wav_complex D2 = wav_complex_mul_real(F2, c2);
  const float c3 = -cosf((WAV_PI_F + (phii + phio)) / (2.0f * n)) /
                   sinf((WAV_PI_F + (phii + phio)) / (2.0f * n));
  const wav_complex D3 = wav_complex_mul_real(F3, c3);
  const float c4 = -cosf((WAV_PI_F - (phii + phio)) / (2.0f * n)) /
                   sinf((WAV_PI_F - (phii + phio)) / (2.0f * n));
  const wav_complex D4 = wav_complex_mul_real(F4, c4);

  const float kro = k * ro;
  const float Ds = 1.0f / (2.0f * n * sqrtf(kro) * sin_beta) * WAV_INV_SQRT_TWO_PI_F;
  /* D = Ds * exp(-i*pi/4); folded into the coefficients below. */

  const float t1 = wav_mod(phii + phio, WAV_PI_2_F);
  const float t2 = wav_mod(phii - phio, WAV_PI_2_F);
  const bool degenerate = (fabsf(t1) < 1e-5f || fabsf(t2) < 1e-5f);
  const wav_complex D1D2 = wav_complex_add(D1v, D2);
  const wav_complex D3D4 = wav_complex_add(D3, D4);
  const wav_complex Ds_sum = degenerate ? wav_make_complex(0.0f, 0.0f) :
                                          wav_complex_sub(D1D2, D3D4);
  const wav_complex Dh_sum = degenerate ? wav_make_complex(0.0f, 0.0f) :
                                          wav_complex_add(D1D2, D3D4);

  const wav_complex phase = wav_complex_exp(wav_make_complex(0.0f, -WAV_PI_4_F));
  wav_utd_ret ret;
  ret.Ds = wav_complex_mul(wav_complex_mul_real(Ds_sum, -Ds), phase);
  ret.Dh = wav_complex_mul(wav_complex_mul_real(Dh_sum, -Ds), phase);
  ret.si = ti;
  ret.hi = bi;
  ret.so = to;
  ret.ho = bo;
  return ret;
}

}  // namespace waveoptics

CCL_NAMESPACE_END
