/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Polarimetric optics: Stokes parameters vector and Mueller operators.
 *
 * Ported (units stripped, frames simplified to a rotation angle around the
 * propagation axis) from wave_tracer include/wt/interaction/polarimetric/
 * stokes.hpp and mueller.hpp, Copyright Shlomi Steinberg, CC BY-NC 4.0.
 * See README.md.
 */

#pragma once

#include "util/math_float3.h"
#include "util/math_float4.h"

#include "waveoptics/wav_complex.h"
#include "waveoptics/wav_fresnel.h"
#include "waveoptics/wav_math.h"

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Stokes parameters vector. S = (I, Q, U, V). */
struct wav_stokes {
  float4 S;
};

ccl_device_inline wav_stokes wav_stokes_zero()
{
  return wav_stokes{make_float4(0.0f, 0.0f, 0.0f, 0.0f)};
}

ccl_device_inline wav_stokes wav_stokes_unpolarized(const float I)
{
  return wav_stokes{make_float4(I, 0.0f, 0.0f, 0.0f)};
}

ccl_device_inline float wav_stokes_intensity(const wav_stokes S)
{
  return S.S.x;
}

/* (Q, U, V) */
ccl_device_inline float3 wav_stokes_polarization_state(const wav_stokes S)
{
  return make_float3(S.S.y, S.S.z, S.S.w);
}

ccl_device_inline bool wav_stokes_is_unpolarized(const wav_stokes S)
{
  return wav_stokes_polarization_state(S) == make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline float wav_stokes_polarized_intensity(const wav_stokes S)
{
  return len(wav_stokes_polarization_state(S));
}

ccl_device_inline float wav_stokes_unpolarized_intensity(const wav_stokes S)
{
  return fmaxf(0.0f, S.S.x - wav_stokes_polarized_intensity(S));
}

ccl_device_inline float wav_stokes_degree_of_polarization(const wav_stokes S)
{
  const float I = S.S.x;
  return I > 0.0f ? wav_stokes_polarized_intensity(S) / I : 0.0f;
}

ccl_device_inline wav_stokes wav_stokes_flip_handness(const wav_stokes S)
{
  return wav_stokes{make_float4(S.S.x, S.S.y, -S.S.z, -S.S.w)};
}

/* Reorients the Stokes vector by a frame rotation of angle (radians) around
 * the propagation axis: the (Q, U) components rotate by 2*angle. */
ccl_device_inline wav_stokes wav_stokes_reorient(const wav_stokes S, const float angle)
{
  const float c = cosf(2.0f * angle);
  const float s = sinf(2.0f * angle);
  const float Q = S.S.y, U = S.S.z;
  return wav_stokes{make_float4(S.S.x, c * Q - s * U, s * Q + c * U, S.S.w)};
}

ccl_device_inline wav_stokes wav_stokes_linearly_polarized_0deg(const float I)
{
  return wav_stokes{make_float4(I, I, 0.0f, 0.0f)};
}
ccl_device_inline wav_stokes wav_stokes_linearly_polarized_45deg(const float I)
{
  return wav_stokes{make_float4(I, 0.0f, I, 0.0f)};
}
ccl_device_inline wav_stokes wav_stokes_linearly_polarized_90deg(const float I)
{
  return wav_stokes{make_float4(I, -I, 0.0f, 0.0f)};
}
ccl_device_inline wav_stokes wav_stokes_circularly_polarized(const bool rhc, const float I)
{
  return wav_stokes{make_float4(I, 0.0f, 0.0f, rhc ? I : -I)};
}

ccl_device_inline wav_stokes wav_stokes_add(const wav_stokes a, const wav_stokes b)
{
  return wav_stokes{make_float4(a.S.x + b.S.x, a.S.y + b.S.y, a.S.z + b.S.z, a.S.w + b.S.w)};
}

ccl_device_inline wav_stokes wav_stokes_mul_real(const wav_stokes a, const float s)
{
  return wav_stokes{make_float4(a.S.x * s, a.S.y * s, a.S.z * s, a.S.w * s)};
}

/* 4x4 Mueller matrix, stored row-major (rows[0..3]). */
struct wav_mueller {
  float4 rows[4];
};

ccl_device_inline wav_mueller wav_mueller_identity()
{
  wav_mueller M;
  M.rows[0] = make_float4(1, 0, 0, 0);
  M.rows[1] = make_float4(0, 1, 0, 0);
  M.rows[2] = make_float4(0, 0, 1, 0);
  M.rows[3] = make_float4(0, 0, 0, 1);
  return M;
}

ccl_device_inline wav_mueller wav_mueller_handness_flip()
{
  wav_mueller M = wav_mueller_identity();
  M.rows[2] = make_float4(0, 0, -1, 0);
  M.rows[3] = make_float4(0, 0, 0, -1);
  return M;
}

/* Mueller rotation operator: rotates from tangent t1 to tangent t2
 * (angle = angle(t1 -> t2)). */
ccl_device_inline wav_mueller wav_mueller_rotation(const float angle)
{
  const float c = cosf(2.0f * angle);
  const float s = sinf(2.0f * angle);
  wav_mueller M = wav_mueller_identity();
  M.rows[1] = make_float4(0, c, -s, 0);
  M.rows[2] = make_float4(0, s, c, 0);
  return M;
}

/* Linear polarizer with polarization angle theta (radians). */
ccl_device_inline wav_mueller wav_mueller_linear_polarizer(const float theta)
{
  const float s = sinf(2.0f * theta);
  const float c = cosf(2.0f * theta);
  wav_mueller P = wav_mueller_identity();
  P.rows[0] = make_float4(1, c, s, 0);
  P.rows[1] = make_float4(c, c * c, c * s, 0);
  P.rows[2] = make_float4(s, c * s, s * s, 0);
  P.rows[3] = make_float4(0, 0, 0, 0);
  for (int i = 0; i < 4; ++i)
    P.rows[i] = make_float4(P.rows[i].x * 0.5f, P.rows[i].y * 0.5f, P.rows[i].z * 0.5f, P.rows[i].w * 0.5f);
  return P;
}

ccl_device_inline wav_mueller wav_mueller_perfect_depolarizer()
{
  wav_mueller P = wav_mueller_identity();
  P.rows[1] = make_float4(0, 0, 0, 0);
  P.rows[2] = make_float4(0, 0, 0, 0);
  P.rows[3] = make_float4(0, 0, 0, 0);
  return P;
}

/* Mueller matrix of a Fresnel interaction (reflection or transmission).
 * fs/fp: complex s/p Fresnel coefficients. */
ccl_device_inline wav_mueller wav_mueller_fresnel(const wav_complex fs, const wav_complex fp)
{
  const float Rs = wav_complex_norm(fs);
  const float Rp = wav_complex_norm(fp);
  const float m00 = (Rs + Rp) * 0.5f;
  const float m01 = (Rs - Rp) * 0.5f;
  const float m22 = fp.re * fs.re + fp.im * fs.im;  /* Re(fp * conj(fs)) */
  const float m23 = fp.im * fs.re - fp.re * fs.im;  /* Im(fp * conj(fs)) */

  wav_mueller M;
  M.rows[0] = make_float4(m00, m01, 0, 0);
  M.rows[1] = make_float4(m01, m00, 0, 0);
  M.rows[2] = make_float4(0, 0, m22, -m23);
  M.rows[3] = make_float4(0, 0, m23, m22);
  return M;
}

/* Mueller matrix of a Fresnel reflection. */
ccl_device_inline wav_mueller wav_mueller_fresnel_reflection(const wav_complex eta_12,
                                                             const float3 w,
                                                             const float3 n)
{
  wav_complex rs, rp;
  wav_fresnel_reflection(eta_12, w, n, &rs, &rp);
  return wav_mueller_fresnel(rs, rp);
}

ccl_device_inline wav_mueller wav_mueller_mul_real(const wav_mueller M, const float s)
{
  wav_mueller R;
  for (int i = 0; i < 4; ++i)
    R.rows[i] = make_float4(M.rows[i].x * s, M.rows[i].y * s, M.rows[i].z * s, M.rows[i].w * s);
  return R;
}

/* Mueller matrix of a Fresnel transmission. */
ccl_device_inline wav_mueller wav_mueller_fresnel_transmission(const wav_complex eta_12,
                                                               const float3 w,
                                                               const float3 n)
{
  const wav_fresnel_ret f = wav_fresnel(eta_12, w, n);
  return wav_mueller_mul_real(wav_mueller_fresnel(f.ts, f.tp), f.Z);
}

ccl_device_inline wav_mueller wav_mueller_add(const wav_mueller a, const wav_mueller b)
{
  wav_mueller R;
  for (int i = 0; i < 4; ++i)
    R.rows[i] = make_float4(a.rows[i].x + b.rows[i].x, a.rows[i].y + b.rows[i].y,
                            a.rows[i].z + b.rows[i].z, a.rows[i].w + b.rows[i].w);
  return R;
}

/* Matrix composition: (A*B)[i][j] = sum_k A[i][k] B[k][j]. */
ccl_device_inline wav_mueller wav_mueller_compose(const wav_mueller a, const wav_mueller b)
{
  wav_mueller R;
  for (int i = 0; i < 4; ++i) {
    float r[4] = {0, 0, 0, 0};
    const float4 ra = a.rows[i];
    for (int k = 0; k < 4; ++k) {
      const float aik = (k == 0) ? ra.x : (k == 1) ? ra.y : (k == 2) ? ra.z : ra.w;
      const float4 rb = b.rows[k];
      r[0] += aik * rb.x;
      r[1] += aik * rb.y;
      r[2] += aik * rb.z;
      r[3] += aik * rb.w;
    }
    R.rows[i] = make_float4(r[0], r[1], r[2], r[3]);
  }
  return R;
}

/* Apply Mueller matrix to a Stokes vector (same frame). */
ccl_device_inline wav_stokes wav_mueller_apply(const wav_mueller M, const wav_stokes S)
{
  float r[4] = {0, 0, 0, 0};
  const float s[4] = {S.S.x, S.S.y, S.S.z, S.S.w};
  for (int i = 0; i < 4; ++i) {
    const float4 row = M.rows[i];
    r[i] = row.x * s[0] + row.y * s[1] + row.z * s[2] + row.w * s[3];
  }
  return wav_stokes{make_float4(r[0], r[1], r[2], r[3])};
}

}  // namespace waveoptics

CCL_NAMESPACE_END
