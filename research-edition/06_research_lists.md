# 研究清单（E 项：四份列表）

> 综合四份调研报告（research/ 目录）：offline_light_transport.md、denoising_and_inverse.md、realtime.md、wave_optics.md。
> 全部代码 URL 已经报告撰写者实测验证；许可证以各仓库 LICENSE 原文为准。

## 许可速查（决定'可并入'还是'只读思路'）

| 可并入（Apache-2.0 兼容） | 只读思路（许可证禁止并入） |
|---|---|
| BSD-3: DQLin/ReSTIR_PT、VolumetricReSTIR、ReservoirSplatting、ReSTCV、BMFR、Area-ReSTIR、Manifold-Hybrid-Shift、mitsuba3、PSDR | GPL-3.0: practical-path-guiding |
| MIT: FSR2/3/4、RealTimeStochasticLightcuts、BMFR、WSKPD、Neural Bilateral Grid、NeuMERL、Langevin MCMC、SmallVCM | Nvidia Source Code License-NC: conditional-restir-prototype |
| Apache-2.0: OIDN、OpenPGL、Embree、pbrt-v3/v4、Sionna、NeRFactor、InvRender、Practical Inverse Rendering | 专有 SDK: RTXDI、RTXGI、NRD、OptiX Denoiser、DLSS、XeSS |
| | CC BY-NC 4.0: wave_tracer（公式可重实现） |
| | 无 LICENSE 文件（保留版权）：一律只读思路 |

---

## 1. OFFLINE（离线研究清单）

| 优先级 | 技术 | 论文/年份 | 参考代码 | 难度 | 备注 |
|---|---|---|---|---|---|
| P0 | Path Guiding 深度集成 | Path Guiding in Production (2019)；SIGGRAPH 2025 课程 | OpenPGL（Apache-2.0，已集成）| 1-2 | Cycles 3.4+ 已有 OpenPGL；做 guided MIS/体积引导/训练调度增量 |
| P0 | 自适应采样升级 | DAAS (2023)；Practical Error Estimation (2024) | Cycles 自带 src/integrator/adaptive_sampling.cpp | 2 | 纯宿主侧改进 |
| P0 | Efficiency-Aware MIS | EAMIS (2022) | 理论公开（C# 参考无 LIC）| 2-3 | 方差最优 MIS 权重，kernel 定点改 |
| P1 | GRIS/RIS 离线 many-light 重采样 | GRIS (2022) | DQLin/ReSTIR_PT（BSD-3）| 4 | 借鉴 reservoir 理论，不做帧间复用；配 light tree |
| P1 | PSSMLT/MLT 可选积分器 | Veach (1997)；Langevin MCMC (2020) | pbrt-v3（BSD-2）；Langevin（MIT）| 3-4 | 新增 integrator 类型，CPU 为主 |
| P2 | 神经重要性采样 | NIS (2022)；NPIS (2024)；NIS-ML (2025) | 多无官方代码 | 4-5 | 需 ML 推理管线 |
| P2 | Gradient-domain | G-PT (2015)；TG-PT (2016) | 复现无 LIC | 4-5 | 需梯度路径 + Poisson 重建 |
| P2 | VCM | SmallVCM (2012) | SmallVCM（MIT）| 4-5 | 光子合并结构，与现内核差异大 |
| P2 | 神经去噪（离线） | RDFC (2019)；BMFR (2023)；扩散去噪 (3DV'25) | BMFR（MIT）| 4 | 建议外部/ONNX 后端 |
| P2 | 可微渲染 | OpenDR/redner/Mitsuba3 | redner（MIT）；mitsuba3（BSD-3）| 5 | 外部工具链，不做内核集成 |
| P2 | 神经材质 | Neural BRDF (2021)；NeuSample (2023) | Neural BRDF（MIT）| 4-5 | closure 级；需推理运行时 |

---

## 2. REALTIME（实时研究清单）

| 优先级 | 技术 | 论文/年份 | 参考代码 | 难度 | 备注 |
|---|---|---|---|---|---|
| P0 | OIDN v3 时域去噪 | OIDN v3.0 (2026) | RenderKit/oidn（Apache-2.0，已集成）| 1 | 升级库 + 接线；Cycles 已有 motion passes |
| P1 | SVGF / A-SVGF | HPG 2017 / HPG 2018 | 社区实现（KIT A-SVGF）| 2 | 最贴合 Cycles pass/film 架构；实时模式首个重建特性 |
| P1 | TAAU / FSR2 上采样 | FSR2 (2022) | GPUOpen-Effects/FidelityFX-FSR（MIT）| 2-3 | 视口低分辨率渲染+上采样 |
| P1 | BMFR | SIGGRAPH Asia 2020 | hchoi405/bmfr（MIT）/ gtong-nv（BSD-3）| 3 | 特征回归重建 |
| P2 | ReSTIR DI | SIGGRAPH 2020 | DQLin/ReSTIR_PT（BSD-3，重写）| 3-4 | 实时标志特性；依赖 lamp 修复 |
| P2 | ReSTIR GI | SIGGRAPH 2021 | 同上（RTXDI 仅参考）| 4 | 需要 shift mapping |
| P2 | Volumetric ReSTIR | SA 2021 | DQLin/VolumetricReSTIRRelease（BSD-3）| 4 | Cycles 有体积管线 |
| P2 | Radiance Cache（实时 GI）| HPG 2024 on-surface caches | 无官方代码 | 4 | 需 lamp |
| P3 | NRD / RTXGI / NN 去噪 | 2020+ | 专有许可 | 4-5 | 不可并入；仅参考 |

---

## 3. WAVE OPTICS（波动光学研究清单）

| 优先级 | 技术 | 论文/年份 | 参考 | 难度 | 备注 |
|---|---|---|---|---|---|
| P0 | 波长采样 / 光谱化 FSD | FSD BSDF (2024)；pbrt-v4 光谱 | pbrt-v4（Apache-2.0）| 3 | ✅ Phase1-1：3-band dispersion 模式已实现（wav_spectrum.h）；完整 CIE 随机波长采样待续 |
| P0 | Stokes 偏振 AOV + Mueller 层 | EG 2018；EGSR 2016 | mitsuba3 stokes.cpp（BSD-3）| 3 | wav_stokes.h 已备；需 4 通道输出 |
| P1 | UTD 边衍射进 integrator | UTD（教科书）；FSD BSDF (2024) | wave_tracer UTD 数学（CC-NC，重实现）；Sionna（Apache-2.0）交叉验证 | 4 | 需边几何提供者抽象（Cycles BVH 缺边邻接）|
| P1 | 薄膜多层 closure | Multilayer Iridescence (SA 2023) | 无代码；wav_fresnel.h 契合 | 2-3 | 依赖波长采样 |
| P2 | 干涉/相干传输（Wigner）| Wigner on Polygons (2022) | 无 | 3-5 | 跨路径场叠加 |
| P2 | 波束/相干传输管线 | Wave Tracing (2025, arXiv 2508.17386)；RT-PLT (SA 2024) | wave_tracer（CC-NC，重实现）| 5 | 长期目标：wave transport regime |
| P2 | 偏振 ray tracing 对照 | Mitsuba 3 | mitsuba3（BSD-3）| 3 | 许可干净对照 |

---

## 4. SHARED（共享基础设施清单）

| 技术 | 说明 | 参考代码 | 难度 |
|---|---|---|---|
| RenderMode/Feature 系统 | 第 3/5/6/7 节核心框架 | 自研（映射 Cycles 节点体系）| 2 |
| lamp 光源修复 | 独立构建 lamp 全黑（实测）| 调试 Cycles light 管线 | 2 |
| OIDN v3 时域去噪升级 | 实时/离线共用 | RenderKit/oidn（Apache-2.0）| 1 |
| OpenPGL 引导（已有）| 离线共用 | OpenPathGuidingLibrary（Apache-2.0）| - |
| Embree / 光树 / motion passes | 已有基础设施 | Cycles 自带 | - |
| 频谱渲染管线 | 波长采样的基础 | pbrt-v4（Apache-2.0）参考 | 3 |
| FSR2 上采样（MIT）| 实时视口可选 | FidelityFX-FSR | 2-3 |

---

*四份原始调研报告见 research/ 目录（每份含全量条目表与 URL 验证清单）。*
