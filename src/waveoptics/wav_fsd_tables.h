/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles integration
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host-side construction of the FSD lobe inverse-CDF tables.
 *
 * The tables are built numerically from the analytic alpha1/alpha2 lobe
 * densities (masked by chi_e) over the canonical domain |zeta| <= RMAX,
 * mirroring the structure of wave_tracer's precomputed LUTs
 * (include/wt/interaction/fsd/fraunhofer/fsd_lut.hpp).
 *
 * Built lazily and thread-safely; host only.
 */

#pragma once

#include "waveoptics/wav_fsd_sampler.h"

#ifdef __KERNEL_GPU__
#  error "wav_fsd_tables.h is host-only"
#endif

#include <mutex>
#include <vector>

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Builds (once) and returns the alpha1/alpha2 lobe tables. */
const wav_fsd_lut *wav_fsd_lut_a1();
const wav_fsd_lut *wav_fsd_lut_a2();

}  // namespace waveoptics

CCL_NAMESPACE_END
