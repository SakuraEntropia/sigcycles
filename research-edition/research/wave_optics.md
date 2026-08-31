# 波动光学渲染研究清单（2019–2026 SIGGRAPH / 顶会）——面向 Cycles waveoptics 轨道

> 调研范围：2019–2026 年 SIGGRAPH / SIGGRAPH Asia / Eurographics / EGSR / CGF / TOG 及其他顶会的物理光学、波动光学渲染工作，
> 并回溯收录 2016–2018 年的奠基性工作（作为背景）。所有仓库 URL、许可证与论文元数据均经 web_search / GitHub API /
> Semantic Scholar API / Crossref / 作者主页交叉核验；未核验项在条目内显式标注"⚠未核验"。
>
> 调研基线（本仓库现有移植）：`cycles/src/waveoptics`（FSD / UTD / Fresnel / Stokes-Mueller / Gaussian beam 数学，
> 自 `wave_tracer` 移植）+ `cycles/src/kernel/closure/bsdf_wave_diffraction.h`（Wave Diffraction BSDF closure，
> v1 = 矩形狭缝 Fraunhofer FSD，单色、f=pdf、CPU 侧查表）。详见 `cycles/src/waveoptics/README.md` 与
> `integration_notes_wavetracer_core.md`、`integration_notes_cycles_closure.md`。

---

## 0. 现有移植基线速览（影响所有"集成难度"评估）

| 模块 | 内容 | 源 | 状态 |
|---|---|---|---|
| `wav_complex.h` / `wav_math.h` | GPU 友好复数、常量与 2D 数学 | 新写 / wt math | ✅ |
| `wav_cerf.h` | 复误差函数（级数+连分式） | 替代 libcerf | ✅ |
| `wav_utd.h` | UTD 楔形衍射（a±、F 过渡函数、wedge UTD） | wt/interaction/fsd/utd.hpp | ✅（未接入 integrator） |
| `wav_fsd.h` + `wav_fsd_sampler.h` + `wav_fsd_tables.*` | Fraunhofer FSD ASF + 重要性采样 + 主机构建 LUT | wt/fraunhofer/fsd* | ✅（closure 已用） |
| `wav_fresnel.h` | 复折射率 Fresnel | wt/interaction/fresnel.hpp | ✅ |
| `wav_stokes.h` | Stokes 向量 + Mueller 算子 | wt/polarimetric/ | ✅（未接入 integrator） |
| `wav_gaussian.h` | Gaussian beam 截面振幅 | wt/beam/gaussian_wavefront.hpp | ✅（未接入） |
| `bsdf_wave_diffraction.h` | Wave Diffraction BSDF closure（SVM/OSL 节点） | 自研 | ✅（CPU；GPU 求值为 0） |

**许可红线（必须先讲）**：`wave_tracer` 的 LICENSE 为 **CC BY-NC 4.0**（非商业），与 Cycles 的 Apache-2.0
在重分发/商业化上不兼容。现有 `cycles/src/waveoptics` 是直接移植，README 已声明"商用需另行向作者取得许可，
或按论文（SIGGRAPH 2024 FSD BSDF）重实现"。本清单中凡涉及 wave_tracer 派生的建议，默认按"重实现公式、不复制代码"处理。

---

## 1. 波动光学路径追踪 / 波场渲染（Steinberg 线，即 WaveTracer 项目）

代码载体统一为 **ssteinberg/wave_tracer**：https://github.com/ssteinberg/wave_tracer （本地已克隆）
- 语言/框架：C++20 / CMake / CPU-only；子模块依赖 glm、tinybvh、tinyobjloader、libcerf（UTD F 过渡函数用，Cycles 端已由 `wav_cerf.h` 替换）等。
- 许可证：**CC BY-NC 4.0**（GitHub API 显示 Other/NOASSERTION 是因 GitHub 不识别 CC-BY-NC；仓库内 LICENSE 为 CC BY-NC 4.0）。
- 定位：波动光学 path tracer，路径跟踪"椭圆锥"（elliptical cones）作为波的几何代理；支持可见光与长波长（10GHz 等）双用途；
  性能约为经典 ray tracing 的 5–20x，但随场景复杂度增长保持稳定；**early alpha**。

| # | 论文 | 年份 / 会议 | DOI | 作者 | 与 Cycles 集成难度 | 兼容性备注 |
|---|---|---|---|---|---|---|
| 1.1 | A Generic Framework for Physical Light Transport（物理光传输通用框架） | SIGGRAPH 2021, ACM TOG 40(4) | 10.1145/3450626.3459791 | Steinberg, Yan | 数学 1 / 管线 5 | 本线奠基：部分相干传输、椭圆锥束、偏振/相干理论。数学层（stokes/mueller/fresnel/beam）已移植；"锥束跟踪"整管线在 Cycles 属全新范式 |
| 1.2 | Towards Practical Physical-Optics Rendering（即用户所提"WaveTracer 2022"） | SIGGRAPH 2022, ACM TOG 41(4) | 10.1145/3528223.3530119 | Steinberg, Sen, Yan | 数学 1 / 管线 5 | Gaussian beam 分解与采样、实用化与性能（~5–20x）；wave_tracer 主实现对应此线 |
| 1.3 | Physical Light-Matter Interaction in Hermite-Gauss Space | ACM TOG 40(6), 2021.12 | 10.1145/3478513.3480530 | Steinberg, Yan | 2 | HG 相干模式展开的光-物质相互作用；与 `wav_gaussian.h` 同源，可扩展为高阶光束模式 |
| 1.4 | Rendering of Subjective Speckle Formed by Rough Statistical Surfaces（主观散斑） | ACM TOG 41(1), 2022.02 | 10.1145/3472293 | Steinberg, Yan | 4 | 需要跨路径场的相干叠加（非 closure 级）；需新传输管线 |
| 1.5 | Accurate Rendering of Liquid-Crystals and Inhomogeneous Optically Anisotropic Media | ACM TOG 39(3), 2020.06 | 10.1145/3381748 | Steinberg | 3 | Jones 传播的各向异性介质；与偏振/Stokes 数学天然衔接，可作"各向异性透明"材质层 |
| 1.6 | **A Free-Space Diffraction BSDF**（FSD BSDF） | **SIGGRAPH 2024**, ACM TOG 43(4) art.113 | 10.1145/3658166 | Steinberg, Ramamoorthi, Bitterli, Mollazainali, D'Eon, Pharr | **已移植（2）** | 基于边（edge-based）的 Fraunhofer FSD，只需 ray tracing、无需几何预处理；**本模块 `wav_fsd*/`、closure 的直接论文来源**。NVIDIA RTR 页面：research.nvidia.com/labs/rtr/publication/steinberg2024diffraction/ |
| 1.7 | A Generalized Ray Formulation for Wave-Optical Light Transport（广义射线形式） | SIGGRAPH Asia 2024, ACM TOG 43(6)；arXiv:2303.15762 | 10.1145/3687902 | Steinberg, Ramamoorthi, Bitterli, D'Eon, Yan, Pharr | 3–5 | Wigner phase-space / 部分相干传输、Gaussian beam 分解、NEE/MIS；给出"如何把波动光学塞进经典 path tracer"的框架，是 Cycles 侧后续管线改造的理论依据 |
| 1.8 | **Wave Tracing: Generalizing The Path Integral To Wave Optics**（用户所提 2025） | arXiv:2508.17386, 2025.08（physics.optics） | — | Steinberg, Pharr | 4 | 把路径积分推广到波动光学的最新理论框架（beam/场级传输），尚未见工程落地 |
| 1.9 | High-Performance Elliptical Cone Tracing | CGF 44(7), 2025.10 | 10.1111/cgf.70230 | Emre, Kanak, Steinberg | 4 | ADS（加速数据结构）加速椭圆锥遍历；性能工程方向，Cycles 无对应结构 |

---

## 2. 衍射渲染（UTD/GTD、Fraunhofer/Fresnel、edge diffraction）

| 论文 / 项目 | 年份 / 会议 | DOI / 链接 | 代码 / 许可 | 难度 | 兼容性备注 |
|---|---|---|---|---|---|
| **A Free-Space Diffraction BSDF**（见 §1.6） | SIGGRAPH 2024 | 10.1145/3658166 | wave_tracer（CC BY-NC 4.0） | 2（Fraunhofer 已移植）/ 4（UTD 进 integrator） | wave_tracer 内 UTD 与 Fraunhofer 两套 FSD BSDF 均存在（docs/source/scenes/bsdfs/fsd/）；Cycles 侧 `wav_utd.h` 已备好但仅作独立模块，v1 closure 用 Fraunhofer |
| A Two-Scale Microfacet Reflectance Model Combining Reflection and Diffraction | SIGGRAPH 2017, TOG 36(4)（背景） | 10.1145/3057960 ⚠DOI 未核验 | 无官方公开代码（HAL: hal-01545440 方法描述） | 3 | Holzschuch & Pacanowski：双尺度微表面 + 远场衍射级数；是"表面衍射"类 BSDF 的最实用参考之一，与 FSD（空间衍射）互补 |
| Rendering Specular Microgeometry with Wave Optics | SIGGRAPH 2018, TOG 37(4)（背景） | 10.1145/3197517.3201351 | 无官方公开代码 | 4 | Yan, Hašan, Walter, Marschner, Ramamoorthi：波动光学渲染高光微几何（glints 的波动版）；需表面微几何统计，非 closure 级 |
| Rendering Diffraction Phenomena on Rough Surfaces in Virtual Reality | ACM VRST 2025 | 10.1145/3641825.3689516 | 未公开 | 2 | 粗糙面衍射的轻量应用向；参考价值一般 |
| **Sionna RT**（RF/6G 波传播，UTD 边衍射） | NVlabs，持续更新 | https://github.com/NVlabs/Sionna | **Apache-2.0**，Python/TensorFlow | 1（仅参考数学） | UTD 边衍射（教程：nvlabs.github.io/sionna/v1.2.2/rt/tutorials/Diffraction.html）；与 `wav_utd.h` 数学同源（教科书公式），可交叉验证；领域是 RF 而非光学渲染 |
| 经典背景：Moravec 1981 "3D Graphics and the Wave Theory"；Stam 1999 "Diffraction Shaders" | SIGGRAPH 1981 / 1999 | — | — | — | 波动光学渲染的起源（光栅/散焦衍射着色器）；无现代代码 |

**备注**：edge diffraction（UTD/GTD 类）在光学渲染领域公开工程很少——主要就是 Steinberg 的 UTD FSD 与 RF 领域（Sionna、声学 UTD）。
Cycles 若要支持"边衍射采样"进 integrator，最大缺口是 **几何边的邻接/边 ID 结构**（Cycles BVH 无此信息，
wave_tracer 依赖自己的 ads；integration notes 已论证 `wedge_edge_t::UTD` 本身只需边几何字段，可解耦）。

---

## 3. 干涉与相干光传输（coherent light transport / Wigner）

| 论文 | 年份 / 会议 | 代码 / 许可 | 难度 | 备注 |
|---|---|---|---|---|
| **Autocorrelation, Wigner and Ambiguity Transforms on Polygons for Coherent Radiation Rendering** | arXiv:2202.02676（2022，未见正式顶会发表） | 无公开代码 | 3 | Mackay, Johnson, Brooker（Sydney）：多边形孔径上的自相关/Wigner/模糊度变换，直接渲染相干辐射干涉图样（菲涅耳区/相干成像）；数学与 FSD 的孔径-ASF 表示同族 |
| A Generalized Ray Formulation …（§1.7） | SIGGRAPH Asia 2024 | wave_tracer（CC BY-NC 4.0） | 3–5 | 用 Wigner phase-space 描述部分相干传输——目前把"相干/干涉"纳入 path tracer 的最完整框架 |
| Physical Light-Matter Interaction in Hermite-Gauss Space（§1.3） | TOG 2021 | 同上 | 2 | 相干模式（coherent modes）分解 |
| Rendering of Subjective Speckle（§1.4） | TOG 2022 | 同上 | 4 | 散斑 = 相干场涨落；需新管线 |
| （相邻，成像非渲染）Phasor fields（NLOS 成像） | COSI 2020 等 | 无 | — | 相干场重建用于非视域成像，与渲染弱相关，仅作背景 |

---

## 4. 偏振渲染（polarization / Stokes-Mueller）

| 论文 / 项目 | 年份 / 会议 | 代码 / 许可 | 难度 | 备注 |
|---|---|---|---|---|
| **Mitsuba 3 polarized rendering**（用户所提 "mitsuba-renderer 的 polarization"） | mitsuba3 主线（非分支），文档 v3.6.x | https://github.com/mitsuba-renderer/mitsuba3（**BSD-3-Clause**），C++17/20 + Dr.Jit；`src/integrators/stokes.cpp` 在主线 | 3 | 主线功能：Stokes 向量输出 integrator + Mueller 矩阵 BSDF（conductor/dielectric/polarizer/retarder 等）；文档：mitsuba.readthedocs.io → "Polarized rendering"。**关键参考实现**：标架旋转约定（receiver 视角）、TransportMode（双向时 Mueller 乘法顺序）。⚠性能：issue #1612 报告 stokes integrator 有性能负面影响 |
| **Bi-Directional Polarised Light Transport**（mitsuba 引为 [MSWK16]） | EGSR 2016, CGF 35(4) | 无公开代码 | 3 | Mojzík, Skřivan, Wilkie, Křivánek：偏振的 BDPT（Mueller-Stokes）；无公开代码 |
| **Bidirectional Rendering of Vector Light Transport**（[JA18]） | Eurographics 2018, CGF 37(2) | 无公开代码 | 3 | Arellano, Gutierrez, Jarabo：向量（偏振）光传输的双向方法；DOI 10.1111/cgf.13314 |
| Wilkie & Weidlich 2012（[WW12]，mitsuba3 引用键 WilkieWeidlich2012） | 2012 | 无 | 2 | 首个单向 polarized light transport 算法；⚠论文全名未能在线核验（以 mitsuba3 文献表为准） |
| A Standardised Polarisation Visualisation for Images（[WW10]） | SCCG 2010 | 无 | 2 | Wilkie & Weidlich：偏振可视化标准（作者主页确认） |
| Ray Tracing with Polarization Parameters（[WK90]，经典） | IEEE CG&A 1990 | 无 | 1 | Wolff & Kurlander：偏振光线跟踪源头；mitsuba3 文档亦引用其 Fresnel 约定 |
| Modeling and Verifying the Polarizing Reflectance of Real-World Metallic Surfaces | IEEE CG&A 2018 | 无 | 3 | Berger, Weidlich, Wilkie, Magnor：真实金属表面偏振反射的测量驱动建模（Mueller 测量验证） |
| ART（Advanced Rendering Toolkit） | 布拉格查尔斯大学研究渲染器 | 不公开 | — | 偏振渲染研究系统（mitsuba3 文档与之做了交叉验证） |

**与 Cycles waveoptics 的关系**：`wav_stokes.h`（Stokes+Mueller）与 `wav_fresnel.h`（复折射率）已是纯头文件且已移植；
mitsuba3 的文档/代码（BSD-3）是**许可干净的对照实现**，用于核对标架旋转、Mueller 复合顺序（`integration_notes_wavetracer_core.md` §3 已处理标架约定）。
真正的工作量在 integrator 层：携带 Stokes 状态、4 通道输出、偏振 AOV（S0–S3）——难度约 3。

---

## 5. 薄膜光学 thin-film / 干涉涂层

| 论文 | 年份 / 会议 | DOI | 代码 / 许可 | 难度 | 备注 |
|---|---|---|---|---|---|
| **Efficient and Accurate Physically Based Rendering of Periodic Multilayer Structures with Iridescence** | SIGGRAPH Asia 2023（Posters/TC 类 ⚠类型以 ACM 为准） | 10.1145/3610542.3626137 | 无公开代码 | 2–3 | Kaminaka, Higaki, Raytchev, Kaneda：周期多层膜（1D 周期结构）干涉色的快速精确渲染；多层膜 = 相干叠层（特征矩阵/Abeles 递推） |
| Real-Time Rendering of Oil Film with Flexible Properties | IIEEJ 2021（日语期刊，次要） | 10.2199/… ⚠ | 无 | 2 | 油膜薄膜干涉实时渲染（现象学） |

**备注**：薄膜/多层干涉是"材料级"相干效应，与空间衍射（FSD）正交；数学上与 `wav_fresnel.h`（复折射率 Fresnel）+ 波长参数天然契合，
作为 closure 材质层实现难度低（2–3）。前提同样是 **波长采样/光谱管线**（见 §7）——干涉色对 λ 极度敏感，单色假设下无意义。

---

## 6. 傅里叶光学 / 波传播渲染（Fourier optics / Gaussian beam）

| 论文 / 项目 | 年份 / 会议 | DOI | 代码 / 许可 | 难度 | 备注 |
|---|---|---|---|---|---|
| **A Phenomenological Approach to Integrating Gaussian Beam Properties and Speckle into a Physically-Based Renderer** | VMV 2016（背景） | 10.2312/vmv.20161357 | 无公开代码 | 3 | Bergmann, Mohammadikaji, Irgenfried, Wörn, Beyerer, Dachsbacher（KIT）：现象学 Gaussian beam + speckle（机器视觉测量渲染场景）；现象学 → 与物理 FSD 管线互补 |
| Steinberg wave tracing / elliptical cones（§1.7, §1.9） | 2024–2025 | 10.1145/3687902；10.1111/cgf.70230 | wave_tracer（CC BY-NC 4.0） | 4–5 | Gaussian beam ≈ 波的传输代理；椭圆锥遍历 = 波传播的工程实现 |
| （相邻）Light Field Rendering using Matrix Optics | 2007（背景） | ⚠ | 无 | — | ABCD/matrix optics（近轴 Fourier 光学） |
| （相邻，数值）A Wavefront-based Gaussian Beam Method | 数值分析文献 | Zbl 1443.65302 | 无 | — | 高频波传播的 wavefront/Gaussian beam 数值方法（非 CG） |

**备注**：FSD 的 paraxial（近轴）假设与 Fourier/矩阵光学一致（README 已注明 tan-space Jacobian 未应用 = 近轴近似）；
Gaussian beam 数学已以 `wav_gaussian.h` 形式在模块内，但"波束传输"在 Cycles 需要新的 integrator 范式（难度 5）。

---

## 7. 光谱渲染 spectral rendering 与波动光学的关系

| 论文 / 项目 | 年份 / 会议 | DOI / 链接 | 代码 / 许可 | 备注 |
|---|---|---|---|---|
| **Spectral Rendering: from Input to Rendering Process** | IS&T Electronic Imaging 2024, 36(10) | 10.2352/EI.2024.36.10.IPAS-251（开放获取） | 无代码 | Kim & Gotchev（Tampere）：从光谱输入到渲染流程的综述/教程，光谱渲染管线全景 |
| **pbrt-v4**（书 + 代码） | 2023 | https://github.com/mmp/pbrt-v4 | **Apache-2.0** | 光谱渲染现代基线（hero wavelength 等）；无波动光学/偏振——可作为 Cycles 光谱采样的对照 |
| 经典：Wilkie et al. 光谱渲染（背景） | 2000s–2010s | — | — | Physically based spectral rendering 系列 |

**关系要点**：波动光学效应（衍射/干涉/薄膜色）全部是 **波长强依赖** 的（相位 ∝ k·L，k=2π/λ）。
Cycles 当前 RGB 管线下，`bsdf_wave_diffraction.h` 的 closure 是**单色**的（wavelength 为 scalar 参数，nm），
README 已声明"不参与 Cycles 光谱管线"。要做真实的衍射/干涉色，必须引入**波长采样（每像素多波长）**——
这是把 FSD 从"单色演示"变成"实景效果"的最大管线改造点（难度 3）。

---

## 8. 2025–2026 最新动态（补充检索）

| 论文 | 年份 / 会议 | DOI | 代码 | 备注 |
|---|---|---|---|---|
| **Realistic Cloth Rendering with a Ray-Wave Hybrid Shading Model** | SIGGRAPH Asia 2025（NVIDIA RTR，2025.12） | ⚠ | 未公开 | Yu, Walter, Marschner, Weidlich：ray-wave 混合织物着色——Gaussian beam 全波仿真校准 + 孔径衍射处理纱线间透射；"混合 ray-wave"是 2025 年新趋势 |
| **Designing and Fabricating Color BRDFs with Differentiable Wave Optics** | SIGGRAPH Asia 2025, ACM TOG | 10.1145/3763275 | 未公开 | Zeng, Choi, Amata, Kang, Heidrich, Wu, Kim（KAIST/浙大）：可微波动光学设计制造彩色 BRDF（超表面）；波动光学进入"可微/制造"方向 |

---

## 9. 汇总评估表（按主题）

| 主题 | 代表工作 | 公开代码 / 许可 | 集成难度（1–5） | 关键结论 |
|---|---|---|---|---|
| 波动光学路径追踪 / 波场 | WaveTracer 线（2020–2025，§1） | wave_tracer：CC BY-NC 4.0 ⚠ | 数学 1；波束管线 5 | 数学层已全移植；"锥束/波束跟踪"需要新 integrator 范式 |
| 衍射渲染（Fraunhofer/UTD） | FSD BSDF 2024（§1.6, §2） | wave_tracer：CC BY-NC 4.0 | Fraunhofer closure 2（已做）；UTD 进 integrator 4 | UTD 需要"边几何/邻接"结构（Cycles BVH 缺）；Sionna（Apache-2.0）的 UTD 可交叉验证 |
| 干涉 / 相干传输 | Wigner/Autocorrelation 2022（§3）；rtplt 2024 | 无 / wave_tracer | 3–5 | 相干效应需跨路径场叠加，非 closure 级 |
| 偏振渲染 | Mitsuba 3（主线）、EGSR 2016、EG 2018（§4） | mitsuba3：BSD-3-Clause ✅ | 数学 1；integrator 3 | `wav_stokes.h` 已备；mitsuba3 是许可干净的对照参考；需 Stokes 状态 + 4 通道输出 |
| 薄膜光学 / 干涉涂层 | Multilayer Iridescence 2023（§5） | 无 | 2–3 | 材料级相干叠层；与 `wav_fresnel.h` 契合；依赖波长采样 |
| 傅里叶 / 波传播 | Gaussian beam VMV 2016；wave tracing（§6） | 无 / wave_tracer | 3–5 | `wav_gaussian.h` 已备；传输管线是难点 |
| 光谱渲染 | EI 2024 综述；pbrt-v4（§7） | pbrt-v4：Apache-2.0 ✅ | 3（波长采样） | 波动光学天然光谱；RGB 管线是当前最大约束 |

**许可速查**：wave_tracer = CC BY-NC 4.0（**不兼容 Cycles Apache-2.0 重分发/商用**）；mitsuba3 = BSD-3-Clause；
Sionna = Apache-2.0；pbrt-v4 = Apache-2.0；libcerf（wave_tracer 依赖，UTD F 函数）许可 ⚠未逐一核验（Cycles 端已用 `wav_cerf.h` 替代）。

---

## 10. 结论与建议（对 Cycles waveoptics 研究轨道）

1. **数学层已到位**：FSD/UTD/Fresnel/Stokes-Mueller/Gaussian beam 均已在 `cycles/src/waveoptics`；后续新增数学（多层膜矩阵、
   Wigner/自相关、HG 模式）都可继续以纯头文件方式落地（难度 1–2）。
2. **许可合规优先**：wave_tracer 派生代码不得进入 Apache-2.0 上游；FSD BSDF（SIGGRAPH 2024）与 UTD（教科书）公式公开，
   按论文重实现即可合规——`integration_notes_wavetracer_core.md` §6 已有此预案。
3. **建议增量顺序**（按"与现有模块契合度 × 收益"）：
   - ① 波长采样 / 光谱化 FSD closure（难度 3）——解锁干涉色与衍射色散；
   - ② Stokes 偏振 AOV + Mueller 材质层（难度 3，参考 mitsuba3 BSD 实现）；
   - ③ UTD 边衍射进 integrator（难度 4，需边几何提供者抽象；wave_tracer 的 UTD 数学可解耦 ads，见 integration notes）；
   - ④ 薄膜多层 closure（难度 2–3，依赖 ①）；
   - ⑤ 波束/相干传输管线（难度 5，长期研究目标，依据 arXiv:2508.17386 与 TOG 2024 广义射线框架）。
4. **值得跟踪的新方向**：SIGGRAPH Asia 2025 的 ray-wave 混合织物着色与可微波动光学 BRDF 制造——前者与 FSD 的"孔径衍射"机制
   直接相关，后者预示波动光学渲染向设计/制造闭环演进。

---

## 附录：核验来源（URL 清单）

- wave_tracer：https://github.com/ssteinberg/wave_tracer （README/BUILD/LICENSE；GitHub API：181★、C++、license=NOASSERTION；本地 LICENSE 文件为 CC BY-NC 4.0）
- wave_tracer 自带文献表：`wave_tracer/docs/source/bibtex.bib`（本报告 §1 的 TOG/arXiv/CGF 条目全部由此核对）
- FSD BSDF：https://research.nvidia.com/labs/rtr/publication/steinberg2024diffraction/ （abstract）
- Wave Tracing：https://arxiv.org/abs/2508.17386 （Steinberg & Pharr）；广义射线 arXiv 版：https://arxiv.org/abs/2303.15762
- Mitsuba 3：https://github.com/mitsuba-renderer/mitsuba3 （LICENSE=BSD-3；`src/integrators/stokes.cpp`；docs "Polarized rendering" v3.6.x；
  issue #1612 性能提示）；偏振引用键（WilkieWeidlich2012 / Mojzik2016BidirectionalPol / Jarabo2018BidirectionalPol / WolffKurlander1990）取自
  mitsuba3 master 的 `docs/src/key_topics/polarization.rst`
- 偏振谱系作者/会议：A. Wilkie 主页 https://cgg.mff.cuni.cz/~wilkie/Website/Home.html （Mojzík-Skřivan-Wilkie-Křivánek EGSR 2016；
  Berger 等 IEEE CG&A；SCCG 2010 可视化）；Wiley 10.1111/cgf.13314（EG 2018）
- Sionna：https://github.com/NVlabs/Sionna （LICENSE 头 SPDX Apache-2.0；RT Diffraction 教程 v1.2.2）
- 论文元数据：Semantic Scholar Graph API（FSD/广义射线/微几何/多层膜/VMV/液晶/散斑/HG/cone-tracing 等 DOI）
- 其余：ACM DL（10.1145/3687902、10.1145/3763275 等）、NVIDIA RTR（yu2025realistic）、KAIST VCLab（siggraphasia2025p1）、
  IS&T library（IPAS-251，作者 Kim & Gotchev 经 tiedejatutkimus.fi 核对）、HAL（hal-01545440，two-scale）
- ⚠未核验项：WW12 论文全名；Kaminaka 论文的精确栏目类型（Posters vs Technical Communications）；libcerf 许可证；
  VRST 2025 衍射论文作者名单；"A Two-Scale" 论文 DOI（仅确认 HAL 与 Semantic Scholar 条目）
