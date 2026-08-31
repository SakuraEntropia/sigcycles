/* SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Double-slit (Young) interference pattern renderer.
 *
 * Computes the far-field ASF of a double slit (width 20 um, height 1 mm,
 * centre separation 100 um, wavelength 550 nm) on a receiver grid and
 * writes a PPM image. The pattern shows the single-slit envelope modulated
 * by the double-slit interference fringes (dark fringes at
 * xi_x = (2n+1)*pi/s_mm, s_mm = 0.1 -> period 2*pi/s_mm = 62.8 in the
 * canonical xi frame).
 *
 * Usage: wav_double_slit [single|double] > out.ppm  (prints P6 PPM to stdout)
 */

#include "waveoptics/wav_fsd.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace ccl;
using namespace ccl::waveoptics;

int main(int argc, char **argv)
{
  const bool single = (argc > 1 && strcmp(argv[1], "single") == 0);

  /* Slit geometry in the canonical mm-scaled frame. */
  const float fsd_unit = 1e-3f;
  const float w_mm = 0.00002f / fsd_unit; /* 20 um slit width */
  const float h_mm = 0.001f / fsd_unit;   /* 1 mm slit height */
  const float s_mm = single ? 0.0f : 0.0001f / fsd_unit; /* 100 um separation */
  const float lambda = 550e-9f;

  wav_fsd_edge edges[4];
  const float half_w = w_mm * 0.5f;
  const float half_s = s_mm * 0.5f;
  edges[0] = wav_fsd_edge{make_float2(0.0f, h_mm), make_float2(-half_s - half_w, 0.0f),
                          wav_make_complex(0.0f, 0.0f), wav_make_complex(0.0f, 1.0f)};
  edges[1] = wav_fsd_edge{make_float2(0.0f, -h_mm), make_float2(-half_s + half_w, 0.0f),
                          wav_make_complex(0.0f, 0.0f), wav_make_complex(0.0f, 1.0f)};
  edges[2] = wav_fsd_edge{make_float2(0.0f, h_mm), make_float2(half_s - half_w, 0.0f),
                          wav_make_complex(0.0f, 0.0f), wav_make_complex(0.0f, 1.0f)};
  edges[3] = wav_fsd_edge{make_float2(0.0f, -h_mm), make_float2(half_s + half_w, 0.0f),
                          wav_make_complex(0.0f, 0.0f), wav_make_complex(0.0f, 1.0f)};
  const int num_edges = single ? 2 : 4;

  /* 0-th order field amplitude via the same 8-point approximation as the
   * closure (wave_aperture_psi02). */
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
  psi02 /= 8.0f;

  /* Receiver grid in the canonical xi frame. xi_x covers the single-slit
   * envelope (+/-2*pi*mm/w = +/-314) resolving the double-slit fringes
   * (period 2*pi/s_mm = 62.8); xi_y stays inside the alpha2 main lobe. */
  const int W = 2000, H = 300;
  const float xmin = -350.0f, xmax = 350.0f;
  const float ymin = -4.0f, ymax = 4.0f;

  std::vector<float> asf(W * H);
  float amax = 0.0f;
  for (int j = 0; j < H; ++j) {
    const float xi_y = ymin + (ymax - ymin) * j / (H - 1);
    for (int i = 0; i < W; ++i) {
      const float xi_x = xmin + (xmax - xmin) * i / (W - 1);
      const float a = wav_fsd_asf(edges, num_edges, psi02, make_float2(xi_x, xi_y));
      asf[j * W + i] = a;
      if (a > amax)
        amax = a;
    }
  }

  /* Logarithmic (dB) tone mapping like the wave_tracer demos. */
  printf("P6\n%d %d\n255\n", W, H);
  const float db_min = -30.0f, db_max = 0.0f;
  for (int j = 0; j < H; ++j) {
    for (int i = 0; i < W; ++i) {
      const float a = asf[j * W + i];
      const float db = 10.0f * log10f(a / amax + 1e-10f);
      float t = (db - db_min) / (db_max - db_min);
      t = fmaxf(0.0f, fminf(1.0f, t));
      const unsigned char v = (unsigned char)(t * 255.0f);
      fputc(v, stdout);
      fputc(v, stdout);
      fputc(v, stdout);
    }
  }
  return 0;
}
