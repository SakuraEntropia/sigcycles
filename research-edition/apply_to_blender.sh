#!/usr/bin/env bash
# 把 Research Edition 移植补丁应用到 Blender 的 intern/cycles。
#
# 用法：
#   cd <blender-源码根目录>
#   bash research-edition/apply_to_blender.sh /path/to/cycles_research_edition.patch
#
# 说明：
# - Blender 官方 cycles 在 intern/cycles/src/，本仓库的 src/ 与其一一对应。
# - patch 用 git format-patch 生成，路径前缀为 a/src/... b/src/...
#   需要 strip 掉 "src/" 前缀（Blender 内路径就是 src/...，实际一致）。
# - 应用后按 08_blender_integration.md 的 2.2-2.4 节补 Blender 节点/RNA/同步代码。

set -e

PATCH="$1"
if [ -z "$PATCH" ]; then
  echo "usage: $0 <path-to-cycles_research_edition.patch>"
  exit 1
fi

# 目标：Blender 源码树的 intern/cycles
CYCLES_DIR="intern/cycles"
if [ ! -d "$CYCLES_DIR/src" ]; then
  echo "error: $CYCLES_DIR/src not found - run from Blender source root"
  exit 1
fi

echo "Applying Research Edition kernel patches to $CYCLES_DIR ..."
# --exclude 排除 standalone 特有文件（Blender 不用 cycles_xml / examples）
git apply --verbose \
  --directory="$CYCLES_DIR" \
  --exclude='src/app/*' \
  --exclude='examples/*' \
  --exclude='src/waveoptics/wav_test.cpp' \
  "$PATCH"

echo ""
echo "Kernel patches applied. Next steps (see 08_blender_integration.md):"
echo "  1. Add Blender shader nodes: node_shader_wave_diffraction/thin_film.cc"
echo "  2. Register closures in svm/types.h (append, don't reorder)"
echo "  3. Wire integrator sync in cycles_integrator.cc"
echo "  4. make update && make bpy"
