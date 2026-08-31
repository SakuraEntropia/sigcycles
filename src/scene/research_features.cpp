/* SPDX-FileCopyrightText: 2025 Entro-Cycles Research Edition
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Research feature registry and validation (spec sections 5-7).
 * Every research feature declares: supported render modes, dependencies,
 * conflicts and experimental status. Enabling conflicting features fails
 * clearly instead of silently picking one. */

#include "scene/research_features.h"

CCL_NAMESPACE_BEGIN

namespace {

const ResearchFeatureInfo feature_registry[] = {
    /* Built-in, always available. */
    {"path_tracing",
     render_mode_bit(RenderMode::OFFLINE) | render_mode_bit(RenderMode::REALTIME),
     {nullptr},
     {"wave_transport", nullptr},
     ResearchFeatureStatus::STABLE},
    {"path_guiding",
     render_mode_bit(RenderMode::OFFLINE),
     {nullptr},
     {nullptr},
     ResearchFeatureStatus::STABLE},
    {"adaptive_sampling",
     render_mode_bit(RenderMode::OFFLINE),
     {nullptr},
     {nullptr},
     ResearchFeatureStatus::STABLE},
    {"mnee_caustics",
     render_mode_bit(RenderMode::OFFLINE),
     {nullptr},
     {nullptr},
     ResearchFeatureStatus::STABLE},

    /* Wave optics track (implemented). */
    {"wave_diffraction",
     render_mode_bit(RenderMode::OFFLINE),
     {nullptr},
     {nullptr},
     ResearchFeatureStatus::EXPERIMENTAL},

    /* Planned / research track. */
    {"wave_transport",
     render_mode_bit(RenderMode::OFFLINE),
     {"wave_diffraction", nullptr},
     {"path_tracing", "restir_di", nullptr},
     ResearchFeatureStatus::RESEARCH},
    {"wavelength_sampling",
     render_mode_bit(RenderMode::OFFLINE),
     {nullptr},
     {nullptr},
     ResearchFeatureStatus::EXPERIMENTAL},
    {"polarization",
     render_mode_bit(RenderMode::OFFLINE),
     {"wavelength_sampling", nullptr},
     {nullptr},
     ResearchFeatureStatus::EXPERIMENTAL},
    {"restir_di",
     render_mode_bit(RenderMode::OFFLINE) | render_mode_bit(RenderMode::REALTIME),
     {nullptr},
     {"wave_transport", nullptr},
     ResearchFeatureStatus::RESEARCH},
    {"svgf",
     render_mode_bit(RenderMode::REALTIME),
     {nullptr},
     {nullptr},
     ResearchFeatureStatus::EXPERIMENTAL},
};

}  // namespace

const ResearchFeatureInfo *research_feature_find(const string &id)
{
  for (const ResearchFeatureInfo &info : feature_registry) {
    if (id == info.id) {
      return &info;
    }
  }
  return nullptr;
}

bool research_validate_features(const vector<string> &enabled, string *error)
{
  for (const string &id : enabled) {
    const ResearchFeatureInfo *info = research_feature_find(id);
    if (info == nullptr) {
      *error = string_printf("Unknown research feature \"%s\"", id.c_str());
      return false;
    }

    /* Dependencies. */
    for (const char *dep : info->dependencies) {
      if (dep == nullptr) {
        break;
      }
      const string dep_id(dep);
      const bool has_dep = std::find(enabled.begin(), enabled.end(), dep_id) != enabled.end();
      if (!has_dep) {
        *error = string_printf(
            "Cannot enable \"%s\": requires feature \"%s\"", id.c_str(), dep);
        return false;
      }
    }

    /* Conflicts. */
    for (const char *conflict : info->conflicts) {
      if (conflict == nullptr) {
        break;
      }
      const string conflict_id(conflict);
      if (std::find(enabled.begin(), enabled.end(), conflict_id) != enabled.end()) {
        *error = string_printf(
            "Cannot enable \"%s\" together with \"%s\": "
            "these features use incompatible transport models",
            id.c_str(), conflict);
        return false;
      }
    }
  }
  return true;
}

CCL_NAMESPACE_END
