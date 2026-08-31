/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Fresnel reflection/refraction with complex refractive index
 * (needed for wave optics polarization handling).
 *
 * Ported verbatim (units stripped) from wave_tracer
 * include/wt/interaction/fresnel.hpp,
 * Copyright Shlomi Steinberg, CC BY-NC 4.0. See README.md.
 */

#pragma once

#include "util/math_float3.h"

#include "waveoptics/wav_complex.h"
#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Reflects direction w about normal n. w points away from the surface. */
ccl_device_inline float3 wav_reflect(const float3 w, const float3 n)
{
  return 2.0f * dot(w, n) * n - w;
}

struct wav_refract_ret {
  float3 t;
  float cost;
  float eta_12;
  bool tir;
};

/* Refracts direction w about normal n. w points away from the surface.
 * eta_12: refractive-index ratio. */
ccl_device_inline wav_refract_ret wav_refract(const float eta_12,
                                              const float3 w,
                                              const float3 n)
{
  float eta = eta_12;
  const float wn = dot(w, n);
  eta = wn > 0.0f ? eta : 1.0f / eta;

  const float cost2 = 1.0f - wav_sqr(eta) * (1.0f - wav_sqr(wn));
  wav_refract_ret ret;
  ret.eta_12 = eta;
  if (cost2 >= 0.0f) {
    const float cost = sqrtf(cost2);
    const float3 t = eta * (wn * n - w) - cost * (wn >= 0.0f ? n : -n);
    ret.t = normalize(t);
    ret.cost = cost;
    ret.tir = false;
  }
  else {
    ret.t = make_float3(0.0f, 0.0f, 1.0f);
    ret.cost = 0.0f;
    ret.tir = true;
  }
  return ret;
}

struct wav_fresnel_ret {
  float3 t;
  wav_complex eta_12;
  float Z; /* change of impedance */
  wav_complex rs, rp;
  wav_complex ts, tp;
  float Ts, Tp;
};

ccl_device_inline bool wav_fresnel_tir(const wav_fresnel_ret &f)
{
  return f.Ts == 0.0f && f.Tp == 0.0f;
}
ccl_device_inline float wav_fresnel_rs_mag(const wav_fresnel_ret &f)
{
  return 1.0f - f.Ts;
}
ccl_device_inline float wav_fresnel_rp_mag(const wav_fresnel_ret &f)
{
  return 1.0f - f.Tp;
}

/* Computes Fresnel coefficients and refracted direction at an interface.
 * w points away from the surface. eta_12: (complex) refractive-index ratio.
 * NOTE: matches wave_tracer: the coefficient formulas use the real part of
 * the ratio (from the refraction step); the complex ratio is only used for
 * the eta == 1 test. */
ccl_device_inline wav_fresnel_ret wav_fresnel(const wav_complex eta_12,
                                              const float3 w,
                                              const float3 n)
{
  wav_fresnel_ret ret;
  ret.eta_12 = eta_12;

  if (eta_12.re == 1.0f && eta_12.im == 0.0f) {
    ret.t = -w;
    ret.Z = 1.0f;
    ret.rs = wav_make_complex(0.0f, 0.0f);
    ret.rp = wav_make_complex(0.0f, 0.0f);
    ret.ts = wav_make_complex(1.0f, 0.0f);
    ret.tp = wav_make_complex(1.0f, 0.0f);
    ret.Ts = 1.0f;
    ret.Tp = 1.0f;
    return ret;
  }

  const float abs_cosi = fabsf(dot(w, n));
  const wav_refract_ret refr = wav_refract(eta_12.re, w, n);

  if (abs_cosi == 0.0f || refr.tir) {
    ret.t = make_float3(0.0f, 0.0f, 1.0f);
    ret.eta_12 = wav_make_complex(refr.eta_12, 0.0f);
    ret.Z = 1.0f;
    ret.rs = wav_make_complex(1.0f, 0.0f);
    ret.rp = wav_make_complex(1.0f, 0.0f);
    ret.ts = wav_make_complex(0.0f, 0.0f);
    ret.tp = wav_make_complex(0.0f, 0.0f);
    ret.Ts = 0.0f;
    ret.Tp = 0.0f;
    return ret;
  }

  const float cost = refr.cost;
  const float eta = refr.eta_12;

  const wav_complex rs = wav_complex_div_real(
      wav_make_complex(eta * abs_cosi - cost, 0.0f),
      eta * abs_cosi + cost);
  const wav_complex rp = wav_complex_div_real(
      wav_make_complex(abs_cosi - eta * cost, 0.0f),
      abs_cosi + eta * cost);
  const wav_complex ts = wav_complex_add_real(rs, 1.0f);
  const wav_complex tp = wav_complex_mul_real(wav_complex_add_real(rp, 1.0f), eta);

  const float Z = fabsf(cost / (eta * abs_cosi));

  ret.t = refr.t;
  ret.eta_12 = wav_make_complex(eta, 0.0f);
  ret.Z = Z;
  ret.rs = rs;
  ret.rp = rp;
  ret.ts = ts;
  ret.tp = tp;
  ret.Ts = fminf(1.0f, Z * wav_complex_norm(ts));
  ret.Tp = fminf(1.0f, Z * wav_complex_norm(tp));
  return ret;
}

/* Fresnel coefficients (reflection only) at a conductive interface.
 * w points away from the surface. eta_12: (complex) refractive-index ratio. */
ccl_device_inline void wav_fresnel_reflection(const wav_complex eta_12,
                                              const float3 w,
                                              const float3 n,
                                              wav_complex *rs,
                                              wav_complex *rp)
{
  const float wn = dot(w, n);
  if ((eta_12.re == 1.0f && eta_12.im == 0.0f) || wn < 0.0f) {
    *rs = wav_make_complex(0.0f, 0.0f);
    *rp = wav_make_complex(0.0f, 0.0f);
    return;
  }

  /* t2 = 1 - (1-wn^2) * eta^2 */
  const wav_complex eta2 = wav_complex_mul(eta_12, eta_12);
  const wav_complex t2 = wav_complex_sub_real(
      wav_complex_mul_real(eta2, 1.0f - wav_sqr(wn)), 1.0f);
  /* t2 = -(t2) to match: c_t{1,0} - (1-wn^2)eta^2 */
  const wav_complex t2n = wav_make_complex(-t2.re, -t2.im);
  const wav_complex t = wav_complex_sqrt(t2n);
  const wav_complex i = wav_make_complex(wn, 0.0f);

  *rs = wav_complex_div(wav_complex_sub(wav_complex_mul(eta_12, i), t),
                        wav_complex_add(wav_complex_mul(eta_12, i), t));
  *rp = wav_complex_div(wav_complex_sub(i, wav_complex_mul(eta_12, t)),
                        wav_complex_add(i, wav_complex_mul(eta_12, t)));
}

}  // namespace waveoptics

CCL_NAMESPACE_END
