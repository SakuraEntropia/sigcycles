# Cycles Research Edition — 架构分析报告（A/B）

> 对象仓库：`/Users/faputa/Documents/Entro-Cycles/cycles`（Blender Cycles 独立仓库，Apache-2.0，基线 `v5.0.0-461-g8424ed531`）

---

## 1. 仓库布局总览

```text
src/
├── app/         独立可执行入口（cycles_standalone.cpp、cycles_xml.cpp 场景解析、OI/OIIO 输出、opengl 预览）
├── device/      GPU/CPU 设备抽象（cpu/cuda/hip/hiprt/metal/oneapi/optix/multi/dummy + 内存/队列/图形互操作/去噪驱动）
├── kernel/      内核（头文件库，GPU/CPU 共用一份 C++ 代码）
├── integrator/  宿主侧积分器（path_trace_work_cpu/gpu、自适应采样、去噪、tile 调度、显示）
├── scene/       场景图（几何/材质/灯光/相机/BSDF 着色器节点/BVH 构建/采样器/积分器设置）
├── session/     会话层（buffers、denoising、display driver、cache eviction）
├── graph/       通用节点系统（Node/NodeType/XML 序列化）
├── subd/        细分曲面
├── bvh/         宿主侧 BVH 构建
├── util/        数学/线程/图像/色彩管理/哈希
├── test/        GTest 测试
└── waveoptics/  【新增】波动光学模块（FSD/UTD/Fresnel/Stokes-Mueller/高斯波前 + 采样表）
```

## 2. 积分器架构（离线路径追踪）

### 2.1 内核侧（kernel/integrator/）
- **单 megakernel 状态机**：`megakernel.h` + `path_state.h`/`state.h`/`state_flow.h`/`state_util.h` 定义 `PathState`/`IntegratorState`。路径：`init_from_camera` → `intersect_closest` → `shade_surface`/`shade_volume`/`shade_background`/`shade_light` → `next_iteration`；`intersect_shadow`/`shade_shadow` 处理 NEE；`intersect_dedicated_light`/`shade_dedicated_light` 处理直接命中发光体。
- **MNEE**（焦散）：`mnee.h`/`intersect_mnee.h`，开关 `caustics_reflective/refractive`（scene/integrator.h）。
- **路径引导（OpenPGL）**：内核 `guiding.h` + 宿主 `src/integrator/guiding.h`；开关组 `use_guiding`/`use_surface_guiding`/`use_volume_guiding`/`guiding_mis_weights`/`guiding_training_samples`/`guiding_distribution_type` 等。
- **自适应采样**：`adaptive_sampling.h`（内核）+ `src/integrator/adaptive_sampling.cpp`，`use_adaptive_sampling`/`adaptive_min_samples`/`adaptive_threshold`。
- 阴影捕捉器 `shadow_catcher.h`、阴影链接 `shadow_linking.h`、次表面 `subsurface.h`/`subsurface_disk.h`/`subsurface_random_walk.h`、位移 `displacement_shader.h`。

### 2.2 宿主侧（src/integrator/）
- `path_trace_work_cpu.cpp/.h`：CPU 路径追踪工作循环。
- `path_trace_work_gpu.cpp/.h`：GPU 端（occupancy 管理、megakernel 分派）。
- `path_trace_tile.cpp`：tile 并发调度；`path_trace_display.cpp`：渐进式交互显示。
- `denoiser_*.cpp`：OIDN（Intel）/ OptiX 去噪器驱动。
- `pass_accessor_*.cpp`：pass 读取。

## 3. 采样架构
- `kernel/types.h`：`enum SamplingPattern { SAMPLING_PATTERN_SOBOL_BURLEY, TABULATED_SOBOL, BLUE_NOISE_PURE, BLUE_NOISE_FIRST, BLUE_NOISE_ROUND }`。
- `kernel/sample/`：`sobol.h`（+Cranley-Patterson 旋转）、`pmj.h`、`tabulated_sobol.h`、`mapping.h`（半球/盘/锥采样）、`filter.h`（像素滤波）、`latte.h`（TAA 抖动，实时基础）。
- RNG：按路径维度分配随机数流。
- **MIS**：`kernel/integrator/surface_shader.h` 的 `_surface_shader_bsdf_eval_mis`（幂次启发式）；`kernel/light/sample.h`（光采样）与 BSDF 采样构成 MIS。
- 直接光照策略：`enum DirectLightSamplingType`（MIS/Forward(NEE-only)/NEE）。

## 4. 光源采样（kernel/light/）
- `tree.h`：**光树**（分层光源聚类 + 光链接）；`distribution.h`：光源 CDF；`sample.h`：NEE 入口；`background.h`：环境光采样（含 equirect）；`area/point/spot/sun/triangle.h`：各光源类型。
- 宿主侧：`src/scene/light.h/.cpp`、`src/scene/light_tree.cpp`。
- ⚠️ 已知环境问题（实测，与本项目无关）：**独立构建中 lamp 类光源（point/area/sun/spot）渲染为黑**（仓库自带 caustics 示例同样黑），背景/天空光正常；`environment_texture` 图像有色彩空间 bug（品红）。研究特性若依赖 lamp 需先修。

## 5. 去噪与 pass 系统
- `kernel/types.h` `enum PassType`：完整 pass 集（EMISSION/BACKGROUND/AO/DIFFUSE*/GLOSSY*/TRANSMISSION*/VOLUME*/POSITION/NORMAL/ROUGHNESS/UV/OBJECT_ID/MATERIAL_ID/MOTION/CRYPTOMATTE/AOV_*/ADAPTIVE_AUX/SAMPLE_COUNT/DENOISING_*）。
- `kernel/film/`：`light_passes.h`/`denoising_passes.h`（clean/depth/normal/albedo 等）/`aov_passes.h`/`cryptomatte_passes.h`/`data_passes.h`/`adaptive_sampling.h`/`volume_guiding_denoise.h`/`write.h`/`read.h`。
- `src/integrator/denoiser_oidn*`/`denoiser_optix`/`denoiser_gpu`；`src/session/denoising.cpp` 编排。
- 扩展点：`kernel/film/` 加重建 pass；`src/integrator/` 加去噪器；`session/denoising.cpp` 编排。

## 6. GPU 后端
- `src/device/`：Device 抽象 + 后端目录 cpu/cuda/hip/hiprt/metal/oneapi/optix/multi/dummy + memory/queue/graphics_interop。
- `src/kernel/device/`：每个后端一个小 TU include `kernel/device/gpu/kernel.h`（megakernel）→ 头文件库模式（`add_library(cycles_kernel INTERFACE)`）保证 CPU/GPU 一致。
- 独立构建现状（实测）：macOS arm64 上 CPU + METAL 可编译（本次完整构建成功）；CUDA/OptiX/HIP 需厂商工具链。
- GPU 运行时编译依赖 `install/source/kernel/`（含新增 waveoptics 头）。
- 扩展点：研究特性写成纯内核头自动获得全部后端；后端特有能力（OptiX 去噪等）在 device/<backend>/ 加。

## 7. BVH / 加速结构
- 宿主侧 `src/bvh/`：bvh2 + Embree + OptiX + MetalRT 四套构建；内核侧 `kernel/bvh/` 对应遍历。
- 场景侧 `src/scene/bvh.cpp`、`src/scene/object.cpp`（motion blur/变换缓存）。

## 8. 材质 / 着色
- 内核闭包 `kernel/closure/`：diffuse/oren_nayar/burley/microfacet(GGX/Beckmann/多GGX)/sheen/toon/ashikhmin/hair(Chiang/Huang)/transparent/ray_portal + 【新增】`bsdf_wave_diffraction.h`；BSSRDF；volume_*；emissive。
- SVM `kernel/svm/`（着色器字节码解释器 + 类型化节点数据）；OSL `kernel/osl/`（118 个内建 .osl）；场景节点 `src/scene/shader_nodes.h/.cpp`。
- Principled BSDF → SVM 运行时拆分为底层闭包；虚拟闭包（physical conductor 等）映射。
- 频谱：`Spectrum` 即 float3（RGB），本 checkout 无独立频谱渲染特性位（grep KERNEL_FEATURE_SPECTRAL 为空）；波长相关效果需自行接入。
- 扩展点：新材质 = 闭包头 + 枚举 + SVM + 节点（本次已走通全流程）。

## 9. 体渲染
- 内核：`kernel/integrator/shade_volume.h`（体积散射）、`intersect_volume_stack.h`（嵌套体积）、`kernel/closure/volume_*.h`（HG/Rayleigh/Mie/Draine/FF 相函数）。
- 选项：`volume_ray_marching`/`volume_max_steps`/`volume_step_rate`；`use_volume_guiding`（OpenPGL）。
- 数据：OpenVDB/NanoVDB（`src/scene/volume.cpp`、`kernel/util/nanovdb.h`）。
- 扩展点：多散射加速在 `shade_volume.h` + `kernel/closure/volume_*.h` 层。

## 10. 实时能力现状
- 独立构建无实时视口（`src/app/opengl/` 仅简单 GL 预览）；Blender 集成不在本仓库。
- 已有实时基础：`kernel/sample/latte.h`（TAA 抖动）、`path_trace_display`（渐进显示）、OptiX/Metal 后端。
- **REALTIME MODE 需新增**：独立低采样/时域复用管线（temporal accumulation、reservoir、SVGF 类滤波、实时去噪编排），建议作为 `src/integrator/` 下与 `path_trace_work_cpu/gpu` 并列的新工作循环，共享 scene/film/device。

## 11. 波动光学现状（本次已集成）
- 模块 `src/waveoptics/`（FSD 夫琅禾费衍射 ASF、UTD 边缘衍射、复误差函数、Fresnel、Stokes/Mueller、高斯波前、重要性采样器+宿主逆 CDF 表）；闭包 `kernel/closure/bsdf_wave_diffraction.h`（Wave Diffraction BSDF，端到端接入，构建+渲染验证通过）。
- **当前定位**：只是一个 BSDF 开关，**不是独立传输模式**。按项目要求需提升为波传输管线（wave transport regime）：Wave Transport（光束/波前传播）→ Propagation → Diffraction(FSD/UTD) → Interference → Polarization(Stokes/Mueller) → wave-specific sampling。
- 与几何光学经典路径追踪假设冲突 → 必须显式隔离（Feature System Conflicts 声明）。
- 现有数学已就绪；缺的是波前/光束传播积分器（wave_tracer 的 elliptic cone 追踪是参考实现——需评估移植 cone 追踪 vs wave BSDF+波传输内核）。

## 12. 研究扩展点总结表

| 子系统 | 位置 | 研究特性落点 |
|---|---|---|
| 路径采样 | kernel/integrator + kernel/sample | ReSTIR/RIS、神经采样、引导(已有 OpenPGL) |
| 光源采样 | kernel/light | 光树改进、ReSTIR DI、多光源 MIS |
| 重建 | kernel/film + src/integrator/denoiser_* | 神经去噪、SVGF、时域累积、可微输出 |
| 材质 | kernel/closure + kernel/svm | 神经 BRDF、衍射(已有)、薄膜、偏振 |
| 体 | kernel/integrator/shade_volume | 多散射、神经体渲染 |
| 实时 | src/integrator/(新增) | 时域复用、reservoir、radiance cache |
| 波动光学 | src/waveoptics + 新波传输内核 | 波前传播、干涉、偏振传输 |
| 设备 | src/device/<backend> | OptiX 去噪、实时光追 |

## 13. 基线保持策略
- 所有研究特性为可选开关（Feature System），默认关闭 → 原版行为不变。
- 内核侧用 `KERNEL_FEATURE_*` 位 + `if constexpr` 特性裁剪（已有机制），不改变默认 kernel 编译。
- 保持 `git stash` 可还原基线；每个特性集成前先建立独立单元验证（如 waveoptics 的 `wav_test.cpp` 模式）。

---
