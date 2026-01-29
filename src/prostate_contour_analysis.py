import numpy as np
import pydicom
import matplotlib.pyplot as plt
from scipy.interpolate import splprep, splev
from scipy.spatial.distance import cdist
from pathlib import Path

# ============================================================
# 用户配置
# ============================================================

RTSTRUCT_PATH = r"D:\point_plotting_reserch\Siemens testing data results on RTStruct\N11780398\AIRC Research Prostate MR - RTSTRUCT_NotForClinicalUse\_.RTSTRUCT.prostate.3030.0.2025.12.09.07.30.49.960.11930327.dcm"

POINT_LIST = [16, 32, 48, 64, 128, 256]  # 不同点数
OUT_DIR = Path("output_adaptive_xy")
OUT_DIR.mkdir(exist_ok=True)

# ============================================================
# RTSTRUCT 读取（忽略 z，只取第一个 contour 的 XY）
# ============================================================

def load_prostate_contour_xy(rtstruct_path):
    ds = pydicom.dcmread(rtstruct_path)

    roi_number = None
    for roi in ds.StructureSetROISequence:
        if roi.ROIName.lower() == "prostate":
            roi_number = roi.ROINumber
            break

    if roi_number is None:
        raise RuntimeError("Prostate ROI not found")

    # 找到 ROIContour
    for roi_contour in ds.ROIContourSequence:
        if roi_contour.ReferencedROINumber == roi_number:
            contour_seq = roi_contour.ContourSequence[0]  # 直接取第一个 slice
            pts = np.array(contour_seq.ContourData).reshape(-1, 3)
            return pts[:, :2]  # 忽略 z，只返回 XY

    raise RuntimeError("No contour found")

# ============================================================
# 自适应采样（曲率高点密集）
# ============================================================

def resample_adaptive(curve, n_points):
    x, y = curve[:,0], curve[:,1]
    x = np.r_[x, x[0]]  # 闭合
    y = np.r_[y, y[0]]
    
    # spline 拟合闭合曲线
    tck, u = splprep([x, y], s=0, per=True)
    u_fine = np.linspace(0, 1, 1000)
    xs, ys = splev(u_fine, tck)
    
    # 计算曲率
    dx = np.gradient(xs)
    dy = np.gradient(ys)
    ddx = np.gradient(dx)
    ddy = np.gradient(dy)
    kappa = np.abs(dx*ddy - dy*ddx) / (dx**2 + dy**2)**1.5
    kappa += 1e-6  # 防止 0
    
    # 累积曲率做权重
    cum_kappa = np.cumsum(kappa)
    cum_kappa /= cum_kappa[-1]
    
    # 在累积曲率上等间距采样 n_points 个点
    u_new = np.interp(np.linspace(0, 1, n_points, endpoint=False), cum_kappa, u_fine)
    xs_new, ys_new = splev(u_new, tck)
    return np.column_stack([xs_new, ys_new])

# ============================================================
# 平滑闭合曲线（仅用于绘图）
# ============================================================

def smooth_closed_curve(points, num=600):
    x, y = points[:,0], points[:,1]
    x = np.r_[x, x[0]]
    y = np.r_[y, y[0]]
    tck, _ = splprep([x, y], s=0, per=True)
    u = np.linspace(0, 1, num)
    xs, ys = splev(u, tck)
    return np.column_stack([xs, ys])

# ============================================================
# 数值分析
# ============================================================

def polygon_area(pts):
    x, y = pts[:,0], pts[:,1]
    return 0.5 * abs(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))

def hausdorff(a, b):
    d = cdist(a, b)
    return max(d.min(axis=1).max(), d.min(axis=0).max())

# ============================================================
# 主流程
# ============================================================

def main():
    orig = load_prostate_contour_xy(RTSTRUCT_PATH)
    area_orig = polygon_area(orig)
    orig_smooth = smooth_closed_curve(orig)

    print("\npts | area_err(%) | hausdorff(mm)")
    print("-"*34)

    for n in POINT_LIST:
        simp = resample_adaptive(orig, n)
        simp_smooth = smooth_closed_curve(simp)

        # 数值分析
        area_err = abs(polygon_area(simp) - area_orig) / area_orig * 100
        h = hausdorff(orig, simp)
        print(f"{n:3d} | {area_err:10.3f} | {h:12.3f}")

        # 绘图
        plt.figure(figsize=(6,6), dpi=160)
        plt.plot(orig_smooth[:,0], orig_smooth[:,1], color='black', linewidth=3, label=f"Original ({len(orig)} pts)")
        plt.plot(simp_smooth[:,0], simp_smooth[:,1], linewidth=2, label=f"{n} pts (adaptive)")
        plt.scatter(simp[:,0], simp[:,1], s=18, zorder=5, color='red')

        plt.axis('equal')
        plt.grid(alpha=0.3)
        plt.legend()
        plt.title(f"{n} pts vs Original\nArea err={area_err:.2f}%, Hausdorff={h:.2f} mm")

        plt.tight_layout()
        plt.savefig(OUT_DIR / f"compare_adaptive_{n:03d}pts.png")
        plt.close()

if __name__ == "__main__":
    main()
