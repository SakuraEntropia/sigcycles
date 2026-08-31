# 特性矩阵（第 16 节，持续更新）

| # | Feature | Offline | Realtime | Wave | Enabled | Status | 参考实现 | 许可 |
|---|---|---|---|---|---|---|---|---|
| 1 | Classic Path Tracing | ✓ | (✓) | - | ✓ | Stable | 基线 | Apache-2.0 |
| 2 | Path Guiding (OpenPGL) | ✓ | - | - | - | Stable(已有) | OpenPGL | Apache-2.0 |
| 3 | Adaptive Sampling | ✓ | - | - | - | Stable(已有) | 自带 | - |
| 4 | MNEE 焦散 | ✓ | - | - | - | Stable(已有) | 自带 | - |
| 5 | Wave Diffraction BSDF | (✓) | - | ✓ | ✓ | Experimental | waveoptics(CC-NC 重实现) | Apache-2.0 侧 |
| 6 | 波长采样/光谱化 | (✓) | - | ✓ | - | Research(Phase1) | pbrt-v4 | Apache-2.0 |
| 7 | Stokes 偏振 | - | - | ✓ | - | Research(Phase1) | mitsuba3 | BSD-3 |
| 8 | SVGF | - | ✓ | - | - | Research(Phase1) | 算法公开 | 重写 |
| 9 | OIDN v3 时域去噪 | ✓ | ✓ | - | - | Research(Phase1) | RenderKit/oidn | Apache-2.0 |
| 10 | Efficiency-Aware MIS | ✓ | - | - | - | Research(Phase1) | EAMIS 2022 | 理论 |
| 11 | 自适应采样升级 | ✓ | - | - | - | Research(Phase1) | DAAS 2023 | 参考 |
| 12 | 薄膜多层干涉 | (✓) | - | ✓ | - | Research(Phase2) | 重实现 | - |
| 13 | GRIS 离线重采样 | ✓ | - | - | - | Research(Phase2) | DQLin/ReSTIR_PT | BSD-3 |
| 14 | PSSMLT | ✓ | - | - | - | Research(Phase2) | pbrt-v3 | BSD-2 |
| 15 | ReSTIR DI | ✓ | ✓ | - | - | Research(Phase2) | 重写 | BSD-3 参考 |
| 16 | Wave Transport(cone/beam) | - | - | ✓ | - | Research(长期) | wave_tracer(重实现) | - |
| 17 | 神经采样/去噪/材质 | ✓ | ✓ | - | - | Research(外部) | 见 06 清单 | 多受限 |
| 18 | 可微渲染 | ✓ | - | - | - | Research(外部) | mitsuba3/redner | BSD-3/MIT |

更新规则：每集成一个特性即更新本表。

## 附录：已确认的环境问题（影响特性选型）

1. ✅ 已修复：lamp 光源全黑根因 = XML 忽略 light tfm 属性（光源在原点）→ commit dde0911f7；ReSTIR 等 lamp 依赖特性现在可用。
2. **environment_texture 色彩空间 bug**（品红）——环境贴图类特性受影响。
3. 两者均为预先存在（原始未改动构建同样复现），非本项目引入。
