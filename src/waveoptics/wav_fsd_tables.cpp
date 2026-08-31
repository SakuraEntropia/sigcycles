/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "waveoptics/wav_fsd_tables.h"

#include <cmath>
#include <mutex>

CCL_NAMESPACE_BEGIN

namespace waveoptics {

namespace {

struct fsd_lut_storage {
  wav_fsd_lut a1;
  wav_fsd_lut a2;
  std::once_flag once;
};

fsd_lut_storage &fsd_storage()
{
  static fsd_lut_storage storage;
  return storage;
}

/* Integrand of the lobe density in polar coordinates:
 * |f(r cos t, r sin t)|^2 * chi_e(r) * r  (Jacobian). */
typedef float (*lobe_fn)(float x, float y);

/* Builds inverse-CDF tables for one lobe. */
void build_lut(const lobe_fn fn, wav_fsd_lut *lut)
{
  constexpr int Nt = wav_fsd_lut_n_theta;
  constexpr int Nr = wav_fsd_lut_n_r;
  constexpr int Nsub = 4096; /* quadrature subdivisions for the radius CDF */

  /* 1) Conditional radius CDFs per theta bin. */
  std::vector<std::vector<float>> cond_cdf(Nt, std::vector<float>(Nr + 1, 0.0f));
  std::vector<float> marginal(Nt, 0.0f);

  for (int ti = 0; ti < Nt; ++ti) {
    const float theta = (float(ti) + 0.5f) / float(Nt) * WAV_PI_2_F;
    const float ct = cosf(theta), st = sinf(theta);

    /* Cumulative over radius with dense quadrature, sampled at Nr bins. */
    std::vector<float> acc(Nsub + 1, 0.0f);
    float total = 0.0f;
    const float dr = wav_fsd_rmax / float(Nsub);
    for (int s = 0; s < Nsub; ++s) {
      const float r = (float(s) + 0.5f) * dr;
      const float x = r * ct, y = r * st;
      const float f = fn(x, y);
      total += f * f * wav_fsd_chi_e(make_float2(r, 0.0f)) * r * dr;
      acc[s + 1] = total;
    }
    /* Note: chi_e depends on r only, precompute via make_float2(r, 0). */

    marginal[ti] = total;

    /* Store inverse CDF at Nr uniform u points (0..1). */
    for (int rj = 0; rj < Nr; ++rj) {
      const float u = (float(rj) + 0.5f) / float(Nr);
      const float target = u * total;
      /* Binary search in acc (monotonic). */
      int lo = 0, hi = Nsub;
      while (lo + 1 < hi) {
        const int mid = (lo + hi) / 2;
        if (acc[mid] < target)
          lo = mid;
        else
          hi = mid;
      }
      /* Linear interpolate between acc[lo], acc[hi]. */
      const float rlo = (float(lo) + 0.5f) * dr;
      const float a0 = acc[lo], a1 = acc[hi];
      const float t = (a1 > a0) ? (target - a0) / (a1 - a0) : 0.0f;
      const float rval = rlo + t * dr; /* actual radius in canonical units */
      lut->iCDF[ti][rj] = clamp(rval, 0.0f, wav_fsd_rmax);
    }
  }

  /* 2) Marginal angle CDF over theta in [0, pi/2]. */
  float total_m = 0.0f;
  for (int ti = 0; ti < Nt; ++ti)
    total_m += marginal[ti];
  if (total_m <= 0.0f) {
    for (int ti = 0; ti < Nt; ++ti)
      lut->iCDFtheta[ti] = float(ti) / float(Nt - 1);
    return;
  }
  std::vector<float> cdf(Nt + 1, 0.0f);
  for (int ti = 0; ti < Nt; ++ti)
    cdf[ti + 1] = cdf[ti] + marginal[ti];
  /* Invert: for each u, find theta. */
  for (int ti = 0; ti < Nt; ++ti) {
    const float u = (float(ti) + 0.5f) / float(Nt);
    const float target = u * total_m;
    int lo = 0, hi = Nt;
    while (lo + 1 < hi) {
      const int mid = (lo + hi) / 2;
      if (cdf[mid] < target)
        lo = mid;
      else
        hi = mid;
    }
    const float t = (cdf[hi] > cdf[lo]) ? (target - cdf[lo]) / (cdf[hi] - cdf[lo]) : 0.0f;
    lut->iCDFtheta[ti] = clamp((float(lo) + t) / float(Nt - 1), 0.0f, 1.0f);
  }
  lut->iCDFtheta[Nt - 1] = 1.0f;
}

void build_all()
{
  fsd_lut_storage &s = fsd_storage();
  build_lut(&wav_fsd_alpha1, &s.a1);
  build_lut(&wav_fsd_alpha2, &s.a2);
}

}  // namespace

const wav_fsd_lut *wav_fsd_lut_a1()
{
  fsd_lut_storage &s = fsd_storage();
  std::call_once(s.once, &build_all);
  return &s.a1;
}

const wav_fsd_lut *wav_fsd_lut_a2()
{
  fsd_lut_storage &s = fsd_storage();
  std::call_once(s.once, &build_all);
  return &s.a2;
}

}  // namespace waveoptics

CCL_NAMESPACE_END
