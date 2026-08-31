# Wave Optics Module (src/waveoptics)

Free-space diffraction (FSD), UTD edge diffraction, Fresnel and
polarimetric (Stokes/Mueller) mathematics ported from
[**wave_tracer**](https://github.com/ssteinberg/wave_tracer)
(a wave-optical path tracer by Shlomi Steinberg), integrated into Cycles.

## Contents

| File | Contents | Source |
|------|----------|--------|
| wav_complex.h  | GPU-friendly complex number type | new |
| wav_math.h     | constants + 2D helpers | wt/math/common.hpp |
| wav_cerf.h     | complex complementary error function (series + continued fraction) | replaces libcerf dependency |
| wav_utd.h      | UTD wedge diffraction (a+/- , F transition, wedge UTD) | wt/interaction/fsd/utd.hpp, common.hpp |
| wav_fsd.h      | Fraunhofer FSD angular scattering function (alpha1/2, chi masks, Psi, ASF) | wt/interaction/fsd/fraunhofer/fsd.hpp |
| wav_fsd_sampler.h | importance sampler for the FSD ASF (rejection sampling) | wt/interaction/fsd/fraunhofer/fsd_sampler.cpp + fsd_lut.hpp |
| wav_fsd_tables.h/.cpp | host-built inverse-CDF lobe tables (256x256, |zeta|<=64) | replaces the precomputed data/fsd LUT files |
| wav_fresnel.h  | Fresnel reflection/refraction with complex index | wt/interaction/fresnel.hpp |
| wav_stokes.h   | Stokes parameters vector + Mueller operators | wt/interaction/polarimetric/stokes.hpp, mueller.hpp |
| wav_gaussian.h | Gaussian beam cross-section amplitude | wt/beam/gaussian_wavefront.hpp |
| wav_test.cpp   | standalone numerical validation (all tests pass) | new |

## Usage

The module is consumed by the kernel closure
`src/kernel/closure/bsdf_wave_diffraction.h`, which exposes the slit
diffraction model as a Cycles BSDF node ("Wave Diffraction BSDF",
`wave_diffraction_bsdf` in scene XML / OSL). See
`examples/scene_wave_diffraction.xml` for a complete demo scene.

Build the standalone test:

    clang++ -std=c++20 -O2 -I <repo>/src -I <repo>/third_party/atomic \
        "-DCCL_NAMESPACE_BEGIN=namespace ccl {" "-DCCL_NAMESPACE_END=}" \
        -include arm_neon.h   # macOS arm64 only \
        src/waveoptics/wav_test.cpp src/waveoptics/wav_fsd_tables.cpp -o /tmp/wav_test
    /tmp/wav_test

## Notes / limitations

- **License.** The ported mathematics are derived from wave_tracer,
  Copyright Shlomi Steinberg, licensed **CC BY-NC 4.0** (non-commercial).
  This is *incompatible* with Cycles' Apache-2.0 for redistribution: the
  module keeps the attribution and CC BY-NC terms. Using it in a commercial
  product requires a separate license from the author, or re-implementation
  from the SIGGRAPH 2024 paper "A Free-Space Diffraction BSDF".
- **Units.** Cycles scenes use meters; the model internally rescales to the
  mm-canonical frame used by wave_tracer (`fsd_unit = 1e-3`). Wavelength is
  taken as a scalar closure parameter (nm); the closure is monochromatic and
  does not participate in Cycles' (RGB-limited) spectral pipeline yet.
- **Closure value convention.** Following wave_tracer, the closure's value
  equals its pdf (f = pdf), so sampling is exact and energy-conserving; the
  tan-space Jacobian is not applied (paraxial approximation).
- **Sampling tables.** The lobe inverse-CDF tables are built on the host
  (CPU) at first use. On GPU devices the closure evaluates to zero
  (documented limitation; CPU rendering is the supported path).
- **Domain.** The lobe tables cover |zeta| <= 64 in canonical coordinates;
  apertures whose diffraction fringes lie beyond that range (extreme
  height/width ratios) are truncated. This mirrors the fixed-domain LUT
  approach of the original.
- The UTD edge diffraction (wav_utd.h) is provided as a self-contained
  module for future edge-based diffraction work; the v1 closure uses the
  Fraunhofer FSD model.
