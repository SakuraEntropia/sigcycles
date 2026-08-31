# Cycles-SIGGRAPH Research Edition — 分析文档索引

> 目标仓库：/Users/faputa/Documents/Entro-Cycles/cycles（基线 v5.0.0-461，Apache-2.0）
> 本目录为第 18 节 A-G 项的分析交付物；实现（H 之后）在 Phase 0 基础设施批准后进行。

## 文档

| 文档 | 内容 | 对应第 18 节 |
|---|---|---|
| 01_architecture_analysis.md | Cycles 源码架构分析（integrator/sampling/light/denoise/GPU/BVH/material/volume/realtime/wave-optics 位置）| A/B |
| 02_feature_compatibility_graph.md | 特性兼容/依赖图 + 传输模型规则 + 许可红线 | F |
| 03_architecture_design.md | RenderMode/Feature 系统、Transport 分层、Wave 提升方案、实时工作循环设计 | 3/8（H 前置设计）|
| 04_feature_matrix.md | 特性矩阵（第 16 节，持续更新）| 16 |
| 05_benchmarking.md | 基准测试方法论（离线/实时/波动光学分测）| 15 |
| 06_research_lists.md | 四份研究清单（OFFLINE/REALTIME/WAVE OPTICS/SHARED）| C/D/E |
| 07_phase1_selection.md | 首批实现选择（Phase 0 基础设施 + Phase 1 六项 + Phase 2 四项）+ 红线 | G |

## 调研原始报告（research/）

| 报告 | 覆盖 |
|---|---|
| research/offline_light_transport.md | ReSTIR 系列/Path Guiding/神经采样/自适应/MIS/MLT-BDPT（32 项）|
| research/denoising_and_inverse.md | 神经去噪/时域滤波/可微渲染/逆向渲染/神经材质（约 40 项）|
| research/realtime.md | ReSTIR 实时/时域累积/SVGF/去噪/上采样/实时 GI（7 主题）|
| research/wave_optics.md | 波动光学路径追踪/衍射/干涉/偏振/薄膜/傅里叶/光谱（7 主题）|

## 核心结论速览

1. **Cycles 已内置**：OpenPGL 路径引导（Apache-2.0）、自适应采样、MNEE、OIDN/OptiX 去噪、光树、motion passes——多个研究特性实为已有设施的增量。
2. **许可证是硬约束**：可并入=BSD-3/MIT/Apache-2.0；wave_tracer（CC BY-NC）公式需重实现；NVIDIA 系专有 SDK 只读思路。
3. **波动光学**：数学层已全移植（src/waveoptics）；下一步波长采样（解锁色散）到偏振 AOV、UTD 边衍射、薄膜、再到波束传输（长期）。
4. **实时模式**：从 SVGF + OIDN v3 时域去噪起步（难度 1-2）；ReSTIR DI 中期（依赖 lamp 修复）。
5. **环境问题**：独立构建 lamp 全黑 + environment_texture 品红（预先存在，非本项目引入）——lamp 修复是实时模式前提。
