from pathlib import Path
import numpy as np
import pydicom
import matplotlib.pyplot as plt
from shapely.geometry import Polygon, LinearRing
from scipy.interpolate import splprep, splev
from path_utils import infer_case_root, output_dir


# ==================================================
# 参数区（只改这里）
# ==================================================
ROOT_DIR = infer_case_root()
ROI_NAME = "Prostate"

POINT_LIST = [16, 32, 48, 64, 128, 256]

AREA_TOL = 0.01       # 1 %
HAUSDORFF_TOL = 0.5   # mm

OUT_DIR = output_dir("outputs")


# ==================================================
# 读取 RTSTRUCT 中的轮廓
# ==================================================
def load_roi_contour(root: Path, roi_name: str) -> np.ndarray:
    rtstruct = next(root.rglob("*.RTSTRUCT*.dcm"))
    ds = pydicom.dcmread(rtstruct)

    roi_number = next(
        r.ROINumber
        for r in ds.StructureSetROISequence
        if r.ROIName == roi_name
    )

    roi_contour = next(
        rc for rc in ds.ROIContourSequence
        if rc.ReferencedROINumber == roi_number
    )

    contour = roi_contour.ContourSequence[0]
    pts = np.array(contour.ContourData).reshape(-1, 3)

    return pts[:, :2]   # CLOSED_PLANAR → XY


# ==================================================
# 等距重采样（真实几何）
# ==================================================
def resample_equal_distance(points: np.ndarray, n: int) -> np.ndarray:
    ring = LinearRing(points)
    distances = np.linspace(0, ring.length, n, endpoint=False)
    return np.array([ring.interpolate(d).coords[0] for d in distances])


# ==================================================
# B-spline 平滑（仅用于显示）
# ==================================================
def smooth_closed_curve(points: np.ndarray,
                        num: int = 500,
                        smooth: float = 0.0) -> np.ndarray:
    x, y = points[:, 0], points[:, 1]

    x = np.r_[x, x[0]]
    y = np.r_[y, y[0]]

    tck, _ = splprep([x, y], s=smooth, per=True)
    u = np.linspace(0, 1, num)
    xs, ys = splev(u, tck)

    return np.column_stack([xs, ys])


# ==================================================
# 主流程
# ==================================================
orig = load_roi_contour(ROOT_DIR, ROI_NAME)

# 严格几何（用于计算）
poly_orig = Polygon(orig)
area_orig = poly_orig.area

# 平滑显示用
orig_smooth = smooth_closed_curve(orig)

print("\npts | area_err(%) | hausdorff(mm)")
print("-" * 34)

best_pts = None

for n in POINT_LIST:
    # ---------- 简化（折线，用于计算） ----------
    simp = resample_equal_distance(orig, n)
    poly_s = Polygon(simp)

    area_err = abs(poly_s.area - area_orig) / area_orig
    hausdorff = max(
        poly_orig.hausdorff_distance(poly_s),
        poly_s.hausdorff_distance(poly_orig)
    )

    print(f"{n:3d} | {area_err*100:8.3f} | {hausdorff:12.3f}")

    if best_pts is None and area_err <= AREA_TOL and hausdorff <= HAUSDORFF_TOL:
        best_pts = n

    # ---------- 平滑显示 ----------
    simp_smooth = smooth_closed_curve(simp)

    # ---------- 画图 ----------
    plt.figure(figsize=(5.5, 5.5), dpi=160)

    # 原始（平滑）
    plt.plot(
        orig_smooth[:, 0], orig_smooth[:, 1],
        color="black", linewidth=2.8,
        label="Original (smooth)"
    )

    # 简化（平滑）
    plt.plot(
        simp_smooth[:, 0], simp_smooth[:, 1],
        linewidth=2.0,
        label=f"{n} pts (smooth)"
    )

    # 简化（真实点）
    plt.scatter(
        simp[:, 0], simp[:, 1],
        s=18, zorder=5,
        label=f"{n} pts (samples)"
    )

    plt.axis("equal")
    plt.grid(alpha=0.3)
    plt.legend()
    plt.title(
        f"{ROI_NAME} – {n} pts\n"
        f"area_err={area_err*100:.2f}%, "
        f"Hausdorff={hausdorff:.2f} mm"
    )
    plt.tight_layout()
    plt.savefig(OUT_DIR / f"compare_{n:03d}pts.png")
    plt.close()

print("\n==== Result ====")
if best_pts is not None:
    print(
        f"Minimal acceptable points = {best_pts}\n"
        f"(area_err ≤ {AREA_TOL*100:.1f}%, "
        f"Hausdorff ≤ {HAUSDORFF_TOL:.1f} mm)"
    )
else:
    print("No point count meets the given thresholds.")
