/*
 * Standalone sanity test for the waveoptics module.
 */

#include "waveoptics/wav_cerf.h"
#include "waveoptics/wav_fsd.h"
#include "waveoptics/wav_fsd_sampler.h"
#include "waveoptics/wav_fsd_tables.h"
#include "waveoptics/wav_fresnel.h"
#include "waveoptics/wav_gaussian.h"
#include "waveoptics/wav_stokes.h"
#include "waveoptics/wav_utd.h"
#include "kernel/closure/bsdf_wave_diffraction.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace ccl;
using namespace ccl::waveoptics;

static int failures = 0;

static void check(const char *name, bool ok)
{
  std::cout << name << " " << (ok ? "OK" : "FAIL") << std::endl;
  if (!ok)
    failures++;
}

int main()
{
  /* cerfc sanity */
  {
    const wav_complex c = wav_cerfc(wav_make_complex(1.0f, 0.0f));
    check("cerfc(1) real ~= 0.157299", fabsf(c.re - 0.1572992070502851f) < 1e-4f && fabsf(c.im) < 1e-5f);
    const wav_complex ci = wav_cerfc(wav_make_complex(0.0f, 1.0f));
    check("cerfc(i) ~= 1 - 1.6504i", fabsf(ci.re - 1.0f) < 1e-3f && fabsf(ci.im + 1.650425758797543f) < 1e-3f);
    const wav_complex c10 = wav_cerfc(wav_make_complex(10.0f, 0.0f));
    check("cerfc(10) small positive", c10.re > 0.0f && c10.re < 1e-40f && fabsf(c10.im) < 1e-6f);
  }

  /* UTD transition function */
  {
    const wav_complex f0 = wav_utd_f(0.0f);
    check("UTDF(0) ~= 0", wav_complex_abs(f0) < 1e-4f);
    const wav_complex fbig = wav_utd_f(50.0f);
    check("UTDF(50) ~= 1", fabsf(fbig.re - 1.0f) < 1e-2f && fabsf(fbig.im) < 1e-2f);
  }

  /* UTD wedge */
  {
    wav_wedge_edge wedge;
    wedge.v = make_float3(0, 0, 0);
    wedge.l = 1.0f;
    wedge.nff = make_float3(0, 0, 1);
    wedge.tff = make_float3(1, 0, 0);
    wedge.nbf = make_float3(0, 0, -1);
    wedge.alpha = 0.5f * WAV_PI_F;
    wedge.eta = 1.0f;

    const float k = 2.0f * WAV_PI_F / 0.00055f;
    const float3 wi = make_float3(0, 0, -1);
    const float3 wo = make_float3(0.3f, 0.0f, -0.95f);
    const wav_utd_ret utd = wav_wedge_utd(wedge, k, wi, wo, 1.0f);
    check("UTD finite", isfinite(utd.Ds.re) && isfinite(utd.Ds.im) && isfinite(utd.Dh.re) && isfinite(utd.Dh.im));
  }

  /* LUT build + sampling */
  {
    const wav_fsd_lut *lut2 = wav_fsd_lut_a2();
    const wav_fsd_lut *lut1 = wav_fsd_lut_a1();
    check("LUTs built", lut1 != nullptr && lut2 != nullptr);

    uint32_t rng = 12345u;
    float mean_r = 0.0f;
    int n = 200000;
    bool all_ok = true;
    for (int i = 0; i < n; ++i) {
      float2 z;
      if (!wav_fsd_lut_sample(lut2, &rng, &z)) {
        all_ok = false;
        break;
      }
      mean_r += sqrtf(z.x * z.x + z.y * z.y);
    }
    check("lut sample works", all_ok);
    mean_r /= (float)n;
    check("alpha2 mean radius sane", mean_r > 0.2f && mean_r < 40.0f);
    std::cout << "  mean |zeta| = " << mean_r << std::endl;
  }

  /* Fresnel */
  {
    const wav_fresnel_ret f = wav_fresnel(
        wav_make_complex(1.5f, 0.0f), make_float3(0, 0, -1), make_float3(0, 0, 1));
    check("fresnel normal incidence Ts~0.96", fabsf(f.Ts - 0.96f) < 0.02f && fabsf(f.Tp - 0.96f) < 0.02f);
  }

  /* Stokes/Mueller */
  {
    /* Ideal linear polarizer at 0deg passes half of unpolarized light,
     * with Q = +0.5 (linear polarization along 0deg). */
    const wav_mueller P = wav_mueller_linear_polarizer(0.0f);
    const wav_stokes S2 = wav_mueller_apply(P, wav_stokes_unpolarized(1.0f));
    check("polarizer 0deg: I=0.5, Q=0.5", fabsf(S2.S.x - 0.5f) < 1e-5f && fabsf(S2.S.y - 0.5f) < 1e-5f);
    /* 0deg-polarized input passes fully. */
    const wav_stokes S3 = wav_mueller_apply(P, wav_stokes_linearly_polarized_0deg(1.0f));
    check("polarizer 0deg: polarized input passes", fabsf(S3.S.x - 1.0f) < 1e-5f && fabsf(S3.S.y - 1.0f) < 1e-5f);
  }

  /* Slit aperture */
  {
    const float w = 0.0002f;      /* slit width 0.2mm in meters */
    const float h = 0.001f;       /* slit height 1mm */
    const float fsd_unit = 1e-3f;
    const float lambda = 550e-9f;
    const float k = 2.0f * WAV_PI_F / lambda;
    const float scale = k * fsd_unit;

    wav_fsd_edge edges[2];
    edges[0].e = make_float2(0.0f, h / fsd_unit);
    edges[0].v = make_float2(-w / 2.0f / fsd_unit, 0.0f);
    edges[0].a_b = wav_make_complex(0.0f, 0.0f);
    edges[0].iab_2 = wav_make_complex(0.0f, 1.0f);
    edges[1].e = make_float2(0.0f, -h / fsd_unit);
    edges[1].v = make_float2(w / 2.0f / fsd_unit, 0.0f);
    edges[1].a_b = wav_make_complex(0.0f, 0.0f);
    edges[1].iab_2 = wav_make_complex(0.0f, 1.0f);

    float psi02 = 0.0f;
    {
      const float p0s = wav_fsd_p0_sigma;
      const float2 pts[8] = {
          make_float2(-0.70710678f, -0.70710678f), make_float2(-1, 0),
          make_float2(-0.70710678f, 0.70710678f), make_float2(0, 1),
          make_float2(0.70710678f, 0.70710678f), make_float2(1, 0),
          make_float2(0.70710678f, -0.70710678f), make_float2(0, -1)};
      for (int i = 0; i < 8; ++i)
        psi02 += wav_fsd_asf_unclamped(edges, 2, make_float2(pts[i].x * 3.0f * p0s, pts[i].y * 3.0f * p0s));
      psi02 /= 8.0f;
    }
    const float P0 = wav_fsd_p0(psi02);

    const float asf_center = wav_fsd_asf(edges, 2, psi02, make_float2(0.0f, 0.0f));
    const float xi_first_min = scale * (lambda / w);
    const float asf_min = wav_fsd_asf(edges, 2, psi02, make_float2(xi_first_min, 0.0f));
    check("slit ASF: center max", asf_center > 0.0f);
    check("slit ASF: first min ~ 0", asf_min < 0.05f * asf_center);
    std::cout << "  asf_center=" << asf_center << " asf_first_min=" << asf_min
              << " xi_min=" << xi_first_min << std::endl;

    wav_fsd_aperture ap;
    ap.edges = edges;
    ap.num_edges = 2;
    ap.psi02 = psi02;
    ap.P0 = P0;
    ap.recp_I = 1.0f / (P0 + wav_fsd_pj(edges[0]) + wav_fsd_pj(edges[1]));
    ap.P0_pdf = P0 * ap.recp_I;
    float edge_pdfs[2];
    edge_pdfs[0] = wav_fsd_pj(edges[0]) * ap.recp_I;
    edge_pdfs[1] = wav_fsd_pj(edges[1]) * ap.recp_I;
    ap.edge_pdfs = edge_pdfs;

    /* 2D cell comparison away from the singular near-axis ridge. */
    uint32_t rng = 987654u;
    const int ns = 400000;
    const int NC = 24;
    const float cmin = -48.0f, cmax = 48.0f;
    std::vector<int> counts(NC * NC, 0);
    int ok_s = 0;
    for (int i = 0; i < ns; ++i) {
      wav_fsd_sample_ret ret;
      if (wav_fsd_sample(ap, rng + i, &ret)) {
        ok_s++;
        const int bx = (int)((ret.xi.x - cmin) / (cmax - cmin) * NC);
        const int by = (int)((ret.xi.y - cmin) / (cmax - cmin) * NC);
        if (bx >= 0 && bx < NC && by >= 0 && by < NC)
          counts[by * NC + bx]++;
      }
    }
    check("sampler produces samples", ok_s > ns / 2);

    /* Analytic 2D cell fractions from ASF * recp_I, integrated over cells. */
    std::vector<float> cell_frac(NC * NC, 0.0f);
    {
      const int NQ = 8;
      float total = 0.0f;
      for (int cy = 0; cy < NC; ++cy) {
        for (int cx = 0; cx < NC; ++cx) {
          const float xa = cmin + (cmax - cmin) * cx / NC;
          const float xb = cmin + (cmax - cmin) * (cx + 1) / NC;
          const float ya = cmin + (cmax - cmin) * cy / NC;
          const float yb = cmin + (cmax - cmin) * (cy + 1) / NC;
          float s = 0.0f;
          for (int qy = 0; qy < NQ; ++qy) {
            const float y = ya + (yb - ya) * (qy + 0.5f) / NQ;
            for (int qx = 0; qx < NQ; ++qx) {
              const float x = xa + (xb - xa) * (qx + 0.5f) / NQ;
              s += wav_fsd_asf(edges, 2, psi02, make_float2(x, y)) * ap.recp_I *
                   ((cmax - cmin) / NC) * ((cmax - cmin) / NC) / (NQ * NQ);
            }
          }
          cell_frac[cy * NC + cx] = s;
          total += s;
        }
      }
      for (int i = 0; i < NC * NC; ++i)
        cell_frac[i] /= total;
    }

    /* Compare only cells with |x| > 5 and |y| > 5 (away from the ridge). */
    float max_dev = 0.0f;
    int compared = 0;
    for (int cy = 0; cy < NC; ++cy) {
      for (int cx = 0; cx < NC; ++cx) {
        const float xc = cmin + (cmax - cmin) * (cx + 0.5f) / NC;
        const float yc = cmin + (cmax - cmin) * (cy + 0.5f) / NC;
        if (fabsf(xc) < 5.0f || fabsf(yc) < 5.0f)
          continue;
        const float emp = (float)counts[cy * NC + cx] / (float)ok_s;
        max_dev = fmaxf(max_dev, fabsf(emp - cell_frac[cy * NC + cx]));
        compared++;
      }
    }
    check("sampler matches ASF 2D cells (max dev < 0.01)", compared > 0 && max_dev < 0.01f);
    std::cout << "  sampler acceptance=" << ok_s << "/" << ns << " 2D max_dev=" << max_dev
              << " over " << compared << " cells" << std::endl;
  }

  /* Dispersion: the three-band eval produces channel-dependent ASFs. */
  {
    WaveDiffractionBsdf b;
    b.N = make_float3(0, 0, 1);
    b.T = make_float3(0, 1, 0);
    b.width = 0.0001f;
    b.height = 0.002f;
    b.wavelength = 550.0f;
    b.dispersion = 1.0f;
    b.polarizer_angle = -1.0f;
    b.polarized_input = 0.0f;

    const float3 wi = make_float3(0, 0, 1);
    float pdf = 0.0f;
    Spectrum s = zero_spectrum();
    float3 wo = make_float3(0, 0, -1);
    /* Scan off-axis directions: the dispersion shifts the fringe pattern per
     * wavelength, so the R/G/B channels must differ substantially somewhere. */
    float max_rel_diff = 0.0f;
    float max_val = 0.0f;
    const int N = 200;
    for (int i = 1; i < N; ++i) {
      const float zeta = 1e-4f + (8e-3f - 1e-4f) * i / N;
      const float dir = zeta / sqrtf(1.0f + zeta * zeta);
      /* Diagonal off-axis direction: both in-plane components are non-zero so
       * the alpha2 lobe (which has a zero on the xi_y=0 line) evaluates. */
      const float d = dir * 0.70710678f;
      wo = make_float3(d, d, -sqrtf(1.0f - 2.0f * d * d));
      s = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
      max_val = fmaxf(max_val, fmaxf(fabsf(s.x), fabsf(s.z)));
      const float denom = fmaxf(fabsf(s.x), fabsf(s.z));
      if (denom > 1e-12f) {
        max_rel_diff = fmaxf(max_rel_diff, fabsf(s.x - s.z) / denom);
      }
    }
    check("dispersion: channels finite", isfinite(s.x) && isfinite(s.y) && isfinite(s.z));
    check("dispersion: pdf matches G channel", fabsf(pdf - s.y) < 1e-6f);
    check("dispersion: R != B relatively (max rel diff > 0.1)",
          max_rel_diff > 0.1f && max_val > 1e-6f);
    std::cout << "  max R-B relative diff over scan = " << max_rel_diff
              << " (max val " << max_val << ")" << std::endl;

    /* Mono mode: flat spectrum, pdf = value. */
    b.dispersion = 0.0f;
    const Spectrum sm = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
    check("mono: flat spectrum", fabsf(sm.x - sm.y) < 1e-6f && fabsf(sm.y - sm.z) < 1e-6f);
    check("mono: pdf = value", fabsf(pdf - sm.x) < 1e-6f);
  }

  /* Polarizer: Malus's law. Linearly polarized input through a linear
   * polarizer at angle theta -> cos^2(theta); unpolarized -> 0.5. */
  {
    WaveDiffractionBsdf b;
    b.N = make_float3(0, 0, 1);
    b.T = make_float3(0, 1, 0);
    b.width = 0.0001f;
    b.height = 0.002f;
    b.wavelength = 550.0f;
    b.dispersion = 0.0f;
    b.polarized_input = 1.0f;

    const float3 wi = make_float3(0, 0, 1);
    const float3 wo = make_float3(0, 0, -1); /* straight through */
    float pdf = 0.0f;

    /* Baseline (no polarizer): full transmission. */
    b.polarizer_angle = -1.0f; /* disabled */
    const Spectrum s0 = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);

    /* Theta = 0: cos^2(0) = 1 -> same as baseline. */
    b.polarizer_angle = 0.0f;
    const Spectrum s0d = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
    check("malus: theta=0 full transmission", fabsf(s0d.x - s0.x) < 1e-5f);

    /* Theta = pi/4: cos^2(pi/4) = 0.5. */
    b.polarizer_angle = 0.25f * WAV_PI_F;
    const Spectrum s45 = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
    check("malus: theta=45deg half transmission",
          fabsf(s45.x - 0.5f * s0.x) < 1e-4f);

    /* Theta = pi/2: cos^2(pi/2) = 0 (crossed polarizers). */
    b.polarizer_angle = 0.5f * WAV_PI_F;
    const Spectrum s90 = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
    check("malus: theta=90deg zero transmission", fabsf(s90.x) < 1e-5f);

    /* Unpolarized input: 0.5 regardless of angle. */
    b.polarized_input = 0.0f;
    b.polarizer_angle = 0.0f;
    const Spectrum su0 = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
    b.polarizer_angle = 0.25f * WAV_PI_F;
    const Spectrum su45 = bsdf_wave_diffraction_eval((const ShaderClosure *)&b, wi, wo, &pdf);
    check("malus: unpolarized -> 0.5", fabsf(su0.x - 0.5f * s0.x) < 1e-5f);
    check("malus: unpolarized angle-independent", fabsf(su0.x - su45.x) < 1e-6f);
  }

  if (failures == 0) {
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
  }
  std::cout << failures << " FAILURES" << std::endl;
  return 1;
}