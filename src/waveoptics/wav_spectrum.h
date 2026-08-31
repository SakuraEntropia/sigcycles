/*
 * SPDX-FileCopyrightText: 2025 Entro-Cycles Research Edition
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wavelength sampling helpers for the wave optics track.
 *
 * The v1 implementation uses a three-band dispersion model: the diffraction
 * pattern is evaluated at the three sRGB primary dominant wavelengths and
 * each band is assigned to the matching RGB channel, so white light through
 * an aperture naturally shows chromatic dispersion. Full per-path stochastic
 * wavelength sampling with a CIE XYZ colour-matching pipeline is future work.
 */

#pragma once

CCL_NAMESPACE_BEGIN

namespace waveoptics {

/* Dominant wavelengths (nm) of the sRGB primaries. */
static constexpr float wav_spectrum_primary_red = 611.36f;
static constexpr float wav_spectrum_primary_green = 549.15f;
static constexpr float wav_spectrum_primary_blue = 464.28f;

/* Wavelengths (nm) used for three-band dispersion evaluation, in
 * (R, G, B) channel order. */
ccl_device_inline void wav_spectrum_three_band(float *wavelengths_out)
{
  wavelengths_out[0] = wav_spectrum_primary_red;
  wavelengths_out[1] = wav_spectrum_primary_green;
  wavelengths_out[2] = wav_spectrum_primary_blue;
}

}  // namespace waveoptics

CCL_NAMESPACE_END
