# 现代高光传输（Light Transport）研究集成进 Blender Cycles 的前期调研报告
### 调研范围：2019–2026 SIGGRAPH / SIGGRAPH Asia（离线渲染方向），附代码、许可证与集成难度评估

- **调研日期**：2026-02（基于 web_search / 仓库直连验证）
- **目标仓库**：`/Users/faputa/Documents/Entro-Cycles/cycles`（Blender Cycles 独立仓库，**Apache-2.0**）
- **说明**：所有代码仓库 URL 均已通过 GitHub API / raw 文件 / `git ls-remote` 实测验证（标注 ✅=已验证存在；⚠️=无许可证文件或未完全验证）。许可证结论以仓库内 LICENSE 文件为准。

---

## 0. 全局结论（先读这里）

1. **Cycles 已经有"路径引导"**：Blender 3.4（2022-12）起 Cycles 已用 **Intel OpenPGL** 集成 path guiding（实验性，详见 wiki 3.4 发布说明），OpenPGL 为 **Apache-2.0**，与 Cycles 许可证完全兼容。后续改进（体素引导、guided MIS、神经引导）都可在这个既有设施上做增量。
2. **许可证是最大的门槛**：与 Apache-2.0 兼容的常用许可证为 **BSD-3/BSD-2/MIT/Apache-2.0**。本调研发现两个典型的"不可直接并入"案例：Tom94 的 *Practical Path Guiding* 代码是 **GPL-3.0**（Cycles 不能并入 GPL 代码）；NVIDIA *Conditional ReSTIR* 原型是 **Nvidia Source Code License-NC（非商业）**；RTXDI SDK 是 NVIDIA 专有 SDK 许可。这类只能"参考思路、重写实现"。
3. **ReSTIR 家族官方代码多在 Falcor 渲染框架内**（BSD-3-Clause），是 C++/HLSL 的实时渲染栈，与 Cycles 的"离线、逐像素多采样、CPU+GPU 双后端"架构差异大，直接移植成本高；但其 **GRIS/RIS 理论**（SIGGRAPH 2022）可作为 Cycles 的 many-light / hard-to-sample 间接光改进的理论基础。
4. **Cycles integrator 结构**（供后续集成对照）：
   - 内核路径追踪主循环：`src/kernel/integrator/path_trace.h`、`shade_surface.h`、`shade_volume.h`；采样函数在 `src/kernel/sample/`，光采样/light tree 在 `src/kernel/light/`；MIS 主要用 balance heuristic + light tree + MNEE（`mnee.h`）。
   - CPU 侧 integrator 状态机：`src/integrator/`（`path_trace.cpp`、`adaptive_sampling.cpp`、`render_scheduler`）。
   - 设备后端：CUDA / OptiX / HIP / Metal（`src/kernel/device/`）+ CPU（Embree）。
   - 既有 OpenPGL 引导封装在 `src/guiding/` 与 kernel 的 path_guiding 相关文件。

5. **难度速览表**（1=最易，5=最难；离线路径追踪视角）：

| 主题 | 代表工作 | 难度 | 一句话理由 |
|---|---|---|---|
| Path guiding | OpenPGL（2019-2025 演进） | 1–2 | Cycles 已集成 OpenPGL（3.4+），增量改进即可 |
| Adaptive sampling | DAAS（2023）/ Error Estimation（2024） | 2 | 纯 CPU 侧像素调度逻辑（`src/integrator/`），无内核结构性改动 |
| 高级 MIS | Efficiency-Aware MIS（2022） | 2–3 | 改 MIS 权重/采样策略，kernel 内定点修改 |
| MLT/PSSMLT | Langevin MCMC（2020）等 | 3–4 | 需新增 integrator 类型与 MCMC 状态；OptiX 后端受限 |
| Gradient-domain | G-PT（2015）/ TG-PT（2016） | 4–5 | 需要梯度路径采样 + 屏幕空间 Poisson 重建（A-buffer/邻域内存） |
| VCM | SmallVCM（2012） | 4–5 | 需要光子图/合并半径结构，与现内核差异大 |
| ReSTIR DI/GI/PT | ReSTIR 系列（2020-2026） | 4–5 | 实时架构（帧间复用、persistent buffer）与离线多采样渲染冲突 |
| 神经采样 | NIS（2022）/ NPIS（2024）/ NIS-ML（2025） | 4–5 | 多数无官方代码 + 需 ML 推理管线（ONNX/PyTorch→kernel） |

---

## 1. ReSTIR 系列（Resampled Importance Sampling）

> 离线渲染语境下的价值点：GRIS 理论（SIGGRAPH 2022）为"任意策略间的无偏复用"提供了完整理论；ReSTIR PT 的 shift mapping 思想可迁移到离线 many-light / 焦散场景。但所有官方实现均为**实时渲染框架（Falcor/RTXDI，C++/HLSL）**。

### 1.1 ReSTIR DI — *Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting*
- **论文**：Benedikt Bitterli, Chris Wyman, Matt Pharr, Peter Shirley, Aaron Lefohn, Wojciech Jarosz — **SIGGRAPH 2020**（ACM TOG 39(4)）
- **官方代码**：官方未公开独立实现（NVIDIA 论文页 "Code: AccessDenied"）；生产化实现为 **RTXDI SDK**（见下）；社区复现：
  - ✅ https://github.com/karel-tomanec/Falcor-ReSTIR （ReSTIR DI in Falcor，BSD-3-Clause）
  - ✅ https://github.com/lindayukeyi/ReSTIR_DX12 （DX12/DXR，⚠️无 LICENSE 文件）；同源 fork https://github.com/Songsong97/ReSTIR_DX12
- **语言/框架**：C++/HLSL，Falcor 或 DirectX 12 + DXR
- **许可证**：官方无代码；RTXDI 为 **NVIDIA RTX SDKs License（专有 SDK 许可，非 OSI）**；社区复现多为 BSD-3 或未声明
- **集成难度：5**。ReSTIR DI 的核心（reservoir + 时空复用）为实时设计：需要逐像素 reservoir 持久缓冲与**帧间**时域复用，与 Cycles"同一帧内逐像素多采样、任意帧数迭代"的离线模型冲突；且 Cycles 已有 light tree + MIS 覆盖直接光 many-light，收益有限。

### 1.2 ReSTIR GI — *ReSTIR GI: Path Resampling for Real-Time Path Tracing*
- **论文**：Yue Ouyang, Shiqiu Liu, Markus Kettunen, Matt Pharr, Jacopo Pantaleoni — **SIGGRAPH 2021**（ACM TOG 40(4)）
- **代码**：官方代码并入 Falcor/RTXDI（RTXDI 2.0 起含 ReSTIR GI，见 [RTXDI README](https://raw.githubusercontent.com/NVIDIA-RTX/RTXDI/main/README.md)）；无独立官方仓库。社区参考：DoeringChristian/restirgi（非官方）
- **语言/框架**：C++/HLSL，Falcor / RTXDI
- **许可证**：RTXDI 专有 SDK 许可
- **集成难度：5**（同 ReSTIR DI；间接漫反射复用对离线的增益不如 path guiding 直接）

### 1.3 Volumetric ReSTIR — *Fast Volume Rendering with Spatiotemporal Reservoir Resampling*
- **论文**：Daqi Lin, Chris Wyman, Cem Yuksel — **SIGGRAPH Asia 2021**（ACM TOG 40(6)）
- **代码**：✅ https://github.com/DQLin/VolumetricReSTIRRelease （**BSD-3-Clause**，Falcor）
- **语言/框架**：C++/HLSL（Falcor）
- **集成难度：4–5**。体渲染 + ReSTIR 对离线体渲染（Cycles 的 volume 步进）有参考价值，但仍是实时架构。

### 1.4 GRIS（SIGGRAPH 2022）+ ReSTIR PT（SIGGRAPH 2023）— 最值得借鉴的理论
- **GRIS 论文**：*Generalized Resampled Importance Sampling: Foundations of ReSTIR* — Daqi Lin*, Markus Kettunen*, Benedikt Bitterli, Jacopo Pantaleoni, Cem Yuksel, Chris Wyman — **SIGGRAPH 2022**（ACM TOG 41(4)，* 并列一作）
- **ReSTIR PT 论文**：*ReSTIR Path Tracing: Resampling with Spatiotemporal Reservoirs* — Daqi Lin, Markus Kettunen, Benedikt Bitterli, Jacopo Pantaleoni, Cem Yuksel, Chris Wyman — **SIGGRAPH 2023**（ACM TOG 42(4)）
- **官方代码（两者合一）**：✅ https://github.com/DQLin/ReSTIR_PT （**BSD-3-Clause**；Falcor 4.4 render pass "ReSTIRPTPass"，含 RunReSTIRPTDemo 场景）
- **GRIS 独立复现**：✅ https://github.com/Pxy951/ReSTIR_PT （**BSD-3-Clause**）
- **语言/框架**：C++/HLSL（Falcor 4.4）
- **集成难度：4**（理论可直接读论文迁移；工程上需要 reservoir 状态与像素缓冲）。**兼容性备注**：GRIS 的"任意源分布 → 目标分布无偏重采样 + MIS 权重"理论是离线 many-light（数万光源场景）和焦散场景的可行改进方向；Cycles 的 many-light 目前靠 light tree + balance heuristic MIS，可参考 GRIS 推导"combined/target distribution 权重"。

### 1.5 Conditional ReSTIR / CRIS — *Conditional Resampled Importance Sampling and ReSTIR*
- **论文**：Markus Kettunen*, Daqi Lin*, Ravi Ramamoorthi, Thomas Bashford-Rogers, Chris Wyman — **SIGGRAPH Asia 2023**
- **代码**：✅ https://github.com/NVlabs/conditional-restir-prototype （**Nvidia Source Code License-NC（非商业）**）
- **语言/框架**：C++/HLSL（Falcor 原型）
- **集成难度：5**（许可证不可用 + 实时架构）。CRIS 的"条件路径空间复用"理论可作 ReSTIR PT 的后续发展阅读。

### 1.6 Area ReSTIR — *Area ReSTIR: Resampling for Real-Time Defocus and Antialiasing*
- **论文**：Song Zhang, Daqi Lin, Markus Kettunen, Cem Yuksel, Chris Wyman — **SIGGRAPH 2024**
- **代码**：官方项目页 https://graphics.cs.utah.edu/research/projects/area-restir/ （含 "Source Code on GitHub"）；直接光部分 ✅ https://github.com/guiqi134/Area-ReSTIR （**BSD-3-Clause**）
- **语言/框架**：C++/HLSL（Falcor）
- **集成难度：5**（4D ray-space reservoir；实时）。

### 1.7 2024 之后的 ReSTIR 进展
- **Reservoir Splatting** — *Reservoir Splatting for Temporal Path Resampling and Motion Blur* — Jeffrey Liu, Daqi Lin, Markus Kettunen, Chris Wyman, Ravi Ramamoorthi — **SIGGRAPH 2025**；✅ 代码 https://github.com/Jebbly/Reservoir-Splatting （**BSD-3-Clause**，Falcor 8.0 独立 ReSTIR PT）。核心：把上一帧 primary hit **forward-project（splat）**到当前帧，保留精确命中，提升时域复用鲁棒性（对离线动画序列的去噪/复用也有参考价值）。
- **ReSTIR FG / Guided ReSTIR FG+**（EGSR 2024 / 2025，TU Clausthal）：光子 final gathering + reservoir，实时焦散；✅ https://github.com/TU-Clausthal-Rendering/ReSTIR-FG 、✅ https://github.com/TU-Clausthal-Rendering/Guided-ReSTIR-FG-Plus （均 **BSD-3-Clause**，Falcor）。
- **Manifold Hybrid Shift**（硕士学位论文项目，2024-2025）：改进 ReSTIR PT 的 shift mapping（specular 顶点重连接 + Jacobian）；✅ https://github.com/takkasila/Manifold-Hybrid-Shift （**BSD-3-Clause**，Falcor 4.4 ReSTIRPTPass）。
- **ReSTIR PG** — *ReSTIR PG: Path Guiding with Spatiotemporally Resampled Paths* — Zheng Zeng, Markus Kettunen, Chris Wyman — **SIGGRAPH Asia 2025**（https://dl.acm.org/doi/full/10.1145/3757377.3763813 ，NVIDIA RTR 项目页 https://research.nvidia.com/labs/rtr/publication/zeng2025restirpg/ ）。**ReSTIR × path guiding 的结合点**，对离线（Cycles 已有 guiding）是重要参考；未发现公开代码。
- **ReSTCV** — *Spatio-Temporal Control Variates with ReSTIR for Real-Time Rendering* — **SIGGRAPH 2026**；✅ 代码 https://github.com/Hercier/ReSTCV （**BSD-3-Clause**，Falcor 系）。控制变量 + reservoir，是 2026 年最新进展。
- **课程资料**：*A Gentle Introduction to ReSTIR: Path Reuse in Real-time*（SIGGRAPH 2022 课程，2023 更新至 ReSTIR PT）— https://intro-to-restir.cwyman.org （含课程笔记 PDF；也收录于 SIGGRAPH 2023 Courses，DOI 10.1145/3587423.3595511）。**强烈建议作为 ReSTIR 系列入门**。
- 相关扩展：*Amortizing Samples in Physics-Based Inverse Rendering Using ReSTIR*（Yu-Chen Wang, Chris Wyman, Lifan Wu, Shuang Zhao — SIGGRAPH 2023，TOG，DOI 10.1145/3618331，ReSTIR 用于可微/逆向渲染）。

---

## 2. Path Guiding（路径引导）

> Cycles 现状：**已集成 OpenPGL**（Blender 3.4+，实验性；wiki 3.4 发布说明明确 "Path guiding was integrated into Cycles using Intel's Open Path Guiding Library"）。OpenPGL 是 Apache-2.0，直接可再分发/修改。

### 2.1 *Path Guiding in Production*（奠定生产级引导的基础论文）
- **论文**：Thomas Müller, Mark Salvi, Alexander Keller — **SIGGRAPH 2019**（ACM TOG 38(4)）
- **代码**：官方无独立代码；**其生产实现即 OpenPGL**（见 2.3）。作者课程页：https://cgg.mff.cuni.cz/~jaroslav/papers/2019-path-guiding-course/index.htm （SIGGRAPH 2019 课程 + 论文 PDF）
- **语言/框架**：C++（论文/课程），OpenPGL 为 C++17
- **许可证**：论文无代码；OpenPGL Apache-2.0
- **集成难度**：Cycles 已集成等价能力，难度 1（后续改进 2–3）。

### 2.2 算法基础代码（注意许可证！）
- *Practical Path Guiding for Efficient Light-Transport Simulation*（Thomas Müller et al.，**EGSR 2017**，OpenPGL 的理论基础）
- ✅ https://github.com/Tom94/practical-path-guiding — **GPL-3.0** ⚠️ **不可直接并入 Apache-2.0 的 Cycles**
- ✅ https://github.com/igorawratu/practical-path-guiding — **GPL-3.0**（复现）
- **集成难度**：若只看算法思想 2；若抄代码则**许可证冲突（GPL）**。

### 2.3 Intel OpenPGL — *Open Path Guiding Library*
- **项目**：Intel 开源的路径引导库（表面+体积引导、guided MIS），由 *Path Guiding in Production* / *Practical Path Guiding* 演进而来的生产实现；现由 **OpenPathGuidingLibrary** 组织维护（原 RenderKit），并已提交 ASWF 提案（2025-12）。
- **代码**：✅ https://github.com/OpenPathGuidingLibrary/openpgl （**Apache-2.0**，LICENSE.txt 已验证；C++17，依赖 Embree）
- **与 Cycles 的关系**：**Blender 3.4 起 Cycles 已依赖 OpenPGL**（`src/guiding/`），故本主题集成难度为 **1–2**：后续工作集中在①体积引导开关、②guided MIS 权重、③与 Cycles 的 light tree/MNEE 组合、④训练预算调度。

### 2.4 *Neural Path Guiding*（NPG）
- **论文**：Thomas Müller, Fabrice Rousselle, Alexander Keller, Jan Novák — **SIGGRAPH 2020**（ACM TOG 39(4)）
- **代码**：**官方未公开**；社区复现：
  - ✅ https://github.com/dom-wuest/NeuralPathGuiding （⚠️无 LICENSE 文件；DXR/Falcor 系，NRC/NPG 方向个人实现，偏向 NRC）
- **语言/框架**：论文为 C++/CUDA（NVIDIA）；复现为 DirectX Raytracing
- **许可证**：官方无；复现未声明
- **集成难度：5**（无官方代码 + 神经网络在线训练/推理集成进 kernel 的工程量巨大；NPG 针对低 spp 实时，对离线收敛性增益不明显，优先级低）。

### 2.5 *Real-time Neural Radiance Caching for Path Tracing*（NRC，顺带）
- **论文**：Thomas Müller, Fabrice Rousselle, Jan Novák, Alexander Keller — **SIGGRAPH 2021**（ACM TOG 40(4)）
- **代码**：官方未公开；复现 ✅ https://github.com/julcst/photon-nrc （NRC/SPPM/Photon-NRC in Falcor，**BSD-3-Clause**）
- **集成难度：5**（同上；离线方向收益低）。

### 2.6 2021 之后的 Path Guiding 进展
- ***Online Neural Path Guiding with Normalized Anisotropic Spherical Gaussians***（ONPG-ASG）— 东北大学（Tohoku）团队 — **ACM TOG 2024**（DOI 10.1145/3649310；arXiv 2303.08064）。在线神经网络引导 + 各向异性 ASG 基；未发现公开代码；**集成难度 5**。
- ***Path Guiding in Production and Recent Advancements*** — **SIGGRAPH 2025 课程**（ACM DOI 10.1145/3721241.3733994），含 Disney Hyperion 的 *Path Guiding Surfaces and Volumes* 案例分析（https://www.yiningkarlli.com/projects/pathguidingcourse2025.html 、https://disneyanimation.com/publications/path-guiding-in-production-and-recent-advancements/ ）。**这份课程是 2025 年引导技术现状的最佳综述**，且与"生产渲染器（类 Cycles）集成引导"直接相关。
- *ReSTIR PG*（SA 2025，见 1.7）：ReSTIR × guiding 的结合，Cycles 可重点关注其"引导 + 空间复用"思路。

---

## 3. 神经重要性采样 / 学习式采样（Neural Importance Sampling）

> 离线语境价值：学习式采样可在**难以手工建模的分布**（体积、焦散、many-light 选择）上降低方差；但需要**训练阶段**（Cycles 目前无 ML 训练基础设施），工程成本集中在数据收集/训练/推理管线。

### 3.1 *Neural Importance Sampling*（NIS）
- **论文**：Thomas Müller, Fabrice Rousselle, Jan Novák, Alexander Keller — **SIGGRAPH 2022**（ACM TOG 41(4)）
- **代码**：**官方未公开独立仓库**（本次调研未找到官方实现；NVIDIA 论文页无下载）。备注：NIS 与其后 NRC/NPG 同源，可参考 dom-wuest/NeuralPathGuiding（⚠️无许可证）。
- **语言/框架**：论文为 C++/CUDA；**集成难度：5**
- **兼容性**：NIS 的"归一化流 + 在线更新目标分布"理论可直接读论文迁移；Cycles 中对应位置是 `src/kernel/sample/` 的采样器与 `integrator` 的 next-event estimation 选择分布。

### 3.2 *Neural Product Importance Sampling via Warp Composition*（NPIS）
- **论文**：Joey Litalien, Miloš Hašan, Fujun Luan, Krishna Mullia, Iliyan Georgiev — **SIGGRAPH Asia 2024**（ACM TOG 43(6)，DOI 10.1145/3680528.3687566；arXiv 2409.18974）
- **代码**：未发现公开仓库（arxiv 页无代码链接）；**集成难度：5**
- **要点**：用 warp composition 学习"BRDF × 光源"乘积分布，是 2024 年神经采样代表作，方向与 Cycles 的 NEE（emitter sampling）直接相关。

### 3.3 *Neural Importance Sampling of Many Lights*
- **论文**：Pedro Figueiredo, Qihao He, Steve Bako, Nima Khademi Kalantari — **SIGGRAPH 2025**（arXiv 2505.11729）
- **代码**：✅ https://github.com/pedrovfigueiredo/nis-manylights （官方实现；⚠️无 LICENSE 文件——默认保留版权，需联系作者）
- **语言/框架**：项目页为混合神经方法（PyTorch 训练 + 渲染集成；见 https://pedrovfigueiredo.github.io/projects/manylights/SIGGRAPH_2025_Importance_Sampling/index.html ）
- **集成难度：4–5**（many-light 场景；与 Cycles light tree 的关系需要评估——light tree 已高效，神经选择分布的价值主要在"极多光源 + 复杂可见性"场景）。

### 3.4 其他相关（2023–2025）
- ONPG-ASG（TOG 2024）本质上也是神经引导采样（见 2.6）。
- 未在 SIGGRAPH 主序列检索到以 "neural MIS" 为题的独立主论文；"neural MIS" 的提法多见于 NIS/NPIS 等学习采样与 MIS 结合的叙述。若要追踪，可关注 Müller 团队的 NRC/NIS 系列后续（多为实时方向）。

---

## 4. 自适应采样（Adaptive Sampling）

> Cycles 现状：**已有成熟的自适应采样**（`src/integrator/adaptive_sampling.cpp`，Render Setting 的 Noise Threshold / Min Samples；基于逐像素方差估计，思想源头可追溯到 Moon et al. 2009 *Adaptive Rendering with Linear Predictions* 一类的 variance-based 自适应）。因此本节聚焦"可改进点"。

- ***Denoising-Aware Adaptive Sampling for Monte Carlo Ray Tracing***（DAAS）— Arthur Firmino, Jeppe Revall Frisvad, Henrik Wann Jensen（Luxion / DTU）— **SIGGRAPH 2023**（DOI 10.1145/3588432.3591537；PDF：http://www2.compute.dtu.dk/~jerf/papers/daas.pdf ）。思路：结合**去噪器误差**指导采样预算，比纯方差更贴合生产流程。未发现公开代码。**集成难度：2**（`src/integrator/` 内调度逻辑 + 与 Cycles 的 OIDN/OptiX 去噪器交互）。
- ***Practical Error Estimation for Denoised Monte Carlo Image Synthesis*** — Arthur Firmino, Ravi Ramamoorthi, Jeppe Revall Frisvad, Henrik Wann Jensen — **SIGGRAPH 2024**（DOI 10.1145/3641519.3657511）。给去噪后图像提供可靠误差估计，可作为自适应采样/终止判据的升级。未发现公开代码。**集成难度：2**。
- 结论：自适应采样主题是六项中**最容易落地**的（难度 2）；主要工作集中在 CPU 侧 scheduler 与像素缓冲，不涉及 kernel 结构性改动。

---

## 5. 高级 MIS / 多通道（Gradient-domain、Variance-aware MIS 等）

### 5.1 Gradient-domain 系列（多通道：主通道 + 梯度通道 + 屏幕空间重建）
- ***Gradient-Domain Path Tracing*** — **Kettunen et al.**：Markus Kettunen, Marco Manzi, Miika Aittala, Jaakko Lehtinen, Frédo Durand, Matthias Zwicker — **SIGGRAPH 2015**（ACM TOG 34(4)）
  - 复现：✅ https://github.com/githole/gdpt （⚠️无 LICENSE 文件）
  - **集成难度：5**：需要梯度路径采样（屏幕空间导数的 path integral）+ 重建（screened Poisson，需要 A-buffer/邻域像素内存）。Cycles 当前无任何梯度域基础设施。
- ***Temporal Gradient-Domain Path Tracing*** — Manzi, Kettunen, Durand, Zwicker — **SIGGRAPH 2016**（ACM TOG 35(4)）
  - 代码：✅ https://github.com/harskish/temporal-gpt （⚠️无 LICENSE 文件）
  - 离线动画序列的时域梯度域，参考价值高但工程量大。
- ***A Survey on Gradient-Domain Rendering*** — Adrien Gruson 等 — **Eurographics 2019 STAR**（CGF 38(2)）— https://profs.etsmtl.ca/agruson/publication/2019_gradientstar/ 。**梯度域渲染最好的综述**，含 G-PT/G-BDPT/G-PDE 与重建方案对比。
- **兼容性备注**：梯度域与 Cycles 的"每像素独立累积 + 后处理去噪"管线冲突较大（需要保留邻域像素的梯度样本与重建步骤）；优先级低于 path guiding / adaptive sampling。

### 5.2 Variance-aware / Efficiency-aware MIS
- ***Efficiency-Aware Multiple Importance Sampling for Bidirectional Rendering Algorithms*** — Pascal Grittmann, Ömercan Yazici, Iliyan Georgiev, Philipp Slusallek — **SIGGRAPH 2022**（ACM TOG 41(4)，DOI 10.1145/3528223.3530126）
  - 代码：✅ https://github.com/pgrit/EfficiencyAwareMIS （**C# / SeeSharp**（.NET），⚠️无 LICENSE 文件；README 注明为论文代码的重实现、基于 SeeSharp）
  - **集成难度：2–3**：理论（以方差/效率为目标优化 MIS 权重，而非标准 balance heuristic）可直接应用到 Cycles 的 BDPT/路径采样 MIS 权重计算（`src/kernel/integrator/` 的 MIS 权重函数）；C# 实现只能作参考，需 C++ 重写。
- **说明**：用户主题中的 "Variance-aware MIS" 无精确对应 SIGGRAPH 论文；Efficiency-Aware MIS 是其最接近的正式工作。另可参考 MIS 经典文献（Veach & Guibas 1995 balance/power heuristic——Cycles 已在用）。

---

## 6. MLT / BDPT / 困难光传输（VCM、PSSMLT 及 2019–2026 改进）

> Cycles 现状：**无 MLT/BDPT/VCM integrator**（只有 path tracing + light tree + MNEE）。pbrt-v4 也移除了 MLT（pbrt-v3 有 MLT）；Mitsuba 3 / LuxCoreRender 保留了 BDPT/VCM/PSSMLT 参考实现。

### 6.1 经典基础（实现参考）
- ***Vertex Connection and Merging***（VCM）— Georgiev et al. — **SIGGRAPH 2012**（ACM TOG 31(4)）
  - 参考实现：✅ **SmallVCM** https://github.com/SmallVCM/SmallVCM （**MIT**，Tomas Davidovic；C++ 单文件式小型渲染器）；另有 **Mitsuba 3**（**BSD-3-Clause**）与 **LuxCoreRender**（**Apache-2.0**）内置 VCM。
  - 注：早先的 wataru-usui/vcm-impl 仓库已删除/私有（`git ls-remote` 实测不存在）。
  - **集成难度：4–5**（光子图/合并结构 + 半径控制，与 Cycles 的 streaming 路径架构差异大）。
- ***Primary Sample Space MLT***（PSSMLT）— Kelemen et al. — Eurographics 2002
  - 参考实现：**pbrt-v3**（**BSD-2-Clause**，metropolis sampler + MLT integrator）、**Mitsuba 3**（BSD-3-Clause）、**NVIDIA Fermat**（仓库 ✅ https://github.com/NVlabs/fermat 存在，但⚠️无 LICENSE 文件，默认保留版权；Fermat 含 PSSMLT/BDPT/PT，文档 https://nvlabs.github.io/fermat/ ）。
  - **集成难度：3–4**（新 integrator 类型；MCMC 状态与 mutation 需要在 `src/kernel/integrator/` 新增；OptiX 后端因自定义控制流受限，主要走 CPU）。

### 6.2 SIGGRAPH 2019–2026 的 MLT/MCMC 改进（有代码的）
- ***Langevin Monte Carlo Rendering with Gradient-based Adaptation*** — Fujun Luan（Luan），（Luan, Zhao, Bala, Gkioulekas）— **SIGGRAPH 2020**（ACM TOG 39(4)，DOI 10.1145/3386569.3392382）
  - 代码：✅ https://github.com/luanfujun/Langevin-MCMC （**MIT**，C++/CUDA）
  - 核心：用 MCMC 的梯度（Langevin dynamics）自适应提议分布，显著改进焦散/硬光传输。
  - **集成难度：4**（MIT 可借鉴；需要 CUDA 侧 MCMC 状态 + 梯度计算；Cycles 的 `kernel/integrator` 无对应结构，需新 integrator 模式）。
- ***Delayed Rejection Metropolis Light Transport*** — Rioux-Lavoie, ..., Gruson 等 — **SIGGRAPH 2020**（ACM TOG 39(4)，DOI 10.1145/3388538）
  - 代码：作者页 "Code & Data" 下载（https://profs.etsmtl.ca/agruson/publication/2020_delayed/ ）；⚠️许可证未标注（默认保留版权，需联系作者确认）
  - **集成难度：4**。
- ***Path Differential-Informed Stratified MCMC and Adaptive Forward Path Sampling*** — Tobias Zirr, Carsten Dachsbacher（KIT）— **SIGGRAPH Asia 2020**（ACM TOG 39(6)，Article 246；项目页 https://cg.ivd.kit.edu/pathdiff-mc.php ，页面 Downloads 区提供代码；⚠️许可证未标注）
  - 核心：用路径微分信息指导 MCMC 分层与提议，显著降低 MLT 在复杂场景的方差。
  - **集成难度：4**。

### 6.3 2021–2026 MLT 现状
- 本次调研在 2021–2026 的 SIGGRAPH/SIGGRAPH Asia 主序列**未检索到新的通用 MLT 主论文**（MLT 的活跃研究多转到 EGSR/HPG 或实时方向的 ReSTIR 家族）。困难光传输的主流路线已从 MLT 转向：①ReSTIR（实时）②path guiding + 更好的采样器（离线）③梯度域/控制变量（如 ReSTCV 2026）。
- 结论：对 Cycles 而言，MLT 系列优先级**低于 path guiding 改进与 ReSTIR 理论借鉴**；若做，建议从 **PSSMLT（pbrt-v3，BSD-2）** 或 **Langevin MCMC（MIT）** 起步，新增一个可选 integrator。

---

## 7. 全量汇总表

| # | 论文/项目 | 年份·会议 | 代码仓库（已验证 ✅） | 语言/框架 | 许可证 | 集成难度 |
|---|---|---|---|---|---|---|
| 1 | ReSTIR DI | 2020·SIGGRAPH | karel-tomanec/Falcor-ReSTIR ✅；lindayukeyi/ReSTIR_DX12 ✅(无LIC) | C++/HLSL(DXR/Falcor) | 复现 BSD-3 / 无LIC | 5 |
| 2 | ReSTIR GI | 2021·SIGGRAPH | RTXDI（专有） | C++/HLSL(Falcor) | NVIDIA SDK | 5 |
| 3 | Volumetric ReSTIR | 2021·SA | DQLin/VolumetricReSTIRRelease ✅ | C++/HLSL(Falcor) | BSD-3 | 4–5 |
| 4 | GRIS | 2022·SIGGRAPH | DQLin/ReSTIR_PT ✅ | C++/HLSL(Falcor) | BSD-3 | 4 |
| 5 | ReSTIR PT | 2023·SIGGRAPH | DQLin/ReSTIR_PT ✅ | C++/HLSL(Falcor) | BSD-3 | 4 |
| 6 | Conditional ReSTIR (CRIS) | 2023·SA | NVlabs/conditional-restir-prototype ✅ | C++/HLSL(Falcor) | **NC 非商业** | 5 |
| 7 | Area ReSTIR | 2024·SIGGRAPH | guiqi134/Area-ReSTIR ✅ | C++/HLSL(Falcor) | BSD-3 | 5 |
| 8 | Reservoir Splatting | 2025·SIGGRAPH | Jebbly/Reservoir-Splatting ✅ | C++/HLSL(Falcor 8) | BSD-3 | 4 |
| 9 | ReSTIR FG / Guided FG+ | EGSR 2024/2025 | TU-Clausthal-Rendering/{ReSTIR-FG,Guided-ReSTIR-FG-Plus} ✅ | C++/HLSL(Falcor) | BSD-3 | 4 |
| 10 | Manifold Hybrid Shift | 2024-25(论文级) | takkasila/Manifold-Hybrid-Shift ✅ | C++/HLSL(Falcor) | BSD-3 | 4 |
| 11 | ReSTIR PG | 2025·SA | 无公开代码 | — | — | 4 |
| 12 | ReSTCV | 2026·SIGGRAPH | Hercier/ReSTCV ✅ | C++/HLSL(Falcor) | BSD-3 | 4 |
| 13 | Path Guiding in Production | 2019·SIGGRAPH | → OpenPGL ✅ | C++ | Apache-2.0 | 1–2（已集成） |
| 14 | Practical Path Guiding | 2017·EGSR | Tom94/practical-path-guiding ✅ | C++ | **GPL-3.0** | 思想2/抄码不可行 |
| 15 | OpenPGL | 2019– | OpenPathGuidingLibrary/openpgl ✅ | C++17(+Embree) | **Apache-2.0** | 1–2 |
| 16 | Neural Path Guiding | 2020·SIGGRAPH | dom-wuest/NeuralPathGuiding ✅(无LIC) | C++/DX12 | 官方无/复现无LIC | 5 |
| 17 | Neural Radiance Caching | 2021·SIGGRAPH | julcst/photon-nrc ✅ | C++/HLSL(Falcor) | BSD-3 | 5 |
| 18 | ONPG-ASG | 2024·TOG(SIGGRAPH 系) | 无公开代码 | — | — | 5 |
| 19 | NIS | 2022·SIGGRAPH | 官方未公开 | — | — | 5 |
| 20 | NPIS | 2024·SA | 无公开代码 | — | — | 5 |
| 21 | NIS of Many Lights | 2025·SIGGRAPH | pedrovfigueiredo/nis-manylights ✅ | PyTorch+C++ | 无LIC | 4–5 |
| 22 | DAAS | 2023·SIGGRAPH | 无公开代码（论文 PDF 官方提供） | — | — | 2 |
| 23 | Practical Error Estimation | 2024·SIGGRAPH | 无公开代码 | — | — | 2 |
| 24 | Gradient-Domain PT | 2015·SIGGRAPH | githole/gdpt ✅(无LIC) | C++ | 无LIC | 5 |
| 25 | Temporal G-PT | 2016·SIGGRAPH | harskish/temporal-gpt ✅(无LIC) | C++ | 无LIC | 5 |
| 26 | G-Domain Survey | 2019·EG STAR | 综述（无代码） | — | — | 参考 |
| 27 | Efficiency-Aware MIS | 2022·SIGGRAPH | pgrit/EfficiencyAwareMIS ✅ | C#/.NET(SeeSharp) | 无LIC | 2–3 |
| 28 | VCM | 2012·SIGGRAPH | SmallVCM ✅；Mitsuba3；LuxCore | C++ | **MIT** / BSD-3 / Apache-2.0 | 4–5 |
| 29 | PSSMLT | 2002·EG | pbrt-v3；Mitsuba3；Fermat ✅(无LIC) | C++ | BSD-2 / BSD-3 / 无LIC | 3–4 |
| 30 | Langevin MCMC | 2020·SIGGRAPH | luanfujun/Langevin-MCMC ✅ | C++/CUDA | **MIT** | 4 |
| 31 | Delayed Rejection MLT | 2020·SIGGRAPH | 作者页下载 | C++ | 未标注 | 4 |
| 32 | Path-Differential MCMC | 2020·SA | KIT 项目页下载 | C++ | 未标注 | 4 |

---

## 8. 对 Cycles 集成的总体建议（按优先级）

1. **P0（低成本、高契合）**：
   - **Path guiding 深度集成**：OpenPGL 已在 Cycles 内（Apache-2.0）。参考 *Path Guiding in Production and Recent Advancements*（SIGGRAPH 2025 课程）与 ReSTIR PG（SA 2025）的引导思路做增量改进（guided MIS、体积引导、训练调度）。
   - **自适应采样升级**：参考 DAAS（2023）与 Practical Error Estimation（2024）改进 `src/integrator/adaptive_sampling.cpp` 的误差判据（难度 2）。
   - **Efficiency-Aware MIS 权重**：把 EAMIS（2022）的方差最优权重引入 MIS 计算（难度 2–3；理论可直接读，C# 代码仅参考）。
2. **P1（中期、收益明确）**：
   - **GRIS 理论应用于离线 many-light**：以 DQLin/ReSTIR_PT（BSD-3）为参考，在 Cycles 的 light sampling 上引入 reservoir/重采样（离线版，不做帧间复用，只做空间/样本间复用），配合 light tree（难度 4）。
   - **新增 PSSMLT/MLT 可选 integrator**：以 pbrt-v3（BSD-2）为参考实现（难度 3–4，CPU 为主）。
3. **P2（长期/低优先级）**：
   - **Langevin MCMC**（MIT 代码可借鉴，难度 4）、**Gradient-domain**（难度 5，需新建重建管线）、**VCM**（难度 4–5）、**神经采样 NIS/NPIS**（难度 5，缺官方代码与 ML 管线）。
4. **红线（许可证）**：GPL-3.0（practical-path-guiding 代码）、Nvidia NC（conditional-restir-prototype）、RTXDI 专有 SDK、以及所有"无 LICENSE 文件"的仓库（默认保留版权），一律**只读思路、禁止并入**；BSD-3/MIT/Apache-2.0 可正常并入（注意保留版权头）。

---

## 附：URL 验证说明
- ✅ = 本次调研中通过 GitHub raw / API / `git ls-remote` 实测存在。
- "无LIC" = 仓库无 LICENSE 文件，按默认法律状态（保留版权）对待，需要作者授权。
- 会议缩写：SA = SIGGRAPH Asia；TOG = ACM Transactions on Graphics；EGSR = Eurographics Symposium on Rendering；EG = Eurographics。
