# 首批实现选择（G 项：前 5-10 项高价值实现）

> 评分依据：研究影响力 × 与 Cycles 契合度 × 许可证合规 × 视觉/教学价值 ÷ 集成难度。
> 硬约束：可并入代码必须 BSD-3/MIT/Apache-2.0；wave_tracer 派生（CC BY-NC）只重实现公式。

## 评分表（候选排序）

| # | 技术 | 模式 | 论文 | 许可 | 难度 | 价值 | 理由 |
|---|---|---|---|---|---|---|---|
| 1 | 波长采样/光谱化 Wave Diffraction | WAVE | FSD BSDF 2024 | 重实现(CC-NC公式) | 3 | ★★★★★ | 解锁衍射色散/干涉色；现有模块最大瓶颈；pbrt-v4(Apache)作管线参考 |
| 2 | Stokes 偏振 AOV + Mueller 层 | WAVE | EG 2018 | mitsuba3 BSD-3 | 3 | ★★★★☆ | 补齐偏振传输；wav_stokes.h 已就绪 |
| 3 | SVGF / A-SVGF | REALTIME | HPG 2017/18 | 重写(算法公开) | 2 | ★★★★★ | 实时模式首个重建特性；Cycles pass 架构契合 |
| 4 | OIDN v3 时域去噪 | SHARED | OIDN 2026 | Apache-2.0 | 1 | ★★★★★ | 库升级即得时域去噪；已集成基础 |
| 5 | Efficiency-Aware MIS | OFFLINE | EAMIS 2022 | 理论公开 | 2-3 | ★★★★☆ | 无偏方差改进；kernel 定点改 |
| 6 | 自适应采样升级 | OFFLINE | DAAS 2023 | 参考 | 2 | ★★★★☆ | 收敛提升；宿主侧 |
| 7 | 薄膜多层干涉 closure | WAVE/OFFLINE | SA 2023 | 重实现 | 2-3 | ★★★☆☆ | 干涉色视觉效果强；依赖 #1 |
| 8 | GRIS/RIS 离线 many-light | OFFLINE | GRIS 2022 | DQLin/ReSTIR_PT BSD-3 | 4 | ★★★★☆ | 困难光传输；配 light tree |
| 9 | PSSMLT 可选积分器 | OFFLINE | Veach 1997 | pbrt-v3 BSD-2 | 3-4 | ★★★☆☆ | 经典 MLT；教学价值 |
| 10 | ReSTIR DI | REALTIME | SIGGRAPH 2020 | 重写(BSD-3参考) | 3-4 | ★★★★★ | 实时标志特性；需 lamp 修复 |
| - | RenderMode/Feature 系统 | SHARED | - | 自研 | 2 | ★★★★★ | 全部特性的框架（第 3/5/6/7 节） |
| - | lamp 光源修复 | SHARED | - | - | 2 | ★★★★★ | 实时/多光源特性的前提 |

## Phase 0（基础设施，先行）

1. **RenderMode + Feature 系统**（03_architecture_design.md）：RenderMode 枚举、ResearchFeature 注册表、依赖/冲突校验器、KERNEL_FEATURE_RESEARCH_* 位、XML 配置入口。
2. **lamp 光源修复**：独立构建 lamp 全黑（实测问题）——定位 light 管线，使点/面/聚光/太阳正常；实时模式的硬前提。
3. **基线确认**：完整构建 + 单元测试门禁（waveoptics wav_test 已就位）；git 工作流（每次集成前 stash 验证基线）。

## Phase 1（首批 6 项）

1. **波长采样/光谱化 Wave Diffraction**（WAVE，难度 3）——FSD closure 从单色改为每路径波长采样；参考 pbrt-v4 光谱实现；解锁色散。
2. **Stokes 偏振 AOV + Mueller 材质层**（WAVE，难度 3）——S0-S3 输出 + Mueller 闭包（mitsuba3 对照）；wav_stokes.h 已备。
3. **SVGF / A-SVGF**（REALTIME，难度 2）——kernel 时域重投影 + 方差引导空间滤波；接入 REALTIME 工作循环。
4. **OIDN v3 时域去噪升级**（SHARED，难度 1）——升级依赖库 + 时域模型接线（motion passes 已就绪）。
5. **Efficiency-Aware MIS**（OFFLINE，难度 2-3）——MIS 权重改进（kernel/light + surface_shader 的 MIS 函数）。
6. **自适应采样升级**（OFFLINE，难度 2）——改进误差判据（DAAS/Error Estimation 思路）。

## Phase 2（中期 4 项）

7. **薄膜多层干涉 closure**（WAVE/OFFLINE，难度 2-3，依赖 #1）。
8. **GRIS/RIS 离线 many-light 重采样**（OFFLINE，难度 4，BSD-3 参考重写）。
9. **PSSMLT 可选积分器**（OFFLINE，难度 3-4，pbrt-v3 参考）。
10. **ReSTIR DI**（REALTIME，难度 3-4，BSD-3 参考重写，依赖 lamp 修复）。

## 红线（明确不做/仅参考）

- GPL-3.0（practical-path-guiding）、Nvidia NC（conditional-restir）、RTXDI/RTXGI/NRD/DLSS/XeSS（专有 SDK）——只读思路。
- 神经采样/神经去噪/神经材质的内核集成（缺官方代码 + ML 推理运行时；CPU/METAL 独立构建不可行）——外部工具链路线。
- ReSTIR 实时全量移植（Falcor/HLSL 架构差异 + 帧间 persistent buffer 与离线多采样冲突）——离线化 GRIS 先行。

---

*本选择在 Phase 0 完成后、实现任何 Phase 1 特性前，需用户确认。*
