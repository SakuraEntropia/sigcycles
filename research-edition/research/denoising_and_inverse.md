# 现代渲染研究 → Cycles 集成前期调研：去噪/重建 · 可微/逆向渲染 · 神经材质（2019–2026）

> 调研员：渲染研究调研员（subagent）｜日期：2025-08
> 方法：`web_search` 多轮交叉检索（SIGGRAPH 2019–2026 / HPG / EGSR / CVPR / ICCV / AAAI / 3DV），并用 GitHub API / raw.githubusercontent.com **逐一核实**代码仓库存在性与许可证（许可证以仓库内 LICENSE 文件为准，未发现许可证文件者标注「未声明」，默认保留所有权利）。
> 标注：★=官方仓库已验证；◐=社区/复现实现；—=无公开代码；⚠=许可证未声明或存疑。
> 目标仓库：`/Users/faputa/Documents/Entro-Cycles/cycles`（Blender Cycles 源码树）。

---

## 0. Cycles 现状速览（已从源码确认，作为兼容性评估基线）

| 组件 | 位置（本仓库内） | 说明 |
|---|---|---|
| 去噪抽象层 | `src/integrator/denoiser.h`、`denoiser.cpp` | `Denoiser` 抽象基类 + `Denoiser::create()` 工厂；`device/denoise.h` 定义 `DenoiseParams` |
| OIDN 后端 | `src/integrator/denoiser_oidn.cpp`、`denoiser_oidn_base.cpp`、`denoiser_oidn_gpu.cpp` | CPU + GPU 版（OIDN 设备：Metal/CUDA 等） |
| OptiX 后端 | `src/integrator/denoiser_optix.cpp` | NVIDIA OptiX AI 去噪器 |
| GPU 公共逻辑 | `src/integrator/denoiser_gpu.cpp` | OptiX/OIDN-GPU 的设备管理与 interop |
| 去噪特征 pass | `src/kernel/film/denoising_passes.h` | 逐样本/逐闭合元写 **depth / normal / albedo / specular_albedo / roughness / flow**（含 `DENOISING_PASS_USE_ALBEDO_ROUGHNESS_WEIGHTING` 选项）；另有 `volume_guiding_denoise.h`（体积引导去噪） |
| pass/film 架构 | `src/kernel/film/*.h`（`adaptive_sampling.h`、`aov_passes.h`、`light_passes.h`、`cryptomatte_passes.h`）、`src/integrator/pass_accessor_{cpu,gpu}.cpp` | 分层 buffer（`RenderBuffers`）+ pass 读写；自适应采样维护**逐像素方差** |
| 时域积累 | `src/integrator/path_trace_work*.cpp` + 视口显示 | 视口连续渲染做样本累加；`flow` pass 为时域去噪/稳定预留 |

**结论**：Cycles 的去噪输入面（特征 pass + 方差 + buffer 管理）已经相当完整，新增学习式去噪器的主要工作量在**推理后端**（ONNX Runtime / TensorRT / OpenVINO / 自研 CUDA kernel），而非特征管线。

---

## 1. 主题一：神经去噪 / 学习式重建（Neural Denoising / Learned Reconstruction）

### 1.1 论文/系统清单

| # | 论文 / 系统 | 年份 | 会议 | 代码 | 语言/框架 | 许可证 | 集成难度 | Cycles 兼容性备注 |
|---|---|---|---|---|---|---|---|---|
| 1 | **RDFC: Recurrent Denoising for Monte Carlo Rendering** | 2019 | SIGGRAPH (TOG 38(6)) | —（官方未发布） | PyTorch | n/a | 4 | 递归时域网络：需历史 buffer + motion/flow 输入；Cycles 已有 flow pass；做「合成器后处理去噪节点」比进内核更现实 |
| 2 | **Real-time Monte Carlo Denoising with the Neural Bilateral Grid** | 2020 | EGSR (CGF 39(2)) | ★ `xmeng525/RealTimeDenoisingNeuralBilateralGrid` | Python + TensorFlow | MIT | 4 | 多尺度双侧网格 + albedo 调制；特征（albedo/normal）Cycles 已产出；推理端可走 ONNX |
| 3 | **BMFR: Blockwise Multi-Order Feature Regression for Real-Time Path-Tracing Reconstruction** | 2023 | SIGGRAPH (TOG 42(4)) | ★ `hchoi405/bmfr` | C++ + OpenCL + OpenImageIO | MIT | 4 | 多阶特征回归（OIDN 同源思路）；依赖输入特征 pass；可直接对照 Cycles 的 denoising passes 设计 |
| 4 | **A-SVGF: Adaptive Spatiotemporal Variance-Guided Filtering**（论文即 *Gradient Estimation for Real-Time Adaptive Temporal Filtering*） | 2018 | HPG / PACMCGIT | ◐ KIT 附带代码（`gitea.com/wicstas/pine` 的 `a_svgf` 模块） | C++ + GLSL | ⚠未声明（研究代码） | 3 | 非学习：方差引导 + 梯度采样自适应时域滤波；需要 variance buffer（Cycles 自适应采样已维护）与 motion；适合视口实时去噪路径 |
| 5 | **OIDN: Intel Open Image Denoise**（现状） | 2018–2025（当前 v2.5.1） | 工业库 | ★ `RenderKit/oidn` | C/C++（ISPC/oneDNN；SYCL/CUDA/HIP/Metal 设备后端） | **Apache-2.0** | 已集成 | Cycles 已内置（CPU 版）；v2.4+/v2.5 加入 Intel Xe/XMX、NVIDIA CUDA、AMD HIP、Apple Metal GPU 支持，Blender 4.5+ 已启用 OIDN GPU 去噪；无时域模式 |
| 6 | **OptiX AI Denoiser**（现状） | 2018（OptiX 5.1）– 当前 OptiX 8.x | 工业库 | SDK 下载（NVIDIA 开发者站，无 GitHub 源码） | CUDA/C++ | **专有 SDK 许可**（免费商用、禁止再分发 SDK 本体） | 已集成 | Cycles 已内置（`denoiser_optix.cpp`）；预训练模型，支持 albedo/normal 特征与可选时域模式 |
| 7 | **NRD: NVIDIA Real-Time Denoisers**（ReBLUR/SIGMA/Reflections） | 2019–2025 | 工业库 | ★ `NVIDIA-RTX/NRD`（原 NVIDIAGameWorks/RayTracingDenoiser） | C++ + HLSL | **NVIDIA RTX SDKs License**（自定义，免费商用） | 3 | 实时去噪器族（含时域 ReBLUR）；为 raster/RT 管线设计，接入 Cycles 需映射特征 pass 与 motion 语义 |
| 8 | **Denoising Monte Carlo Renders with Diffusion Models** | 2025 | 3DV（NeurIPS 2024 有相关工作） | ★ `vibe007/denoising-monte-carlo-renders-with-diffusion-models` | Python + PyTorch | ⚠未声明 | 5 | 扩散模型去噪：质量好且对重尾噪声（高光/折射）稳健；推理慢，只能作离线后处理；可条件化 albedo/normal 等（与 Cycles 特征一致） |
| 9 | **A Statistical Approach to Monte Carlo Denoising** | 2024 | SIGGRAPH Asia | ★ `cg-tuwien/StatMC`（基于 pbrt-v3） | C++ + CUDA（OpenCV/tev 生态） | ⚠未声明 | 4 | 非学习：在路径追踪中显式跟踪统计量（ACRR 近似贡献俄罗斯轮盘、SMIS 选择性 MIS），做统计一致去噪；与 Cycles 的 integrator 深度耦合，移植的是**思路**而非代码 |
| 10 | **Statistical Error Reduction for Monte Carlo Rendering** | 2025 | SIGGRAPH Asia | ★ `cg-tuwien/StatER-opencv_contrib`（CUDA 去噪器） | C++ + CUDA | ⚠未声明 | 4 | StatMC 的后续：更直接的统计误差削减；同样属 integrator 侧改造思路 |
| 11 | 基线：**KPCN**（SIGGRAPH 2017）/ **RDAE**（SIGGRAPH 2017） | 2017 | SIGGRAPH | ◐（社区实现，如 `yuyingyeh/rdae`） | PyTorch/TensorFlow | 视实现而定 | 4 | 范围外但作为对比基线；KPCN 思想即 OptiX/OIDN 的前身 |

**主题小结**：2019–2026 的学术去噪呈三条线：① 工业级预训练 CNN（OIDN/OptiX/NRD，已入 Cycles）；② 非学习统计/方差方法（SVGF→A-SVGF→StatMC/StatER，适合嵌入 integrator）；③ 生成式/扩散重建（2024–2025 起，离线）。Cycles 集成优先级建议：**①已具备；②最贴合现有 pass 架构；③作独立后处理管线**。

---

## 2. 主题二：时域/方差引导滤波（SVGF / Temporal Accumulation / Variance-Guided Filtering）

| # | 论文 / 系统 | 年份 | 会议 | 代码 | 语言/框架 | 许可证 | 集成难度 | Cycles 兼容性备注 |
|---|---|---|---|---|---|---|---|---|
| 1 | **SVGF: Spatiotemporal Variance-Guided Filtering**（基线） | 2017 | HPG | ◐ 社区实现：`jacquespillet/SVGF`（HLSL/DX）、`TheVaffel/spatiotemporal-variance-guided-filtering`（GLSL）；官方代码位于 NVIDIA Falcor 中 | HLSL/GLSL | 社区实现 MIT 不等 | 3 | 实时参考基准：空间 atrous + 时域指数积累 + 方差引导；需要逐像素方差（Cycles 自适应采样已有类似 buffer）、depth/normal/motion |
| 2 | **A-SVGF / Gradient Estimation for Real-Time Adaptive Temporal Filtering** | 2018 | HPG | ◐ `gitea.com/wicstas/pine`（a_svgf 模块，KIT） | C++/GLSL | ⚠未声明 | 3 | 对 SVGF 的时域部分做自适应（梯度样本前向投影、自适应积累权重），显著降低拖影/ghosting；时域语义与 Cycles 视口积累互补 |
| 3 | **Neural Temporal Denoising for Indirect Illumination**（附注） | 2022 | IEEE TVCG | — | PyTorch | n/a | 4 | 神经时域去噪间接光；思路可迁移到 Cycles 的 indirect 分桶 pass |
| 4 | 时域积累现状（Cycles 视角） | — | — | — | — | — | — | Cycles 视口 = 多帧样本累加（无滤波）；**真正的时间滤波**（motion-vector 重投影 + 指数积累）尚未内置；`flow` pass 已就绪，是接入 SVGF/A-SVGF/时域神经去噪的前提 |

**主题小结**：SVGF 家族是「实时 + 低成本 + 非学习」的最优折中，且与 Cycles 现有 buffer 语义（depth/normal/flow/方差）几乎一一对应，是**中期内性价比最高的内核内集成项**（可先做视口实时模式）。

---

## 3. 主题三：可微渲染（Differentiable Rendering）

| # | 论文 / 系统 | 年份 | 会议 | 代码 | 语言/框架 | 许可证 | 集成难度 | Cycles 兼容性备注 |
|---|---|---|---|---|---|---|---|---|
| 1 | **OpenDR**（基线，近似可微光栅化） | 2014 | ECCV | ★ `mattloper/opendr` | MATLAB + chumpy | MIT | 5 | 历史基线；无物理意义梯度；仅作演进参照 |
| 2 | **redner: Differentiable Monte Carlo Ray Tracing through Edge Sampling** | 2019 | SIGGRAPH (TOG 38(6)) | ★ `eamonious/redner` | Python + C++/CUDA（PyTorch 绑定） | **MIT** | 5 | 边采样 + AD；面向小型场景优化；与 Cycles 是不同架构，只能作外部优化器 |
| 3 | **Mitsuba 2: A Retargetable Forward and Inverse Renderer** | 2019 | SIGGRAPH (TOG 38(6)) | ★ `mitsuba-renderer/mitsuba2` | C++17 + LLVM JIT（Enoki） | **BSD-3-Clause** | 5 | 无偏可微路径追踪范本；其「可重定向」设计（variant 系统）是 Cycles 若做可微化的主要参照 |
| 4 | **Mitsuba 3** | 2022 | SIGGRAPH Asia (TOG 41(6)) | ★ `mitsuba-renderer/mitsuba3` | Python（Dr.Jit 动态 JIT） | **BSD-3-Clause** | 5 | 目前**事实标准的可微渲染器**；生态（nvdiffrec、PracticalInv、NeRF 系）均基于它；作为 Cycles 的外部可微前端最现实 |
| 5 | **Dr.Jit: A Just-In-Time Compiler for Differentiable Rendering** | 2022 | SIGGRAPH Asia (TOG 41(6)) | ★ `mitsuba-renderer/drjit` | C++（LLVM JIT，CUDA/CPU） | **BSD-3-Clause** | 5 | Mitsuba 3 的数值后端；「Python 写、JIT 编译、反向 AD」范式；若 Cycles 未来走可微路线，Dr.Jit 式设计是唯一成熟参照 |
| 6 | **PSDR: Path-Space Differentiable Rendering** | 2020 | SIGGRAPH (TOG 39(4)) | ★ `uci-rendering/psdr-cuda`（官方组织；另有 fork `Ginko2501/psdr-cuda`） | C++/CUDA | **BSD-3-Clause** | 5 | 首次给出路径空间无偏导数估计（含边界/遮挡项）；是 Mitsuba 2 可微性的理论补全 |
| 7 | **psdr-jit: Unbiased Physically Based Differentiable Renderer** | 2024 | 技术报告/软件发布（UCI） | ★ `andyyankai/psdr-jit` | Python + OptiX 7 + Dr.Jit | ⚠未声明（仓库无 LICENSE 文件） | 5 | PSDR 的 Dr.Jit 重写，API 更友好；同属「外部可微渲染器」阵营 |
| 8 | **Nvdiffrast: Forward and Inverse Differentiable Rendering** | 2020 | SIGGRAPH Asia (TOG 39(6)) | ★ `NVlabs/nvdiffrast` | Python + CUDA | **NVIDIA Source Code License (1-Way Commercial)** | 4 | 光栅化可微管线（渲染器侧），逆向任务事实标准；与路径追踪无关，但可作「可微预览」 |
| 9 | **Nvdiffrec: Extracting Triangular 3D Models, Materials, and Lighting from Images** | 2023 | SIGGRAPH (TOG 42(6)) | ★ `NVlabs/nvdiffrec` | Python + PyTorch（基于 nvdiffrast + Mitsuba 2） | **NVIDIA Source Code License** | 4 | 从图像优化网格+SVBRDF+环境光；**输出即 Cycles 可导入资产**（glTF/PBR 贴图），是「可微渲染→Cycles」的最佳桥梁案例 |
| 10 | **3D Gaussian Splatting**（邻接） | 2023 | SIGGRAPH (TOG 42(4)) | ★ `graphdeco-inria/gaussian-splatting` | Python + CUDA | **Gaussian-Splatting License（非商用）** | 4 | 可微光栅化的 3D 表示；与 Cycles 无直接关系，但其逆向渲染变体（见主题四）会输出材质/光照 |
| 11 | 综述：*A Brief Review on Differentiable Rendering*（Electronics 2024）等 | 2021–2025 | 期刊/综述 | — | — | — | — | 进展脉络：无偏路径空间导数（PSDR）→ JIT 编译生态（Dr.Jit/Mitsuba 3）→ 光栅化可微（nvdiffrast/3DGS）→ 与生成模型结合（2024–2025） |

**主题小结**：可微渲染**不可能低成本内嵌 Cycles**（无 AD、无 JIT 编译设施）。正确姿势是「外部可微渲染器（Mitsuba 3 / psdr-jit / redner）做优化 → 输出 PBR 资产 → 导入 Cycles」；nvdiffrec/PracticalInv 已示范该闭环。若 Entro-Cycles 有自研可微内核诉求，Mitsuba 2 的 variant 架构与 Dr.Jit 是必读范本。

---

## 4. 主题四：逆向渲染 / 逆材质 / 逆光照（Inverse Rendering / Inverse Material & Lighting，2019–2026）

| # | 论文 | 年份 | 会议 | 代码 | 语言/框架 | 许可证 | 集成难度 | Cycles 兼容性备注 |
|---|---|---|---|---|---|---|---|---|
| 1 | **Deep Inverse Rendering for High-resolution SVBRDF Estimation from an Arbitrary Number of Images** | 2019 | SIGGRAPH (TOG 38(3)) | ★ `msraig/DeepInverseRendering` | Python 3.6 + TensorFlow 1.x | ⚠未声明 | 4 | 多图 SVBRDF 估计（Duan Gao, Xiao Li, Yue Dong, Pieter Peers, Kun Xu, Xin Tong）；输出 PBR 贴图，可直接进 Cycles 材质节点 |
| 2 | **NeRFactor: Neural Factorization of Shape and Reflectance Under an Unknown Illumination** | 2021 | **SIGGRAPH Asia** (TOG 40(6)) | ★ `google/nerfactor` | Python + TensorFlow | **Apache-2.0** | 5 | NeRF 解耦几何+BRDF+光照（Xiuming Zhang 等）；输出反照率/法线/粗糙度/光照（SH），与 Cycles PBR 语义接近但需重烘焙 |
| 3 | **PhySG: Inverse Rendering with Spherical Gaussians** | 2021 | **CVPR** | ★ `Kai-46/PhySG` | Python + PyTorch（依赖 Mitsuba 2） | **MIT** | 5 | 球面高斯光照 + 微表面 BRDF；端到端可微；输出可重打光场景 |
| 4 | **InvRender: Inverse Rendering with Interactive Global Illumination** | 2022 | CVPR | ★ `zju3dv/InvRender` | Python + PyTorch | **Apache-2.0** | 5 | NeRF 框架内做可微光追（环境光 + 材质分解）；输出 8K 级材质 |
| 5 | **TensoIR: Tensorial Inverse Rendering** | 2023 | CVPR | ★ `Haian-Jin/TensoIR` | Python + PyTorch | **MIT** | 5 | 张量分解 + 显式可见性光线；分解几何/材质/光照最干净的 NeRF 系方法之一 |
| 6 | **GS-IR: 3D Gaussian Splatting for Inverse Rendering** | 2024 | CVPR | ★ `lzhnb/GS-IR` | Python + PyTorch（3DGS 基） | **MIT** | 5 | 3DGS 上的逆渲染；速度快，材质/光照分解质量随 3DGS 前进 |
| 7 | **Relightable 3D Gaussian** | 2024 | CVPR | ★ `NJU-3DV/Relightable3DGaussian` | Python + PyTorch | **Gaussian-Splatting License（非商用）** | 5 | 3DGS 可重打光；注意非商用限制 |
| 8 | **Practical Inverse Rendering of Textured and Translucent Appearance** | 2025 | SIGGRAPH | ★ `google/practical-inverse-rendering-of-textured-and-translucent-appearance` | Python，**基于 Mitsuba 3 + Dr.Jit** | **Apache-2.0** | 4 | 2025 年代表作（Weier 等）：真实纹理 + 半透明外观的实用逆渲染；Mitsuba 3 生态使其最易复用到「Cycles 资产导入」流程 |
| 9 | **DiffusionRenderer: Neural Inverse and Forward Rendering with Video Diffusion Models** | 2025 | CVPR（Oral） | ★ `nv-tlabs/diffusion-renderer` | Python + PyTorch | **NVIDIA Source Code License** | 5 | 扩散模型做逆/正渲染（材质+光照+视频）；代表 2024–2026「生成式逆向渲染」方向 |
| 10 | 逆光照专项：环境光估计（Gardner 等 ICCV 2019 系） | 2019–2023 | ICCV/CVPR | ◐ 多数未开源或已下线 | Python + PyTorch | 视项目而定 | 4 | 单图全景光估计 → 生成 HDRI → Cycles World 节点；工程价值高、论文复现价值低（多无代码） |

**主题小结**：逆向渲染 2019–2026 的演进 = NeRF 系（NeRFactor→InvRender→TensoIR）→ 3DGS 系（GS-IR→Relightable3DGS）→ 生成式（DiffusionRenderer），输出物逐步逼近「可直接进 DCC 的 PBR 资产」。对 Cycles 的正确集成点是**资产/场景导入端**（SVBRDF+HDRI → 材质节点/World），而非渲染内核。

---

## 5. 主题五：神经材质 / 分层外观 / 测量材质重建（Neural BRDF/BSDF, Layered Appearance, Measured Materials）

| # | 论文 / 系统 | 年份 | 会议 | 代码 | 语言/框架 | 许可证 | 集成难度 | Cycles 兼容性备注 |
|---|---|---|---|---|---|---|---|---|
| 1 | **Neural BRDF Representation and Importance Sampling** | 2021 | Computer Graphics Forum 40(6)（EGSR 2022 口头报告） | ★ `asztr/Neural-BRDF` | Python + PyTorch | **MIT** | 4 | 用 MLP 表示 BRDF + 解析重要性采样（Sztrajman, Rainer, Ritschel, Weyrich）；可直接落地为 Cycles 的**新 SVM closure**（评估+采样都在 `kernel/closure` 内实现） |
| 2 | **NeuMIP: Multi-Resolution Neural Materials** | 2021 | SIGGRAPH (TOG 40(4)) | —（官方代码未公开；项目页 `cseweb.ucsd.edu/~viscomp/projects/NeuMIP/`；NeuSample 仓库提供其简化实现与 6D 数据） | Python + PyTorch | ⚠（数据以研究授权分发） | 4 | 多分辨率 6D 神经材质纹理（Kuznetsov 等）；「查表型」神经材质，契合 Cycles 的纹理管线；可仿照其 6D 结构做 `kernel/closure` 实现 |
| 3 | **NeuSample: Importance Sampling for Neural Materials** | 2023 | SIGGRAPH | ★ `bing-xu-graphics/neusample_release` | Python + PyTorch | ⚠未声明 | 4 | 神经材质的重要性采样（解析/归一化流/查表），附 40+ 6D 材质数据；与 NeuMIP 配套，是「神经材质进入路径追踪器」最完整的参考实现 |
| 4 | **Neural Layered BRDFs** | 2022 | SIGGRAPH (TOG 41(4)) | ◐ 官方 `Yujie-G/Neural-Layered-BRDF`（检索时不可达）；复现 `sssssy/pytorch-mitsuba-NLB_Release` | Python + PyTorch + Mitsuba | ⚠ | 4 | 用神经网络实现分层材质（顶涂层+散射层）的光学合成；分层外观正是 Cycles 缺少的能力（现有 Principled 为近似） |
| 5 | **NeuMERL（数据集）**：Neural Augmented MERL（2400 BRDF，NBRDF MLP 权重） | 2024/2026 | 配套 AAAI-40（M3ashy） | ★ HuggingFace `Peter2023HuggingFace/NeuMERL` | Python + numpy | **BSD** | 3 | 测量材质（MERL 100）的神经化扩充：每个 BRDF 存为 MLP 权重；可直接作 Cycles 神经 BRDF 闭包的**训练数据源** |
| 6 | **MatFormer: A Generative Model for Procedural Materials** | 2024 | SIGGRAPH (TOG 43(4)) | ★ 代码经 `adobe-research/ProcMatRL` 发布（含原 MatFormer 实现 + RL 训练，对应 *Procedural Material Generation with RL*, SIGGRAPH Asia 2024） | Python + Substance 3D Designer | **ADOBE RESEARCH LICENSE（非商用）** | 2 | 图条件生成 Substance Designer 程序化材质图；是**离线资产生成器**，导出为 Cycles 可用的 PBR 资产即可，与渲染内核无关 |
| 7 | 相关：**Neural Radiance Caching** | 2021 | SIGGRAPH | —（NVlabs 仓库当前不可达/未公开） | C++/CUDA | n/a | 4 | 渲染端神经加速（辐射缓存），与逆向/材质同属「神经网络进渲染器」主线；Cycles 已有 `volume_guiding_denoise.h` 同类先例 |

**主题小结**：神经材质的集成路径最清晰——① 数据：NeuMERL/NeuMIP 6D 数据；② 表示+采样：Neural BRDF / NeuSample 的 MLP+采样器；③ 落地：`kernel/closure` 新增 neural BSDF closure（SVM 节点「Neural BSDF」），评估走 MLP、采样走解析/查表。难度集中在 GPU kernel 的 MLP 推理与训练权重烘焙（offline 训练 → 权重表随材质分发）。

---

## 6. 集成 Cycles 综合评估与推荐路径

### 6.1 难度总览（1=直接可做，5=架构级/需外部管线）

| 方向 | 平均难度 | 接入点 | 结论 |
|---|---|---|---|
| 学习式去噪（OIDN 系 CNN / BMFR / NBGrid） | 4 | `src/integrator/denoiser.cpp` 抽象层 + 新推理后端（ONNX Runtime/TensorRT/OpenVINO） | **推荐近期落地**；特征 pass 完全复用；Blender 已带 ONNX 依赖，工程成本可控 |
| 非学习统计去噪（SVGF/A-SVGF/StatMC 思路） | 3 | `kernel/film` 方差 buffer + `path_trace_work` 时域状态 | **推荐中期落地**（视口实时模式）；最贴合现有 pass/film 语义 |
| 扩散去噪（3DV 2025） | 5 | 独立后处理管线（合成器节点） | 可行但性能受限，作离线选项 |
| 可微渲染（Mitsuba3/psdr-jit/redner） | 5 | 外部渲染器 + 场景/资产互转 | **不做内核集成**；以外部工具链形式提供 |
| 逆向渲染（TensoIR/GS-IR/PracticalInv） | 5 | 外部优化管线 → PBR 资产导入 | 同上；PracticalInv（Apache-2.0、Mitsuba 3 基）是最佳参考闭环 |
| 神经材质（Neural BRDF/NeuSample/NeuMIP） | 4 | `kernel/closure` 新增 neural BSDF closure + SVM 节点 | **推荐中期落地**；offline 训练 → 权重随材质分发 |
| 程序化资产生成（MatFormer） | 2 | 离线工具 → PBR 导出 | 低风险高性价比 |

### 6.2 建议路线（结合 Entro-Cycles 定位）
1. **近期（内核侧）**：在 `Denoiser` 抽象下新增「ONNX 学习式去噪器」后端，输入直连现有 denoising passes（depth/normal/albedo/roughness/flow），先对齐 OIDN 质量基线；
2. **近期（数据侧）**：用 NeuMERL（BSD）+ NeuMIP 6D 数据建立神经材质训练资产库；
3. **中期（实时）**：SVGF/A-SVGF 风格方差引导时域滤波，作为视口实时去噪路径（复用 `flow` + 自适应采样方差）；
4. **中期（材质）**：`kernel/closure` 新增 neural BSDF（MLP 评估 + 解析重要性采样，参考 NeuSample）；
5. **远期（外部工具链）**：Mitsuba 3 / psdr-jit 可微管线 + TensoIR/GS-IR/PracticalInv 逆向管线，输出 PBR 资产经 glTF/OpenPBR 导入 Cycles，形成「逆向→Cycles」闭环。

---

## 7. 附录

### 7.1 许可证速查（全部经仓库文件核实）
- Apache-2.0：OIDN、NeRFactor、InvRender、Practical Inverse Rendering
- BSD-3-Clause：Mitsuba 2 / Mitsuba 3 / Dr.Jit / psdr-cuda；NeuMERL 数据（BSD）
- MIT：redner、OpenDR、Neural-BRDF、BMFR、NBGrid(去噪)、PhySG、TensoIR、GS-IR
- 自定义 NVIDIA 许可（免费商用、限制再分发）：OptiX SDK、NRD（RTX SDKs License）、nvdiffrast（1-Way Commercial）、nvdiffrec、DiffusionRenderer
- 非商用自定义许可：3D Gaussian Splatting、Relightable 3D Gaussian（沿用 GS License）、MatFormer/ProcMatRL（Adobe Research License）
- 未声明（默认保留权利）：RDFC（无代码）、A-SVGF（KIT 附带代码）、NeuSample、Neural Layered BRDFs（官方仓库不可达）、psdr-jit、StatMC/StatER、DeepInverseRendering、扩散去噪
- 提示：集成进 Blender/Cycles（GPL / Apache-2.0 混合）时，**非商用与自定义许可的代码/权重不可内嵌**，只能作外部工具或数据来源；优先选 MIT/BSD/Apache 项。

### 7.2 「InteLi 2024」检索说明（重要）
对 **"InteLi"** 做了多轮检索（web_search 中英文、arXiv API 标题/全文、GitHub 仓库搜索），**未找到** 2024 年名为 "InteLi" 的 SIGGRAPH 论文或工具。最可能的对应物（供委托方确认）：
- (a) **Intel OIDN 2024–2025 的 GPU 更新**（v2.4/v2.5 加入 Xe/CUDA/HIP/Metal 支持，Blender 4.5+ 启用）——"InteLi" 疑为 "Intel" 的误记；
- (b) **Denoising Monte Carlo Renders with Diffusion Models**（NeurIPS 2024 / 3DV 2025，本报告 1.1 #8）；
- (c) **A Statistical Approach to Monte Carlo Denoising**（SIGGRAPH Asia 2024，1.1 #9）。
若委托方有原出处（论文名/链接），请反馈以便补录。

### 7.3 验证方法
- 仓库存在性与许可证：GitHub REST API（`/repos/{owner}/{repo}/license`）优先；受限时改用 `raw.githubusercontent.com` 拉取 LICENSE/LICENSE.md/LICENSE.txt/COPYING 并读首部；
- 会议/年份：优先官方项目页、作者主页、DOI（如 Deep Inverse Rendering 的 TOG 38(3) DOI）、Wiley/ACM 页面；
- 两处关键修正：NeRFactor = **SIGGRAPH Asia 2021**（非 ICCV）；PhySG = **CVPR 2021**（非 ICCV）；
- NRD 仓库已由 `NVIDIAGameWorks/RayTracingDenoiser` 迁移至 `NVIDIA-RTX/NRD`。

---

*报告结束。所有 URL/许可证以 2025-08 检索结果为准；仓库可能随上游更新而变动。*