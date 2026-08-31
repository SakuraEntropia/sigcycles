# 特性兼容 / 依赖图（F 项）

## 1. 特性元数据结构（概念）

    Feature
    ├── Supported render modes      # OFFLINE / REALTIME / WAVE
    ├── Requires                    # 硬依赖
    ├── Optional                    # 显式启用的可选增强
    ├── Conflicts                   # 不兼容项（启用时报错）
    ├── Experimental status         # Stable / Experimental / Research
    └── Kernel / host scope         # 见 01 §12

## 2. 关键特性详情

### Wave Optics（含 Wave Diffraction BSDF）
- Mode: WAVE（+ OFFLINE 内可作为材质层使用）；Experimental
- Requires: waveoptics 模块（FSD/UTD/Fresnel/Stokes-Mueller）、波长采样（Phase1 后）
- Optional: 偏振层（Stokes/Mueller）、薄膜层
- Conflicts: 几何光学积分器的整体假设（Wave Transport 与 Classic PT/ReSTIR 互斥）——启用 Wave Transport 时报错
- 注：FSD/UTD/偏振的数学与几何光路兼容（可作为闭包/材质层）；只有波前/光束传输才与几何积分器冲突

### ReSTIR DI / GI
- Mode: OFFLINE + REALTIME；Experimental
- Requires: reservoir sampling 基础设施、lamp 光源（当前有 bug，需先修复）
- Optional: temporal reuse、spatial reuse（各自独立开关，不随 ReSTIR 自动开启）
- Conflicts: Wave Transport

### SVGF / A-SVGF
- Mode: REALTIME；Research
- Requires: 时域重投影（motion vectors，Cycles 已有 passes）
- Optional: 自适应参数（A-SVGF）
- Conflicts: 无已知

### OIDN v3 时域去噪
- Mode: OFFLINE + REALTIME；Research(Phase1)
- Requires: denoising passes（已有）；OIDN 库升级
- Conflicts: 无

### Path Guiding（OpenPGL，已有）
- Mode: OFFLINE；Stable
- Optional: guided MIS、体积引导、训练调度（增量）
- Conflicts: 无；与自适应采样可共存

## 3. 传输模型兼容性（核心规则）

            Transport Model
                 |
   +-------------+--------------+
   |                            |
Geometric Optics            Wave Optics
   |                            |
PT / ReSTIR / Guiding /      Wave Transport
MIS / ...                   (cone/beam propagation)

规则：
1. Wave Transport 与几何光学积分器互斥——冲突时明确报错，不静默选择。
2. 波光学内可叠加：衍射 + 干涉 + 偏振（同一传输模型的层）。
3. 实时/离线互不自动启用；共享基础设施（scene/BVH/film/device）除外。
4. 无隐藏激活：ReSTIR 不自动开 temporal/denoising；依赖显式声明。
5. 许可证红线（并入代码必须 BSD-3/MIT/Apache-2.0）：GPL/Nvidia-NC/专有 SDK/无 LIC 只读思路。

## 4. 与 Cycles 既有开关的映射

| Cycles 已有 | 对应研究特性 |
|---|---|
| use_guiding / OpenPGL | Path Guiding（已内置）|
| use_adaptive_sampling | Adaptive Sampling（已内置）|
| caustics_reflective/refractive | MNEE（已内置）|
| light tree / light linking | 多光源 MIS 基础设施 |
| denoiser (OIDN/OptiX) | 传统去噪（已内置）|
| motion/backward_motion passes | 时域去噪/重投影输入（已就绪）|

---
