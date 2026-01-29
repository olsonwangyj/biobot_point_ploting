import numpy as np
import pydicom
import matplotlib.pyplot as plt
from scipy.spatial.distance import cdist
from pathlib import Path

# ============================================================
# 配置
# ============================================================

RTSTRUCT_PATH = r"D:\point_plotting_reserch\Siemens testing data results on RTStruct\N11780398\AIRC Research Prostate MR - RTSTRUCT_NotForClinicalUse\_.RTSTRUCT.prostate.3030.0.2025.12.09.07.30.49.960.11930327.dcm"

POINT_LIST = [16, 32, 48, 64, 128, 256]
OUT_DIR = Path("output_adaptive_smooth")
OUT_DIR.mkdir(exist_ok=True)

# ============================================================
# 读取 RTSTRUCT（XY）
# ============================================================

def load_prostate_contour_xy(path):
    ds = pydicom.dcmread(path)

    roi_number = next(
        r.ROINumber
        for r in ds.StructureSetROISequence
        if r.ROIName.lower() == "prostate"
    )

    roi_contour = next(
        rc for rc in ds.ROIContourSequence
        if rc.ReferencedROINumber == roi_number
    )

    pts = np.array(
        roi_contour.ContourSequence[0].ContourData
    ).reshape(-1, 3)

    return pts[:, :2]

# ============================================================
# 离散曲率（顶点转角）
# ============================================================

def discrete_curvature(points):
    pts = np.vstack([points[-1], points, points[0]])
    v1 = pts[1:-1] - pts[:-2]
    v2 = pts[2:]   - pts[1:-1]

    dot = np.sum(v1 * v2, axis=1)
    n1 = np.linalg.norm(v1, axis=1)
    n2 = np.linalg.norm(v2, axis=1)

    cos = dot / (n1 * n2 + 1e-8)
    return np.arccos(np.clip(cos, -1.0, 1.0))

# ============================================================
# 自适应采样（正确维度版）
# ============================================================

def resample_adaptive_polyline(points, n, alpha=5.0):
    N = len(points)

    # 闭环
    pts = np.vstack([points, points[0]])

    # 边长（N 条）
    seg_len = np.linalg.norm(np.diff(pts, axis=0), axis=1)

    # 顶点曲率（N 个）
    curvature = discrete_curvature(points)

    # 映射到边
    edge_curv = 0.5 * (curvature + np.roll(curvature, -1))

    # 权重（N 条边）
    weight = seg_len * (1 + alpha * edge_curv)

    cum_w = np.concatenate([[0.0], np.cumsum(weight)])
    cum_w /= cum_w[-1]

    targets = np.linspace(0, 1, n, endpoint=False)

    res = []
    for t in targets:
        i = np.searchsorted(cum_w, t) - 1
        i = np.clip(i, 0, N - 1)
        local = (t - cum_w[i]) / (cum_w[i+1] - cum_w[i] + 1e-8)
        res.append((1 - local) * pts[i] + local * pts[i+1])

    return np.array(res)

# ============================================================
# Chaikin 平滑（局部、不炸 Hausdorff）
# ============================================================

def chaikin(points, n_iter=3):
    pts = points.copy()
    for _ in range(n_iter):
        new_pts = []
        for i in range(len(pts)):
            p0 = pts[i]
            p1 = pts[(i + 1) % len(pts)]
            new_pts.append(0.75 * p0 + 0.25 * p1)
            new_pts.append(0.25 * p0 + 0.75 * p1)
        pts = np.array(new_pts)
    return pts

# ============================================================
# 几何分析
# ============================================================

def polygon_area(pts):
    x, y = pts[:, 0], pts[:, 1]
    return 0.5 * abs(
        np.dot(x, np.roll(y, -1)) -
        np.dot(y, np.roll(x, -1))
    )

def hausdorff(a, b):
    d = cdist(a, b)
    return max(d.min(axis=1).max(), d.min(axis=0).max())

# ============================================================
# 主流程
# ============================================================

def main():
    orig = load_prostate_contour_xy(RTSTRUCT_PATH)
    area_orig = polygon_area(orig)
    orig_smooth = chaikin(orig, 3)

    print("\npts | area_err(%) | hausdorff(mm)")
    print("-" * 34)

    for n in POINT_LIST:
        simp = resample_adaptive_polyline(orig, n)
        simp_smooth = chaikin(simp, 3)

        area_err = abs(polygon_area(simp) - area_orig) / area_orig * 100
        h = hausdorff(orig, simp)

        print(f"{n:3d} | {area_err:10.3f} | {h:12.3f}")

        plt.figure(figsize=(6, 6), dpi=160)
        plt.plot(orig_smooth[:,0], orig_smooth[:,1],
                 color="black", linewidth=3, label="Original (smooth)")
        plt.plot(simp_smooth[:,0], simp_smooth[:,1],
                 linewidth=2, label=f"{n} pts (adaptive)")
        plt.scatter(simp[:,0], simp[:,1],
                    s=18, color="red", zorder=5)

        plt.axis("equal")
        plt.grid(alpha=0.3)
        plt.legend()
        plt.title(
            f"{n} pts\nArea err={area_err:.2f}%, Hausdorff={h:.2f} mm"
        )
        plt.tight_layout()
        plt.savefig(OUT_DIR / f"compare_{n:03d}pts.png")
        plt.close()

if __name__ == "__main__":
    main()
