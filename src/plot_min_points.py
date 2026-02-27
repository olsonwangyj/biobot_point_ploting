from pathlib import Path
import numpy as np
import pydicom
import matplotlib.pyplot as plt
from shapely.geometry import Polygon, LinearRing
from shapely.ops import nearest_points
from path_utils import infer_case_root, output_dir


# =========================
# 参数区
# =========================
ROOT_DIR = infer_case_root()
ROI_NAME = "Prostate"

POINT_LIST = [16, 32, 48, 64, 128, 256]

AREA_TOL = 0.01        # 1%
HAUSDORFF_TOL = 0.5    # mm

OUT_DIR = output_dir("outputs")


# =========================
# 读取轮廓
# =========================
def load_roi_contour(root, roi_name):
    rtstruct = next(root.rglob("*.RTSTRUCT*.dcm"))
    ds = pydicom.dcmread(rtstruct)

    roi_number = next(
        r.ROINumber for r in ds.StructureSetROISequence
        if r.ROIName == roi_name
    )

    roi_contour = next(
        rc for rc in ds.ROIContourSequence
        if rc.ReferencedROINumber == roi_number
    )

    contour = roi_contour.ContourSequence[0]
    pts = np.array(contour.ContourData).reshape(-1, 3)

    return pts[:, :2]


# =========================
# 等距重采样
# =========================
def resample(points, n):
    ring = LinearRing(points)
    dists = np.linspace(0, ring.length, n, endpoint=False)
    return np.array([ring.interpolate(d).coords[0] for d in dists])


# =========================
# Hausdorff 距离（mm）
# =========================
def hausdorff(a, b):
    pa, pb = Polygon(a), Polygon(b)
    return max(
        pa.hausdorff_distance(pb),
        pb.hausdorff_distance(pa)
    )


# =========================
# 主流程
# =========================
orig = load_roi_contour(ROOT_DIR, ROI_NAME)
poly_orig = Polygon(orig)
area_orig = poly_orig.area

print("\npts | area_err(%) | hausdorff(mm)")
print("-" * 32)

best_pts = None

for n in POINT_LIST:
    simp = resample(orig, n)
    poly_s = Polygon(simp)

    area_err = abs(poly_s.area - area_orig) / area_orig
    hdist = hausdorff(orig, simp)

    print(f"{n:3d} | {area_err*100:8.3f} | {hdist:10.3f}")

    if best_pts is None and area_err <= AREA_TOL and hdist <= HAUSDORFF_TOL:
        best_pts = n

    # 单独画一张图
    plt.figure(figsize=(5, 5), dpi=160)
    plt.plot(orig[:, 0], orig[:, 1],
             color="black", linewidth=2.5, label=f"Original ({len(orig)} pts)")
    plt.plot(simp[:, 0], simp[:, 1],
             marker="o", markersize=3, linewidth=1.2,
             label=f"{n} pts")

    plt.axis("equal")
    plt.grid(alpha=0.3)
    plt.legend()
    plt.title(
        f"{ROI_NAME}: {n} pts\n"
        f"area_err={area_err*100:.2f}%, hausdorff={hdist:.2f} mm"
    )
    plt.tight_layout()
    plt.savefig(OUT_DIR / f"compare_{n:03d}pts.png")
    plt.close()

print("\n==== Result ====")
if best_pts:
    print(f"Minimal acceptable points: {best_pts}")
else:
    print("No point count meets the given thresholds.")
from pathlib import Path
import numpy as np
import pydicom
import matplotlib.pyplot as plt
from shapely.geometry import Polygon, LinearRing
from shapely.ops import nearest_points


# =========================
# 参数区
# =========================
ROOT_DIR = infer_case_root()
ROI_NAME = "Prostate"

POINT_LIST = [16, 32, 48, 64, 128, 256]

AREA_TOL = 0.01        # 1%
HAUSDORFF_TOL = 0.5    # mm

OUT_DIR = output_dir("outputs")


# =========================
# 读取轮廓
# =========================
def load_roi_contour(root, roi_name):
    rtstruct = next(root.rglob("*.RTSTRUCT*.dcm"))
    ds = pydicom.dcmread(rtstruct)

    roi_number = next(
        r.ROINumber for r in ds.StructureSetROISequence
        if r.ROIName == roi_name
    )

    roi_contour = next(
        rc for rc in ds.ROIContourSequence
        if rc.ReferencedROINumber == roi_number
    )

    contour = roi_contour.ContourSequence[0]
    pts = np.array(contour.ContourData).reshape(-1, 3)

    return pts[:, :2]


# =========================
# 等距重采样
# =========================
def resample(points, n):
    ring = LinearRing(points)
    dists = np.linspace(0, ring.length, n, endpoint=False)
    return np.array([ring.interpolate(d).coords[0] for d in dists])


# =========================
# Hausdorff 距离（mm）
# =========================
def hausdorff(a, b):
    pa, pb = Polygon(a), Polygon(b)
    return max(
        pa.hausdorff_distance(pb),
        pb.hausdorff_distance(pa)
    )


# =========================
# 主流程
# =========================
orig = load_roi_contour(ROOT_DIR, ROI_NAME)
poly_orig = Polygon(orig)
area_orig = poly_orig.area

print("\npts | area_err(%) | hausdorff(mm)")
print("-" * 32)

best_pts = None

for n in POINT_LIST:
    simp = resample(orig, n)
    poly_s = Polygon(simp)

    area_err = abs(poly_s.area - area_orig) / area_orig
    hdist = hausdorff(orig, simp)

    print(f"{n:3d} | {area_err*100:8.3f} | {hdist:10.3f}")

    if best_pts is None and area_err <= AREA_TOL and hdist <= HAUSDORFF_TOL:
        best_pts = n

    # 单独画一张图
    plt.figure(figsize=(5, 5), dpi=160)
    plt.plot(orig[:, 0], orig[:, 1],
             color="black", linewidth=2.5, label=f"Original ({len(orig)} pts)")
    plt.plot(simp[:, 0], simp[:, 1],
             marker="o", markersize=3, linewidth=1.2,
             label=f"{n} pts")

    plt.axis("equal")
    plt.grid(alpha=0.3)
    plt.legend()
    plt.title(
        f"{ROI_NAME}: {n} pts\n"
        f"area_err={area_err*100:.2f}%, hausdorff={hdist:.2f} mm"
    )
    plt.tight_layout()
    plt.savefig(OUT_DIR / f"compare_{n:03d}pts.png")
    plt.close()

print("\n==== Result ====")
if best_pts:
    print(f"Minimal acceptable points: {best_pts}")
else:
    print("No point count meets the given thresholds.")
