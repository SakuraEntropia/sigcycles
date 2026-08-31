# Cycles 实时渲染模式集成调研：2019–2026 实时渲染研究清单

> 调研范围：面向 Blender Cycles（`/Users/faputa/Documents/Entro-Cycles/cycles`，Apache-2.0，基线 v5.0.0-461）**实时渲染模式**（interactive viewport、低采样率、时域复用、激进去噪）的 SIGGRAPH/HPG/EGSR 等实时渲染研究前期调研。
> 调研方法：web_search 逐题检索 + GitHub（API/raw/html）逐一核实公开代码仓库与许可证。代码 URL 均经实际访问验证（HTTP 状态或内容确认），许可证以仓库 LICENSE 文件内容为准（GitHub API 的 spdx 分类不可靠，已用原文核实）。
> 中文报告、技术名英文。

---

## 0. Cycles 集成上下文（评分与兼容性备注的依据）

### 0.1 设备 / Kernel 抽象（已核实）
- Cycles 的设备后端位于 `src/device/`：`cpu / cuda / hip / hiprt / metal / multi / oneapi / optix / dummy`。
- Kernel 侧是**跨后端可移植子集**：同一份 `src/kernel` 源码经 `src/kernel/device` 下的包装头（CUDA/HIP/MSL/OPTIX/oneAPI/CPU）编译到各后端。**任何要进 kernel 的算法必须用这个可移植子集书写**（不能用 CUDA 独有特性 / HLSL / DXC），否则要分别维护多份后端实现。
- **独立构建现状**：CPU、METAL 后端可用；`WITH_CYCLES_DEVICE_OPTIX` 默认 ON 但需要 OptiX SDK + CUDA 工具链；`WITH_CYCLES_OPENIMAGEDENOISE` 默认 ON（OIDN 是 CPU 库，Apache-2.0，独立构建可用）。
- **去噪基础设施已存在**：`src/device/denoise.h` 定义 `DENOISER_OPTIX / DENOISER_OPENIMAGEDENOISE`，并已支持 albedo / specular_albedo / normal / roughness / depth / **motion / backward_motion / specular_motion** 等引导 pass 以及 prefilter（NONE/FAST/ACCURATE）与 quality（HIGH/BALANCED/FAST）——**说明 Cycles 已为"时域去噪"备好了 motion vectors 通道**，OIDN 3.0 时域模型可直接挂接。
- SPPM（随机渐进光子映射）已从 Cycles 移除（kernel 中无 sppm 残留）。

### 0.2 lamp bug 的影响面（用户给定前提）
独立构建中 **lamp（点光/面光/聚光等显式光源）光照有 bug**。以下技术都依赖 lamp 直接光，会受影响；**纯 emissive mesh + 环境光场景不受影响**：
- 依赖 lamp 直接光采样/发射：ReSTIR DI / ReSTIR GI / ReSTIR PT（NEE 项）、DDGI/NRC/OSCGI/SHARC 的 direct 项、Stochastic Lightcuts、ReGIR、光子类算法、SVGF/BMFR 的 direct 缓冲内容（滤波算法本身不依赖 lamp，但输入渲染结果会缺直接光）。
- 不依赖 lamp（后处理/纯滤波）：OIDN、OptiX denoiser、NRD、TAAU、FSR、DLSS/XeSS（上采样）、SVGF/BMFR 的滤波逻辑本身。

### 0.3 难度评分说明（1–5）
1 = 几乎零改动（库/API 已集成）；2 = 小型 kernel 移植（纯算法，数天-周）；3 = 中等移植（算法 + 状态缓冲/管线接入）；4 = 大工作量（需要重写官方 HLSL/DX12 实现进 Cycles 可移植 kernel，或深度管线集成）；5 = 极高（神经网络运行时、专有硬件/SDK 依赖，或 CPU/METAL 上基本不可行）。

---

## 1. ReSTIR 与 reservoir-based 光照（实时应用）

| 条目 | 年份/会议 | 官方代码仓库 | 语言/框架 | 许可证 | 难度 | lamp 依赖 |
|---|---|---|---|---|---|---|
| ReSTIR（ReSTIR DI）| SIGGRAPH 2020（TOG 39(4)）| 官方=RTXDI SDK；第三方=tatran5/…ReSTIR | HLSL + C++，D3D12/Vulkan | **NVIDIA RTX SDKs LICENSE（专有）**；第三方未注明 | 3 | 是 |
| ReSTIR GI | SIGGRAPH 2021（TOG 40(4)）| 官方=RTXDI v2.0 | HLSL + C++ | 专有（同上）| 4 | 是 |
| GRIS（ReSTIR 理论基础）| SIGGRAPH 2022（TOG 41(4)）| **DQLin/ReSTIR_PT** | C++/HLSL，Falcor 4.4，DX12，Windows | **BSD-3-Clause** | 4 | 是 |
| ReSTIR PT（GRIS 的路径追踪应用）| 2022–2023（课程/发布）| 同上 DQLin/ReSTIR_PT | Falcor 4.4 + DX12 + OptiX | BSD-3-Clause | 4 | 是 |
| RTXDI SDK（DI/GI/PT 官方集成包）| v3.0（2024 引入 ReSTIR PT）| NVIDIA-RTX/RTXDI | HLSL 库 + C++，D3D12/Vulkan（Donut 示例）| NVIDIA RTX SDKs LICENSE（专有）| 3–4 | 是 |
| RTXGI / DDGI | JCGT 8(2) 2019（I3D）| NVIDIA-RTX/RTXGI（DDGI 模块）| HLSL + D3D12 | 专有（RTX SDK LICENSE）| 4 | 是 |
| Volumetric ReSTIR | SIGGRAPH Asia 2021（TOG 40(6)）| **DQLin/VolumetricReSTIRRelease** | Falcor/DX12 | **BSD-3-Clause** | 4 | 是（体积光源）|
| Conditional ReSTIR（CRIS / Suffix ReSTIR）| SIGGRAPH Asia 2023 | NVlabs/conditional-restir-prototype | 基于 NVIDIA 内部渲染器（CUDA/OptiX）| **Nvidia Source Code License-NC（非商用）** | 4 | 是 |
| ReSTIR BDPT（双向 ReSTIR，焦散）| 2025（NVIDIA 实时渲染研究页）| 无公开代码 | — | — | 4–5 | 是 |
| Neural Importance Sampling of Many Lights | 2025（arXiv:2505.11729；SIGGRAPH 2025）| 无官方代码（第三方 pedrovfigueiredo/nis-manylights）| PyTorch/渲染器结合 | 未公开 | 5 | 是 |
| ReSTCV（Spatio-Temporal Control Variates with ReSTIR）| SIGGRAPH 2026（仓库自标注，未独立核实）| Hercier/ReSTCV | 未核实 | 未核实 | 4–5 | 是 |

### 1.1 详解与 Cycles 备注
- **ReSTIR（Spatiotemporal Reservoir Resampling for Real-time Ray Tracing with Dynamic Direct Lighting）**，Bitterli et al.，SIGGRAPH 2020。官方实现即 **RTXDI**（NVIDIA-RTX/RTXDI，v1.0 即 ReSTIR DI）：HLSL 着色器 + C++ 宿主代码，可嵌入 D3D12/Vulkan 渲染器。**许可证为 NVIDIA RTX SDKs LICENSE（专有 EULA，可自由用于开发，但不可再分发源码、不可商用闭源集成到开源产品中——直接集成进 Apache-2.0 的 Cycles 有许可风险）**。第三方复刻：tatran5/Reservoir-Spatio-Temporal-Importance-Resampling-ReSTIR（C++，许可证未注明）。
- **ReSTIR GI**（Ouyang et al. 2021，"ReSTIR GI: Path Resampling for Real-Time Path Tracing"）：RTXDI v2.0 官方实现。对漫反射间接光的 path prefix 时空复用，需要 shift mapping + 去噪配合。
- **GRIS / ReSTIR PT**（Lin, Kettunen, Bitterli, Pantaleoni, Yuksel, Wyman，SIGGRAPH 2022）：**官方源码已发布**为 `DQLin/ReSTIR_PT`（BSD-3-Clause），以 "ReSTIRPTPass" 形式集成在 **Falcor 4.4**（DX12/Windows/OptiX）。ReSTIR PT 是 GRIS 理论在路径追踪上的应用（处理 hard shadow 与 many-lights、支持镜面路径复用），同仓库覆盖。Pxy951/ReSTIR_PT 是第三方移植（BSD-3-Clause）。SIGGRAPH 2023 有配套课程 *A Gentle Introduction to ReSTIR Path Reuse in Real-Time*（intro-to-restir.cwyman.org，免费 slides，无代码）。
- **RTXDI SDK**：集成 ReSTIR DI（v1）、ReSTIR GI（v2）、ReSTIR PT（v3）的官方"标准答案"，含 MinimalSample（单 pass 最小实现）与 FullSample。**可作算法参考实现，但许可证不允许直接搬源码进 Cycles**。
- **RTXGI / DDGI**（Majercik et al.，JCGT 2019）：probe-based 辐射度场（ray-traced irradiance fields），随时间收敛；官方在 RTXGI 仓库（DDGI 模块）。probe 更新 + 插值是纯算法，**可移植进 Cycles kernel**，但官方是 HLSL/D3D12，需要重写；且依赖 lamp 直接光。
- **Volumetric ReSTIR**（Lin et al.，SIGGRAPH Asia 2021）：官方代码 BSD-3-Clause（Falcor/DX12）。Cycles 有体积渲染管线，集成复杂度高。
- **Conditional ReSTIR**（Kettunen et al.，SIGGRAPH Asia 2023）：**许可证是 Nvidia Source Code License-NC（非商用）**，Cycles 不能集成。
- **ReSTIR BDPT**（2025）：双向 ReSTIR 路径追踪，支持焦散；研究页在 research.nvidia.com/labs/rtr（hedstrom2025restir），无公开代码。
- **Neural Importance Sampling of Many Lights**（Lin et al.，2025）：神经网络学习光源重要性采样，替代 reservoir；属于 2025 新方向，无官方代码，且需要神经网络运行时。
- **Cycles 集成难度**：ReSTIR 系列的 reservoir 逻辑本身与 API 无关，理论上可写进 Cycles 可移植 kernel（reservoir 状态缓冲 + 时空复用 pass）；但需要 ①重写官方 HLSL 实现（无法复用 Falcor/DX12 代码），②把 Cycles 的 lamp/emissive 光源采样接入 candidate 生成，③配合去噪器与 motion vectors（Cycles 已有）。评分 3（DI）到 4（GI/PT）。lamp bug 会让 ReSTIR 的 direct 项直接出错。

---

## 2. 时域累积与时空复用（temporal accumulation / spatiotemporal reuse）

| 条目 | 年份/会议 | 代码 | 语言/框架 | 许可证 | 难度 | 备注 |
|---|---|---|---|---|---|---|
| SVGF（Spatiotemporal Variance-Guided Filtering）| HPG 2017（基线）| 社区实现：jacquespillet/SVGF、TheVaffel/…、blxl909/…、HummaWhite/Project4-CUDA-Denoiser | GLSL/CUDA 等 | 各异（多数未注明/开源）| 2 | 官方无独立仓库（NVIDIA research 页有描述）|
| A-SVGF（Adaptive SVGF 变体）| 2021–2023 | Pine 渲染器内 a_svgf（gitea.com/wicstas/pine）| C++/Vulkan | 开源 | 2 | SVGF 的自适应变体 |
| BMFR（Blockwise Multi-Order Feature Regression）| SIGGRAPH Asia 2020 | **hchoi405/bmfr（MIT）**；**gtong-nv/BMFR-DXR-Denoiser（BSD-3-Clause，DXR 实现）** | CUDA / DXR | MIT / BSD-3-Clause | 3 | 特征回归 + 时域重投影 |
| A Survey of Temporal Antialiasing Techniques | EG 2020 STAR（CGF 39(2)）| 无（综述）| — | — | N/A | 时域累积技术全景综述 |
| Gradient Estimation for Real-Time Adaptive Temporal Filtering | SIGGRAPH 2018（基线）| 无公开代码 | — | — | 3 | 自适应时域滤波梯度估计 |
| Spatiotemporal Reservoir（ReSTIR 时空复用）| 2020+ | 见第 1 章 | — | — | 3–4 | 跨帧/跨像素 reservoir 复用 |
| OIDN 3.0 时域去噪 | 2026 | 见第 3 章 | CPU | Apache-2.0 | 1 | 时域去噪进入 OIDN |

### 2.1 详解与 Cycles 备注
- **SVGF**（Schied et al.，HPG 2017）：时域重投影 + 空间方差引导滤波的经典基线（虽在 2019 之前，但是该领域一切后续工作的参照物）。官方源码没有独立仓库（NVIDIA 只给了 research 页与论文），但社区实现众多。**集成进 Cycles 难度低（2）**：纯滤波算法，Cycles 已有 albedo/normal/roughness/depth/motion 引导通道；可写进 kernel 或做成 CPU 侧后处理。
- **BMFR**（Tong/Choi 等，SIGGRAPH Asia 2020）：分块多阶特征回归重建，作者仓库 **hchoi405/bmfr（MIT）** 与 **gtong-nv/BMFR-DXR-Denoiser（BSD-3-Clause，DXR 实现）**。需要特征缓冲 + 时域重投影 + 多尺度回归；**许可证友好（MIT/BSD-3）**，可移植进 Cycles kernel（难度 3）。依赖 lamp 只体现在输入渲染质量上。
- **时域累积通用模式**：EG 2020 STAR 综述（Yang/Liu/Salvi 等）系统总结了 reprojection/clamping/history 管理；Cycles 实时模式做时域复用时可参考其中的 history 有效性判断、motion vector 异常处理。
- **Cycles 现状**：Cycles 已有 motion / backward_motion / specular_motion pass 与 OptiX 时域去噪接口；缺的是 kernel 内时域累积（reproject + blend）与 SVGF 类方差引导滤波。OIDN 3.0（2026）时域模型可覆盖大部分"激进去噪"需求（见第 3 章）。

---

## 3. 实时去噪（real-time denoising）

| 条目 | 年份 | 代码 | 语言/框架 | 许可证 | 难度 | Cycles 备注 |
|---|---|---|---|---|---|---|
| Intel Open Image Denoise（OIDN）| v1 2019 / v2.0 2023 / **v3.0 2026（时域去噪）** | **RenderKit/oidn** | C++/ISPC，CPU（v2 起 SYCL/CUDA/HIP）| **Apache-2.0** | 1 | **Cycles 已集成**（DENOISER_OPENIMAGEDENOISE），v3 时域模型是升级点 |
| NVIDIA OptiX Denoiser | 2019+ | OptiX SDK 内 | CUDA/OptiX | 专有 | 1 | **Cycles 已集成**（DENOISER_OPTIX），仅 CUDA/OptiX 后端可用 |
| NVIDIA NRD（ReBLUR/SIGMA/RELAX）| 2020+ | NVIDIA-RTX/NRD | HLSL（API 无关，D3D12/Vulkan）| **NVIDIA RTX SDKs LICENSE（专有）** | 4 | 许可证 + HLSL→Cycles kernel 重写成本高 |
| KPCN（Kernel-Predicting CNNs）| SIGGRAPH 2017（基线）| 无官方代码 | 神经网络（训练框架）| — | 4–5 | 神经网络推理运行时 |
| Interactive MC Denoising using Affinity of Neural Features | SIGGRAPH 2021（TOG 40(4)）| AlstonXiao/InteractiveDenoiserWithAffinity（第三方）| 神经网络（PyTorch 等）| 仓库无 LICENSE 文件 | 4–5 | 同上 |
| Deep Compositional Denoising（含 "on Frame Sequences" 时域版）| CGF 40(4) 2021（EGSR 2021）；时域版 2023 | 无官方代码（Disney Research 未发布）| 神经网络 | — | 4–5 | 同上 |
| Real-time MC Denoising with Weight-Sharing KPD | CGF 40(4) 2021（EGSR 2021）| **Rendering-at-ZJU/weight-sharing-kernel-prediction-denoising** | 神经网络 | **MIT** | 4 | 有开源实现，仍需要 NN 运行时 |
| BMFR（跨章引用）| 2020 | hchoi405/bmfr（MIT）| CUDA | MIT | 3 | 非神经网络，可移植性最好 |
| Edge-aware denoising framework for real-time mobile ray tracing | 2025（期刊）| 无公开代码 | — | — | 3–4 | 移动端轻量去噪参考 |
| DLSS 3.5 Ray Reconstruction | 2023 | NVIDIA/DLSS SDK（二进制）| 专有 | 专有 | 5 | 全光线重建（去噪+重建合一），仅 NVIDIA |

### 3.1 详解与 Cycles 备注
- **OIDN**：Intel，Apache-2.0，**与 Cycles 的 Apache-2.0 许可完全兼容，且 Cycles 已集成**。v2.0（2023）换了新架构（约 2 倍提速，新增 SYCL/Xe GPU、CUDA、HIP 后端）；**v3.0（2026 年初）新增时域去噪（temporal denoising）**——这直接对应 Cycles 实时模式"激进去噪 + 时域复用"需求：现有 denoise pass（motion/backward-motion）已备好输入。集成难度 1（库升级 + 参数接线）。
- **OptiX Denoiser**：已集成；时域模式强，但依赖 CUDA+OptiX SDK，独立构建（CPU/METAL）不可用。
- **NRD**：NVIDIA 实时去噪库（ReBLUR 时域去噪、SIGMA 阴影、RELAX GI/反射），API 无关、性能极好，**但许可证是专有的 NVIDIA RTX SDKs LICENSE**，且是 HLSL 着色器集——需要整体重写进 Cycles kernel 才符合许可证与后端要求，难度 4。
- **神经网络去噪器（KPCN 家族）**：2019–2026 的神经网络实时去噪（affinity 2021、Deep Compositional 2021/2023、WSKPD 2021 等）质量高但都需要 NN 推理运行时（PyTorch/ONNX/tiny-cuda-nn）与训练管线。**CPU/METAL 独立构建基本不可行**（除非 ONNX Runtime 推理，质量/性能存疑），难度 4–5。WSKPD 有 MIT 开源实现可作参考。**结论：对 Cycles 实时模式，优先 OIDN v3 时域模型 + 现有 OptiX（有 NVIDIA 硬件时），而非把神经网络去噪器搬进 kernel。**
- lamp 依赖：去噪器本身不依赖 lamp（后处理滤波输入），但 lamp bug 会污染输入渲染。

---

## 4. 神经上采样 / 超分重建（neural upscaling）

| 条目 | 年份 | 代码 | 语言/框架 | 许可证 | 难度 | Cycles 备注 |
|---|---|---|---|---|---|---|
| TAAU（Temporal Anti-Aliasing Upsampling）| 2019+（Falcor 示例）| NVIDIA Falcor 仓库内 | HLSL | BSD-3-Clause（Falcor 整体）| 2–3 | 经典时域上采样模板 |
| AMD FSR 2 / FSR 3 | 2022 / 2023 | **GPUOpen-Effects/FidelityFX-FSR** | HLSL/GLSL | **MIT** | 2–3 | 时域上采样 + 帧生成；MIT 兼容 |
| AMD FSR 4 | 2025 | GPUOpen（先"意外"公开，后正式发布）| ML 上采样 + 着色器 | **MIT**（权重随包）| 3–4 | ML 模型，跨平台性待评估 |
| NVIDIA DLSS 2 / 3 / 3.5（含 Ray Reconstruction）| 2019/2022/2023 | NVIDIA/DLSS SDK（GitHub 有仓库，二进制分发）| 专有 SDK | 专有 | 4–5 | 仅 NVIDIA；RR 兼具去噪功能 |
| Intel XeSS 2 / 3 | 2023 / 2025 | intel/xess（SDK）| 头文件 + 二进制 | **闭源 SDK**（源码未开放）| 4–5 | 仅 Intel/跨厂商受限 |
| ExtraSS（Joint Spatial Super Sampling + Frame Extrapolation）| SIGGRAPH Asia 2023（TOG 42(6)）| 无公开代码（项目页 zheng95z.github.io）| — | 未发布 | 4 | 空间超分 + 帧外推联合框架 |
| PatchEX（Patch-based Parallel Extrapolation）| arXiv 2024（2407.17501）；ACM TOG 45（2026 卷）| 无公开代码 | — | — | 4 | 时域超分 + 帧外推 |
| Efficient Video Super-Resolution for Real-time Rendering with Decoupled G-buffer Guidance | CVPR 2025 | 无公开代码 | — | — | 4 | G-buffer 引导的视频超分 |

### 4.1 详解与 Cycles 备注
- **学术脉络**：DLSS 本身不公开算法，但 2019–2026 学术界有对应研究：TAAU（Falcor，开源，BSD-3）是"传统"时域上采样基线；ExtraSS（SIGGRAPH Asia 2023，NVIDIA/UCSB 合作）把空间超分与帧外推统一；PatchEX（2024，ACM TOG 2026 卷）做 patch 级并行外推；CVPR 2025 用 G-buffer 解耦引导视频超分。
- **可落地性排序**：**FSR 2/3（MIT）最友好**——纯着色器时域上采样 + 帧生成，Cycles 已有 motion/depth pass，可作为 viewport 低分辨率渲染 + 上采样的现成方案（难度 2–3，CPU/METAL 上也能跑，只是 ML 部分弱化）。TAAU（Falcor BSD-3）是更简单的参考。DLSS/XeSS 均为专有二进制 SDK，仅 NVIDIA/Intel 平台可用，且集成进开源 Cycles 有许可与维护问题（难度 4–5）；若走这条线，只能做成可选插件式集成。
- lamp 依赖：上采样为后处理，不依赖 lamp。

---

## 5. 辐射缓存（radiance caching）

> 标题澄清：用户所述 "SIGGRAPH 2024 *Radiance Caching for Real-Time Global Illumination*" 未检索到同名论文。最接近的三项：①SIGGRAPH 2021 **Real-time Neural Radiance Caching for Path Tracing**（NRC，常被中文资料简称为 "Radiance Caching for real-time GI"）；②HPG 2024 **Radiance Caching with On-Surface Caches for Real-Time Global Illumination**（HPG 2024，TU Graz）；③RTXGI 2.0（2024）的 **Spatial Hash Radiance Cache（SHARC）**。以下三者都列出。

| 条目 | 年份/会议 | 代码 | 语言/框架 | 许可证 | 难度 | Cycles 备注 |
|---|---|---|---|---|---|---|
| NRC（Real-time Neural Radiance Caching for Path Tracing）| SIGGRAPH 2021（TOG 40(4)，DOI 10.1145/3450626.3459812）| 官方=RTXGI v2.0 NRC 模块 / NVIDIA-RTX/NRC；第三方=AdamYuan/VkNRC（Vulkan）、cuteday/nrc | CUDA/OptiX（官方）；Vulkan（第三方）| **专有（RTX SDK LICENSE）**；第三方未注明 | 5 | 多分辨率 hash 编码 + MLP 推理进 kernel，CPU/METAL 不可行 |
| OSCGI（Radiance Caching with On-Surface Caches for Real-Time GI）| **HPG 2024**（PACMCGIT 7(3)，DOI 10.1145/3675382，TU Graz：Tatzgern/Weinrauch/Stadlbauer）| **无官方代码**（HPG 幻灯片 + 第三方复现：AUEB 硕士论文实现等）| 论文演示基于 D3D12 | — | 4 | 表面缓存（on-surface caches）算法本身可移植 |
| SHARC（Spatially Hashed Radiance Cache）| 2024（RTXGI 2.0）| **NVIDIA-RTX/SHARC**（纯 shader 库，接入 RTXGI 使用）| HLSL shader-only | **仓库无 LICENSE 文件**；按 RTXGI 生态应为 RTX SDK 许可（专有）| 4 | 需要与 path tracer 深度集成 + 大量超参 |
| Radiance Cascades（2D/新兴）| 2023–2025 社区热点 | bevy_radiance_cascades、n01r1r/radiance-cascades-urp、Yaazarai/RadianceCascades 等 | Rust/WGPU、HLSL/URP | MIT 等（社区）| 3 | 2D 起家，3D 扩展质量/可扩展性存疑 |
| Real-time all-frequency GI with radiance caching | 2023（Computational Visual Media）| 无公开代码 | — | — | 3–4 | 全频段辐射缓存 |

### 5.1 详解与 Cycles 备注
- **NRC**（Müller, Rousselle, Novák, Keller，SIGGRAPH 2021）：用多分辨率 hash grid + 小型 MLP 在命中点回归辐亮度，实现 1spp 路径追踪下的实时 GI；2024 年随 RTXGI 2.0 开源（NVIDIA-RTX/RTXGI 含 NRC 模块，另有独立 NVIDIA-RTX/NRC 仓库），但均为**专有 RTX SDK 许可证**。核心难点：NN 推理需要 tiny-cuda-nn 风格的 hash encoding + 训练环，**在 Cycles 的 CPU/METAL 独立构建上基本不可行**（难度 5）。第三方 Vulkan 实现（AdamYuan/VkNRC）可作算法参考。
- **OSCGI（HPG 2024）**：在物体表面稀疏缓存点存辐亮度（配合层次结构），避免 NRC 的神经网络开销，纯图形算法、**理论上可移植进 Cycles kernel**（难度 4），但**官方未发布代码**，需要自行实现 + 调参；依赖 lamp 直接光。
- **SHARC**：纯 HLSL shader 库（world-space hash 缓存），无 LICENSE 文件（生态内为专有许可）；集成需要改 path tracer 的 radiance 查询路径，难度 4。
- **Radiance Cascades**：2023 年起社区热点（2D 起家，3D 扩展有质量上限争议），代码多、许可宽松（MIT 等），适合做原型验证（难度 3），是否值得进 Cycles 主渲染管线待评估。
- 结论：辐射缓存方向**算法价值高**（低采样下收敛快），但官方实现许可证/后端不友好；Cycles 若要做，推荐参考 OSCGI 与 Radiance Cascades 的纯图形方案（避开 NRC 的 NN 依赖），或仅在带 CUDA 的构建上评估 NRC 移植。

---

## 6. 实时 GI / 实时路径追踪

| 条目 | 年份/会议 | 代码 | 语言/框架 | 许可证 | 难度 | lamp 依赖 |
|---|---|---|---|---|---|---|
| DDGI（RTXGI）| JCGT 8(2) 2019 | NVIDIA-RTX/RTXGI | HLSL/D3D12 | 专有 | 4 | 是 |
| ReSTIR GI | SIGGRAPH 2021 | RTXDI v2.0 | HLSL | 专有 | 4 | 是 |
| NRC | SIGGRAPH 2021 | RTXGI 2.0 | CUDA/OptiX | 专有 | 5 | 是 |
| RTXGI 2.0（DDGI + NRC + SHARC）| 2024 | NVIDIA-RTX/RTXGI + SHARC | HLSL/D3D12 | 专有 | 4–5 | 是 |
| Lumen（UE5 实时 GI）| SIGGRAPH 2022 课程（Karis 等）| 无公开代码（UE 源码授权）| UE（C++/HLSL）| 专有（UE EULA）| 5 | 是 |
| Research Advances Toward Real-Time Path Tracing（"Stochastic All the Way"）| GDC 2022 演讲（Clarberg）| 无代码 | — | — | N/A（理念）| 是 |
| Real-Time Path Tracing and Beyond（keynote）| SIGGRAPH 2022（Clarberg）| 无代码 | — | — | N/A（理念）| 是 |
| ReSTIR PT / GRIS | 2022–2023 | DQLin/ReSTIR_PT | Falcor 4.4/DX12/OptiX | BSD-3-Clause | 4 | 是 |
| Cyberpunk 2077 Overdrive（全路径追踪模式）| 2023（GDC/Advances in RT 报告）| 无代码 | CDPR 引擎 | 专有 | N/A | 是 |
| Neural Two-Level Monte Carlo Real-Time Rendering | EG 2025（CGF 44，项目页 mishok43.github.io/nirc）| 无公开代码 | — | — | 5 | 是 |
| Real-Time Stochastic Lightcuts | I3D 2020（PACMCGIT，Lin & Yuksel）| **DQLin/RealTimeStochasticLightcuts** | MiniEngine/DX12 | **MIT** | 3–4 | 是（多光源/灯组）|
| Volumetric ReSTIR | SIGGRAPH Asia 2021 | DQLin/VolumetricReSTIRRelease | Falcor/DX12 | BSD-3-Clause | 4 | 是 |
| Neural Importance Sampling of Many Lights | 2025 | 无官方代码 | — | — | 5 | 是 |

### 6.1 详解与 Cycles 备注
- **"Stochastic all the way"**（Clarberg，GDC 2022 *Research Advances Toward Real-Time Path Tracing*）：NVIDIA 实时路径追踪的核心理念——对光源、材质、GI 全部做随机化求值，配合低采样 + 去噪/重建，而不是求高精度直接光。这是把 Cycles 交互模式推向"1spp 路径追踪 + 激进去噪"的方法论依据（非论文，无代码）。
- **ReSTIR PT**：2023 年游戏行业标杆（Cyberpunk 2077 Overdrive 用 ReSTIR PT + DLSS 3.5 Ray Reconstruction 实现全路径追踪）；官方代码 BSD-3-Clause（DQLin/ReSTIR_PT，Falcor 4.4）。对 Cycles 实时模式是最相关的"完整路径追踪 + 采样复用"方案，但实现工作量与调参成本高（难度 4）。
- **Lumen**（UE5，SIGGRAPH 2022 课程）：混合 SDF/屏幕空间追踪 + 表面缓存 + 探针的实时 GI 工程方案；无公开代码，仅可参考其架构思路（难度 5）。
- **Neural Two-Level MC**（EG 2025）：两级蒙特卡洛（神经光源采样 + 神经辐射缓存）的实时渲染方案，无代码 + NN 依赖（难度 5）。
- **实时光子映射**：2019–2026 没有出现有代码的主流 SIGGRAPH 实时光子映射论文（多为 2004–2015 时代工作）；Cycles 曾内置 SPPM 现已移除。实时"光子/多光源"需求实际由 ReSTIR PT、Stochastic Lightcuts、NRC/SHARC 覆盖。**Stochastic Lightcuts（I3D 2020，MIT，MiniEngine/DX12）**是"实时 many-lights + 焦散"最轻量的可用代码，值得参考。
- lamp 依赖：本节全部技术都依赖 lamp（直接光采样/发射/多光源）。独立构建 lamp bug 修复前，任何实时 GI 结果都会缺直接光成分。

---

## 7. GPU 光线追踪加速 / 高效可见性（OptiX/DXR 层面）

| 条目 | 年份/会议 | 代码 | 语言/框架 | 许可证 | 难度/备注 |
|---|---|---|---|---|---|
| OptiX 7（OptiX: A General Purpose Ray Tracing Engine，Parker 等）| SIGGRAPH 2019（TOG 38(4)）| OptiX SDK | CUDA/C++/OptiX | 专有（SDK）| **Cycles 已集成**（OptiX 后端），N/A |
| DirectX Raytracing（DXR）| 2018（API 基线）| Microsoft DXR 规范 | HLSL/DX12 | 规范 | N/A（API 层）|
| Vulkan Ray Tracing（VKRay）| 2020（Khronos 扩展）| Vulkan 规范 | GLSL/HLSL/Vulkan | 规范 | N/A（API 层）|
| Embree 4 | 2022 | **RenderKit/embree** | C++/ISPC，CPU | **Apache-2.0** | **Cycles CPU 后端已用**，N/A |
| A Survey on Bounding Volume Hierarchies for Ray Tracing | CGF 40(8) 2021（EGSR STAR，Meister 等）| 无（综述）| — | — | 参考价值 3（Cycles 自研 BVH 可对照）|
| Compact Tetrahedralization-based Acceleration Structures for Ray Tracing | arXiv 2021 / JVIS 2022（Demirci/Aman/Güdükbay）| 无公开代码 | — | — | 3–4（替代 BVH 的加速结构，冷门）|
| Opacity Micro-Maps（OMM）| 2023（NVIDIA RTX 特性）| 无论文（技术文档/驱动层）| 硬件/API | 专有 | 4（alpha 测试几何的可见性加速）|
| Ray Tracing Gems（教程书）| 2019 | 免费电子书 | 多语言 | 免费 | 参考 |

### 7.1 详解与 Cycles 备注
- Cycles 的可见性层已经是"三方并立"：CPU=Embree（Apache-2.0，已集成）、NVIDIA GPU=OptiX（已集成）、Apple=Metal RT（已集成），另有自研 BVH（`src/bvh`，供非 Embree/OptiX 路径与一致性管理）。因此 **OptiX/DXR/VKRay 层面的研究对 Cycles 主要是"理解硬件特性"而非"移植代码"**。
- 2019–2026 加速结构研究偏"综述/替代结构"（BVH survey、四面体化结构），没有出现取代 BVH 的主流突破；对 Cycles 自研 BVH 的工程优化（compaction、traversal 调度）可从 EGSR 2021 STAR 综述对照。
- OMM（Opacity Micro-Maps）类硬件特性（alpha 测试加速）依赖 NVIDIA 硬件/驱动，Cycles kernel 内做等价优化成本高（难度 4）。

---

## 8. 汇总表（按 Cycles 实时模式可落地性排序）

| 优先级 | 技术 | 许可证 | 官方代码 | 难度 | 落地路径 |
|---|---|---|---|---|---|
| P0 | OIDN v3.0（时域去噪）| Apache-2.0 | 有（RenderKit/oidn）| 1 | 已是 Cycles 依赖，升级 + 接时域模型即可 |
| P0 | OptiX Denoiser（时域）| 专有 | SDK 内 | 1 | 已集成；仅 NVIDIA 硬件 |
| P1 | SVGF / A-SVGF 类时空滤波 | 开源 | 社区实现 | 2 | 写进 kernel（用现有 aux/motion passes）|
| P1 | BMFR | MIT/BSD-3 | 有 | 3 | kernel 移植，特征回归 + 时域重投影 |
| P1 | TAAU / FSR 2（时域上采样）| BSD-3/MIT | 有（FSR）| 2–3 | 后处理接入（motion+depth 已有）|
| P2 | ReSTIR DI / GI（算法移植）| 官方专有，参考实现 | RTXDI（参考）/DQLin（BSD-3）| 3–4 | 用 DQLin/BSD-3 源码做参考，重写进 kernel；依赖 lamp |
| P2 | ReSTIR PT / GRIS | BSD-3（DQLin）| 有 | 4 | 工作量大；需要 denoiser + motion 配合 |
| P2 | Stochastic Lightcuts | MIT | 有 | 3–4 | 多光源实时采样，参考 MiniEngine 实现 |
| P2 | OSCGI / Radiance Cascades（纯图形辐射缓存）| 无官方代码 / MIT 社区 | 部分 | 3–4 | 免 NN；从社区实现起步做原型 |
| P3 | RTXGI 2.0（DDGI/NRC/SHARC）| 专有 | 有（RTXGI/SHARC）| 4–5 | 许可不兼容 + HLSL/CUDA 重写 + NN（NRC）|
| P3 | NRD | 专有 | 有 | 4 | 许可不兼容 + HLSL 重写 |
| P3 | 神经网络去噪（KPCN/affinity/Deep Comp/WSKPD）| 各异（WSKPD MIT）| 部分 | 4–5 | CPU/METAL 独立构建基本不可行 |
| P3 | DLSS / XeSS / ExtraSS / PatchEX | 专有/无代码 | SDK（闭源）| 4–5 | 仅平台厂商硬件，插件式集成 |
| P3 | NRC / Neural Two-Level MC / NIS-ML | 专有/无代码 | 无 | 5 | NN 运行时 + 训练管线，当前构建不可行 |

---

## 9. 对 Cycles 实时模式（interactive viewport、低采样、时域复用、激进去噪）的建议

1. **先去噪、再采样复用**：P0 是把已集成的 OIDN 升级到 v3.0 时域模型（配合现有 motion/backward_motion/specular_motion passes），这是"激进去噪 + 时域稳定"成本最低的路径；有 NVIDIA 硬件时 OptiX 时域去噪并行可用。
2. **时域累积层**：P1 在 kernel 内加 reprojection + history 管理的时域累积（参考 EG 2020 TAA 综述的 history 有效性/clamping），再叠 SVGF 类方差引导滤波（难度 2）。这是后续一切时空复用（ReSTIR、BMFR）的地基。
3. **上采样**：P1 接入 FSR 2/3（MIT，纯着色器）或自研 TAAU 式方案，viewport 低分辨率渲染 + 时域上采样；Cycles 已有 motion/depth，工作量集中在后处理管线上。
4. **采样复用进阶**：P2 以 DQLin/ReSTIR_PT（BSD-3-Clause）与 ReSTIR 课程为参考，把 ReSTIR DI → GI → PT 渐进移植进可移植 kernel；**注意 lamp bug 必须先修或限定 emissive-mesh 场景**，否则 ReSTIR 的 direct 项与所有依赖 lamp 的实时 GI 都会出错。
5. **辐射缓存**：P2 评估 OSCGI/Radiance Cascades 这类**免神经网络**的缓存方案；NRC/SHARC 因专有许可 + NN 依赖建议暂缓（除非未来有 CUDA 构建目标）。
6. **明确不碰**：NRD、RTXGI 全家桶、DLSS/XeSS 因**专有许可证**（NVIDIA RTX SDKs LICENSE / Nvidia Source Code License-NC）与 Cycles 的 Apache-2.0 开源许可不兼容，不能直接集成源码；只能作为"外部对照实现/参考论文"。
7. **kernel 书写约束**：所有移植必须走 Cycles 可移植 kernel 子集（`src/kernel` + `src/kernel/device` 包装），不能引入 CUDA-only/HLSL 代码；CPU/METAL 后端必须可编译可跑。

---

## 10. 附录

### 10.1 "AHF" 缩写澄清
调研中未检索到实时渲染领域名为 "AHF" 的公认论文/代码。最接近的候选：
- **A-SVGF**（Adaptive SVGF，Pine 渲染器 a_svgf，2021–2023）；
- **Interactive MC Denoising using Affinity of Neural Features**（SIGGRAPH 2021，第三方实现 AlstonXiao/InteractiveDenoiserWithAffinity，仓库无 LICENSE 文件）——"affinity" 与 "AHF" 可能被混淆；
- **Ray Histogram Fusion**（Boosting Monte Carlo Rendering by Ray Histogram Fusion，2022，港城大/岭南团队，无公开代码）；
- Edge-aware denoising for mobile ray tracing（2025，期刊）。
若用户所指是其中某项，请以实际名称为准。

### 10.2 许可证速查（已核实）
- Apache-2.0：OIDN、Embree、Cycles 本体
- BSD-3-Clause：Falcor、DQLin/ReSTIR_PT、DQLin/VolumetricReSTIRRelease、Pxy951/ReSTIR_PT、gtong-nv/BMFR-DXR-Denoiser
- MIT：hchoi405/bmfr、FSR 2/3/4（GPUOpen）、DQLin/RealTimeStochasticLightcuts、Rendering-at-ZJU/WSKPD、FidelityFX-FSR
- 专有（NVIDIA RTX SDKs LICENSE）：RTXDI、RTXGI、NRC、SHARC、NRD、OptiX、DLSS
- 非商用（Nvidia Source Code License-NC）：NVlabs/conditional-restir-prototype
- 闭源二进制 SDK：XeSS（intel/xess）、DLSS（NVIDIA/DLSS）
- 未注明/无 LICENSE 文件：AlstonXiao/InteractiveDenoiserWithAffinity、SHARC（仓库内无）、tatran5 ReSTIR、RealTimeStochasticLightcuts 之外的第三方实现

### 10.3 关键参考链接
- ReSTIR 2020：https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/
- RTXDI：https://github.com/NVIDIA-RTX/RTXDI · RTXGI：https://github.com/NVIDIA-RTX/RTXGI · SHARC：https://github.com/NVIDIA-RTX/SHARC
- ReSTIR PT/GRIS 官方源码：https://github.com/DQLin/ReSTIR_PT · Volumetric ReSTIR：https://github.com/DQLin/VolumetricReSTIRRelease
- CRIS：https://github.com/NVlabs/conditional-restir-prototype
- ReSTIR 2023 课程：https://intro-to-restir.cwyman.org/
- NRC 论文：arXiv:2106.12372 · OSCGI（HPG 2024）：https://highperformancegraphics.org/slides24/hpg24_oscgi.pdf（DOI 10.1145/3675382）
- OIDN：https://github.com/RenderKit/oidn（v3.0 时域去噪：https://www.cgchannel.com/2026/01/open-image-denoise-3-will-support-temporal-denoising/）
- NRD：https://github.com/NVIDIA-RTX/NRD
- FSR：https://github.com/GPUOpen-Effects/FidelityFX-FSR · XeSS：https://github.com/intel/xess
- ExtraSS：https://zheng95z.github.io/publications/extrass23 · PatchEX：arXiv:2407.17501
- DDGI：https://jcgt.org/published/0008/02/01/ · TAA 综述：https://diglib.eg.org/items/cfa6a05c-53b3-4ee1-979d-48abc6225f6b/full
- Clarberg GDC 2022（Stochastic all the way）：https://research.nvidia.com/labs/rtr/publication/clarberg2022gdc/
- Neural Two-Level MC：https://mishok43.github.io/nirc/（arXiv:2412.04634）

*调研日期：2026 年（所有 URL 与许可证在撰写时经在线访问核实；个别 2025–2026 新论文的会议信息以仓库/页面自标注为准，已在文中注明）。*
