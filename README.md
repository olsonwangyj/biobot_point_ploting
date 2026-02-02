# point_plotting_reserch

本仓库用于研究 Siemens MR 的 DICOM RTSTRUCT（放疗轮廓）数据：

- 判定 RTSTRUCT 存储的是“边界点”还是“内部点”
- 在保证几何相似度的前提下，寻找轮廓“最少需要多少点”
- 基于边界几何复杂度做自适应采样，并对重建质量进行分析

更完整的实验过程与结论见：

- report.md（中文）
- report_en.md（English）

## RTSTRUCT 存的是什么？

对本数据集，RTSTRUCT 的轮廓条目通常为：

- `ContourGeometricType = CLOSED_PLANAR`
- `ContourData` 为病人坐标系下的 (x, y, z)（单位：mm）

这意味着它存储的是器官轮廓的**边界点（contour points）**，而不是区域内部的点云。

## 环境与依赖

- 推荐 Python 3.10+
- 安装依赖：

```bash
pip install -r requirements.txt
```

说明：代码里用到了 `pydicom / numpy / matplotlib / scipy`，以及用于几何计算的 `shapely`。

## 数据位置（重要）

仓库中示例数据位于：

- `Siemens testing data results on RTStruct/`

当前脚本大多采用“脚本顶部配置区”的方式指定路径（例如 `RTSTRUCT_PATH` 或 `ROOT_DIR`）。你需要先把这些路径改成你机器上实际的 RTSTRUCT 路径/病人目录。

## 脚本说明（以 src/ 为准）

下面这些脚本是仓库当前存在且可运行的入口（旧 README 里提到的 `analyze_rtstruct.py` 等脚本已不在当前仓库中）。

- `src/check_edge_points.py`
	- 作用：快速检查 RTSTRUCT 中 ROI 列表与轮廓字段，验证 `len(ContourData) = 3 * NumberOfContourPoints`。

- `src/plot_min_points.py`
	- 作用：对单个 slice 的轮廓做“等弧长重采样”（固定点数），计算面积相对误差与 Hausdorff 距离，并输出不同点数的对比图。
	- 输出：在当前工作目录生成 `outputs/compare_XXXpts.png`。

- `src/plot_min_points_with_smoothing.py`
	- 作用：与 `plot_min_points.py` 类似，但增加 B-spline 平滑（主要用于可视化；误差计算仍基于真实折线点）。
	- 输出：在当前工作目录生成 `outputs/compare_XXXpts.png`。

- `src/prostate_contour_analysis.py`
	- 作用：一种自适应采样（用 spline 曲率做权重），并输出对比图与误差表。
	- 输出：在当前工作目录生成 `output_adaptive_xy/compare_adaptive_XXXpts.png`。

- `src/prostate_contour_analysis_enforce.py`
	- 作用：改进版自适应采样（基于多段线离散曲率 + Chaikin 平滑显示），并输出对比图与误差表。
	- 输出：在当前工作目录生成 `output_adaptive_smooth/compare_XXXpts.png`。

- `src/area_diff.py`
	- 作用：读取同一 RTSTRUCT 中前列腺的所有 slice，按面积挑选 small/medium/large 三个代表性 slice，分别评估自适应采样在不同点数下的面积误差与 Hausdorff。
	- 特点：Hausdorff 使用“点到线段”的版本（`hausdorff_polyline`），在两条曲线采样密度不同的情况下更稳定。
	- 输出：在当前工作目录生成：
		- `output_adaptive_smooth/area_small/compare_XXXpts.png`
		- `output_adaptive_smooth/area_medium/compare_XXXpts.png`
		- `output_adaptive_smooth/area_large/compare_XXXpts.png`

## 复现运行（推荐从仓库根目录运行）

1) 安装依赖

```bash
pip install -r requirements.txt
```

2) 修改脚本顶部的路径配置

- `RTSTRUCT_PATH = r"...\.dcm"`（指向具体 RTSTRUCT 文件）
- 或 `ROOT_DIR = Path(r"...\N11780398")`（指向包含 RTSTRUCT 的病人目录）

3) 运行示例

```bash
python src/check_edge_points.py
python src/plot_min_points_with_smoothing.py
python src/prostate_contour_analysis_enforce.py
python src/area_diff.py
```

## 输出物解释

- 脚本会在“运行时的当前目录”下创建 `outputs/`、`output_adaptive_xy/`、`output_adaptive_smooth/` 等输出目录。
- PNG 图主要用于对比：原始轮廓（通常以平滑曲线显示） vs 简化/自适应采样后的轮廓（含采样点）。
- 终端表格会打印每个点数对应的：
	- 面积相对误差（%）
	- Hausdorff 距离（mm）

## 常见问题

- 如果 `pip install shapely` 在 Windows 报错，通常是环境/缓存问题；建议先升级 `pip`：

```bash
python -m pip install -U pip
```

- 如果你从 `src/` 目录运行脚本，输出会生成在 `src/` 下；从仓库根目录运行则会生成在根目录下。两者都可以，但建议固定一种方式便于管理输出。
