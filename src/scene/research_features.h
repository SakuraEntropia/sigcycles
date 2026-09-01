#pragma once

#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Rendering modes (Research Edition spec, section 3). */
enum class RenderMode : uint {
  OFFLINE = 0,
  REALTIME = 1,
  NUM_RENDER_MODES
};

/* Experimental status of a research feature. */
enum class ResearchFeatureStatus {
  STABLE,
  EXPERIMENTAL,
  RESEARCH,
};

/* Metadata for one research feature: supported render modes, required
 * dependencies, incompatible features and experimental status.
 * Dependencies/conflicts are null-terminated id lists. */
struct ResearchFeatureInfo {
  const char *id;
  uint modes;                  /* bitmask of RenderMode values */
  const char *dependencies[4]; /* required features (null-terminated) */
  const char *conflicts[4];    /* incompatible features (null-terminated) */
  ResearchFeatureStatus status;
};

static constexpr uint render_mode_bit(RenderMode mode)
{
  return 1u << (uint)mode;
}

/* Kernel flag bits for enabled research features (mirrors feature ids in
 * the research_features string; consumed by kernel_data.integrator.research_flags). */
static constexpr uint RESEARCH_FEATURE_WAVE_DIFFRACTION = 1u << 0;
static constexpr uint RESEARCH_FEATURE_WAVELENGTH_SAMPLING = 1u << 1;
static constexpr uint RESEARCH_FEATURE_POLARIZATION = 1u << 2;
static constexpr uint RESEARCH_FEATURE_SVGF = 1u << 3;

/* Parse a space-separated feature id list into a kernel flag bitmask.
 * Unknown ids are ignored (validation happens in research_validate_features). */
uint research_features_to_flags(const string &features);

/* Look up feature info by id; nullptr when unknown. */
const ResearchFeatureInfo *research_feature_find(const string &id);

/* Validate an enabled-feature set against the registry: checks that every
 * feature is known, its dependencies are present and no conflicts are
 * enabled. On failure returns false and fills *error with the reason. */
bool research_validate_features(const vector<string> &enabled, string *error);

CCL_NAMESPACE_END
