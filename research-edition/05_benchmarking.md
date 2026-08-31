# 基准测试方法论（第 15 节规划）

## 1. 离线模式（Offline）

| 指标 | 方法 |
|---|---|
| 渲染时间 / samples/s | 同场景同 spp 计时（cycles_standalone --samples，wall time + CPU 时间） |
| 收敛速度 | 固定时间预算 vs 固定 spp 的 RMSE 曲线（对参考解） |
| 方差 | 多种子渲染的 per-pixel 方差 / 参考图像 RMSE |
| PSNR / SSIM | 与参考解（高 spp / 无偏参考）比较（Python: skimage/metrics） |
| 内存 | peak RSS（/usr/bin/time -l 或 mach task_info） |
| GPU 利用率 | Metal: xcrun xcrun metal 工具 / Instrum；CUDA: nvidia-smi |

基准场景集（随项目维护）：Cornell Box（验证基础）、Sponza（GI）、双缝衍射（波动光学）、BDPT 难题（MLT 类）、体散射场景。

## 2. 实时模式（Realtime）

| 指标 | 方法 |
|---|---|
| 帧时间 / FPS | 连续帧计时（排除首帧编译） |
| 延迟 | 输入→显示 端到端 |
| 时域稳定性 | 帧间 flicker（时序方差）、静止场景帧间差 |
| 噪声 | 1-4 spp 时域累积后的 RMSE |
| 图像质量 | 参考高 spp 离线帧的 PSNR/SSIM |
| GPU 内存 | 设备内存占用 |

## 3. 波动光学（Wave）

- **不盲目套用路径追踪指标**：波模拟是确定性场传播，测量物理量：
  - 衍射图案与解析解的一致性（单缝/双缝 sinc 图案、Airy 斑）——逐点相对误差；
  - 干涉条纹对比度（visibility = (Imax-Imin)/(Imax+Imin)）；
  - 偏振度（DoP）随场景传播的守恒/变换正确性；
  - 计算开销（wave 模式 vs 等效几何模式）。
- 沿用 waveoptics 单元测试（wav_test.cpp）作为最小正确性门禁。

## 4. 流程

每个特性集成后：单元验证 → 构建 → 基准对比（Research vs 基线 Cycles）→ 数据记入 feature matrix（04 文档）。
