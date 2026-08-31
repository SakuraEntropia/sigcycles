# Research Edition 架构设计（第 3/8 节落地草案）

> 依据第 18 节 H 项：先设计、批准后再改代码。本草案映射到 Cycles 现有抽象。

## 1. RenderMode 概念 → Cycles 映射

- **建议**：在 src/scene/integrator.h 增加 render_mode 枚举（随场景/会话配置），默认 OFFLINE → 完全保留原版行为。
- 内核侧：kernel/types.h 增加 RenderMode 与 KernelIntegrator.render_mode 字段，路径状态机据此选择工作循环。
- 宿主侧：src/integrator/path_trace_work_cpu/gpu 之外新增 path_trace_work_realtime.cpp（REALTIME 工作循环：低采样 + 时域复用 + 去噪编排），共享 session/scene/device。

## 2. Transport 分层

    RenderMode
    ├── OFFLINE
    │   └── Transport: Geometric（现有 path_trace_work_* + 研究模块）
    │       或 Transport: Wave（wave transport 内核）
    └── REALTIME
        └── Transport: Geometric（实时工作循环）

- 传输模型（Geometric vs Wave）是比渲染模式更低一层的正交维度：合法组合 OFFLINE×Geo（默认）、OFFLINE×Wave（实验）、REALTIME×Geo（规划）；REALTIME×Wave 不做。
- 内核数据 KernelIntegrator 增加 transport 字段；if constexpr/特性位裁剪（沿用 KERNEL_FEATURE_* 机制）。

## 3. Feature 系统 → Cycles 节点体系

- **建议**：新增 src/scene/research_features.h/.cpp，定义 ResearchFeature 结构（id、enabled 显式开关、modes 支持的模式位、dependencies/conflicts 列表、status）。
- 宿主侧校验器 validate_feature_config()：启用前检查依赖（缺失→报错）、冲突（→报错并列出原因，遵循'不静默选择'）。
- 场景节点/XML：cycles_xml.cpp 暴露 integrator render_mode/features 配置；未来 Blender 集成走 Blender 属性。
- 内核侧：每个特性对应一个 KERNEL_FEATURE_RESEARCH_* 位（kernel/features.h 扩展），由场景特性位集合驱动编译裁剪。

## 4. Wave Optics 提升为传输模式（第 4/11 节）

- 现状：src/waveoptics/（数学）+ bsdf_wave_diffraction.h（闭包）。
- 提升路径：
  1. **保留** FSD/UTD/Fresnel/Stokes-Mueller 数学 + Wave Diffraction BSDF（wave 模式可用闭包）。
  2. **新增 Wave Transport 内核**（research 阶段）：参考 wave_tracer 的 elliptic cone 追踪（wave_tracer/include/wt/beam/、wt/integrator/plt_path/），评估移植 cone 追踪 vs 中间形态（kernel 内 kernel/wave/ 模块：波前/光束传播+交互采样，与几何 path_trace_work_* 并列）。
  3. **偏振/干涉**：在 wave 传输内作为层（Stokes/Mueller 已在 waveoptics 模块）。
  4. 冲突声明：Wave Transport 与 Geometric PT/ReSTIR 互斥（Feature System 强制）。

## 5. 实时模式工作循环（规划）

    path_trace_work_realtime
    ├── 低采样数路径（1-4 spp）
    ├── reservoir / temporal accumulation（帧间复用）
    ├── 重建（SVGF 类滤波 / 神经去噪 / 上采样）
    └── display driver（渐进显示，复用 path_trace_display）

- 依赖：lamp 光源 bug（实测独立构建 lamp 全黑）必须先修复——实时模式几乎全部依赖 lamp。

## 6. 模块隔离与基线

- 每个研究特性 = 独立目录/头文件组 + 特性位 + 独立开关 + 单元验证（复用 waveoptics 的 wav_test.cpp 模式）。
- 新增文件不触碰既有默认路径：path_trace_work_cpu/gpu、kernel/integrator 默认流保持原版。
- 每次集成：git stash 验证基线可构建 → 加特性 → 编译 → 单元测试 → 基准（见 05_benchmarking.md 规划）。
