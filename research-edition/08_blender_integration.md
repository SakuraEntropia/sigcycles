# Blender 集成指南（Research Edition -> Blender Cycles）

> 目标：把 Research Edition 的新闭包（wave_diffraction、wave_thin_film）
> 和研究功能系统（render_mode、research_features、mis_exponent、
> svgf_temporal）集成进 Blender 的 Cycles。
>
> Blender 官方 Cycles 不是插件，而是 Blender 源码树内的内置渲染引擎
> （intern/cycles/）。所以"怼进去"的正规做法是源码级移植 + 节点
> 系统注册 + （可选）Python 插件包装 UI。本文档参考 Blender 官方
> Cycles 的集成方式给出完整映射。

## 1. 官方 Cycles 在 Blender 中的位置

    blender/
    ├── intern/cycles/          # 渲染内核（与我们的 cycles/ 结构一致）
    │   ├── src/kernel/         # 内核（closure/svm/...）
    │   ├── src/scene/          # 场景图 + shader 节点
    │   ├── src/app/            # standalone 入口（Blender 不用 app/，用 api/）
    │   └── src/api/            # Blender 专用 API 层（cycles 与 Blender 的桥）
    ├── source/blender/
    │   ├── nodes/shader/nodes/ # Blender 节点定义（node_shader_xxx.cc）
    │   ├── nodes/intern/       # 节点编译到 Cycles SVM
    │   ├── makesdna/           # DNA 结构（.h 头文件，.c 默认值）
    │   ├── makesrna/           # RNA API + UI 面板
    │   └── render/intern/      # 渲染引擎接入（CyclesRender 类）
    └── extern/cycles/          # 旧版；新版在 intern/cycles

## 2. 移植步骤（源码级）

### 2.1 内核侧（直接复制）

以下文件在 Blender 的 intern/cycles/src/ 下路径相同，直接复制：

| Research Edition 文件 | 目标路径（intern/cycles/src/） | 说明 |
|---|---|---|
| waveoptics/*.h | waveoptics/ | 纯数学，零依赖 |
| kernel/closure/bsdf_wave_diffraction.h | kernel/closure/ | 衍射闭包 |
| kernel/closure/bsdf_wave_thin_film.h | kernel/closure/ | 薄膜闭包 |
| kernel/svm/node_types.h（改动） | kernel/svm/ | SVM 数据结构 |
| kernel/svm/closure.h（改动） | kernel/svm/ | SVM 到闭包 dispatch |
| kernel/svm/types.h（改动） | kernel/svm/ | ClosureType 枚举 |
| kernel/closure/bsdf.h（改动） | kernel/closure/ | sample/eval/label dispatch |
| kernel/data_template.h（改动） | kernel/data_template.h | KernelIntegrator 字段 |
| kernel/sample/mis.h（改动） | kernel/sample/ | mis_exponent |
| kernel/light/sample.h（改动） | kernel/light/ | MIS 权重调用 |
| kernel/film/adaptive_sampling.h（改动） | kernel/film/ | 自适应误差 |

注意：Blender 官方 ClosureType 枚举（svm/types.h）与我们的不完全相同
（Blender 版本差异），加新闭包时在枚举末尾追加，不要改现有值，
避免 SVM 字节码不兼容。

### 2.2 场景侧（Blender 节点系统）

Blender 的 shader 节点在 source/blender/nodes/shader/nodes/，
每个节点一个 node_shader_xxx.cc。仿照官方 node_shader_glass.cc：

    /* node_shader_wave_thin_film.cc */
    #include "node_shader_util.hh"
    namespace blender::nodes::node_shader_wave_thin_film_cc {
    static void node_declare(NodeDeclarationBuilder &b)
    {
      b.add_input<decl::Float>("Ambient Index").default_value(1.0f).min(1.0f);
      b.add_input<decl::Float>("Film Index").default_value(1.5f).min(1.0f);
      b.add_input<decl::Float>("Substrate Index").default_value(2.0f).min(1.0f);
      b.add_input<decl::Float>("Film Thickness").default_value(100.0f).min(0.0f);
      b.add_input<decl::Color>("Color").default_value({1, 1, 1, 1});
      b.add_output<decl::Shader>("BSDF");
    }
    NODE_SHADER_NODE_DEFINE(wave_thin_film, ShaderNodeWaveThinFilm, "Wave Thin Film");
    }  // namespace
    void register_node_type_sh_wave_thin_film(void) { ... }

然后：
1. source/blender/nodes/shader/nodes/CMakeLists.txt 加 node_shader_wave_thin_film.cc
2. source/blender/nodes/shader/register.cc 加 register_node_type_sh_wave_thin_film()
3. DNA：source/blender/makesdna/DNA_node_types.h 加 ShaderNodeWaveThinFilm
4. RNA：source/blender/makesrna/intern/rna_nodetree.cc 注册属性

### 2.3 SVM 编译（Blender 节点到 Cycles）

Blender 节点编译到 Cycles SVM 在 source/blender/nodes/intern/node_exec.cc
+ source/blender/editors/.../node_shader_util.cc。仿照 glass 闭包，
把 wave_diffraction / wave_thin_film 加入闭包映射表（sh_node_bsdf_*_build_svm）。

### 2.4 渲染引擎接入（integrate）

Blender 的 Cycles 渲染器在 source/blender/render/intern/cycles_*.cc：
- cycles_scene.cc：场景同步（BlenderSync）
- cycles_shader.cc：shader 同步（BlenderShader）
- cycles_integrator.cc：integrator 属性同步

在 cycles_integrator.cc 的 sync_integrator 中加：
    integrator->set_render_mode(...);
    integrator->set_research_features(...);
    integrator->set_mis_exponent(b_scene.render->cycles.mis_exponent);
    integrator->set_svgf_temporal(b_scene.render->cycles.svgf_temporal);

### 2.5 构建

    make update          # 更新子模块（含 cycles 依赖）
    make bpy             # 构建 Blender with Cycles

## 3. 插件包装（可选，纯 Python addon）

源码级集成需要重新编译 Blender。如果只想在官方 Blender 里用，
可以做一个 Python addon，通过 OSL 节点或自定义节点组暴露参数。

OSL 路径：Blender 的 OSL 支持允许用户 shader 直接调内核函数
（如果内核导出 OSL 闭包）。把 bsdf_wave_thin_film_eval 逻辑写成
OSL shader 即可在官方 Blender 用（性能低于原生闭包，但零编译）。

## 4. 官方集成参考（本次移植的依据）

| 官方功能 | Blender 集成文件 | 我们的对应 |
|---|---|---|
| Glass BSDF | node_shader_glass.cc + bsdf_microfacet.h | wave_thin_film |
| 新增 Pass | film/passes.h + cycles_scene.cc | （未来 Stokes AOV） |
| 新增 Integrator 属性 | cycles_integrator.cc + scene/integrator.h | mis_exponent/svgf_temporal |
| 新增 Closure 类型 | svm/types.h + bsdf.h dispatch | wave_diffraction/thin_film |

## 5. 验收

1. Blender 中新建材质，添加 Wave Thin Film 节点，调整厚度看到干涉色
2. Render Properties 面板出现 Research Edition 区块（render_mode、features、MIS、SVGF）
3. 渲染对比：不同膜厚面板颜色不同（对应 examples/scene_thin_film.xml）

---
*本指南与 research-edition/ 其它文档配套；Blender 源码版本不同时，
节点/RNA 文件名可能变化，但映射关系一致。*

---

## 10. Blender 5.3 移植关键发现（实测验证）

### 10.1 Socket 名匹配规则（重要!）

Blender 5.3 的 Cycles 中，ShaderInput::name() 返回 UI 名（socket_type.ui_name），
不是变量 identifier。因此：

- SVMCompiler::input_float("n0") 会返回 nullptr 并崩溃 —— 必须用 UI 名
  input_float("Ambient Index")。
- Blender 节点声明的输入名（b.add_input<decl::Float>("Ambient Index")）生成
  identifier = name（含空格），cycles 侧 name() 也必须等于该名字，
  两侧才能匹配（node_find_input_by_name 用 b_socket.identifier 匹配
  node->input()）。
- 结论：Blender 节点输入名、cycles NODE_DEFINE 的 ui_name、compile 里
  input_float() 参数三者必须完全一致。

### 10.2 厚度单位

- Blender 节点 "Film Thickness (nm)" 默认 100.0（nm）。
- cycles scene 节点 SOCKET_IN_FLOAT(thickness, "Film Thickness (nm)", 100.0f)。
- closure setup 里 thickness * 1e-9f 转米；eval 里波长 *1e-9f 转米。

### 10.3 实测结果（built 8e5913cb9515）

- 60nm -> 蓝 (0,8,24)
- 120nm -> 橙红 (32,16,0)
- 200nm -> 黄 (40,32,0)
- 与 wav_thin_film_reflectance_unpolarized 数值一致。
- Wave Diffraction 节点 width=0.01 时渲染白色（窄缝 0.2mm 能量过低近黑）。

### 10.4 相机对不准的坑

Blender 默认相机朝 -Z，cam.location=(0,-6,1.5) 不设 rotation 会看地面，
渲染出来全是背景色（59,59,59），误以为闭包没生效。用
dir_vec.to_track_quat('-Z','Y') 或显式 rotation 对准目标。
