from pathlib import Path
import csv
import numpy as np
import pydicom
import matplotlib.pyplot as plt
from scipy.interpolate import splprep, splev

# ============================================================
# Config
# ============================================================
RTSTRUCT_PATH = (
    r"D:\point_plotting_reserch\Siemens testing data results on RTStruct\N11780398"
    r"\AIRC Research Prostate MR - RTSTRUCT_NotForClinicalUse"
    r"\_.RTSTRUCT.prostate.3030.0.2025.12.09.07.30.49.960.11930327.dcm"
)
ROI_NAME = "Prostate"

RATIO_LIST = [0.03, 0.04, 0.05, 0.06, 0.08]
MIN_POINTS = 3
MIN_AREA = 1e-3

# Curvature-weighted sampling
CURVATURE_ALPHA = 5.0

# Closed B-spline fitting parameters
SPLINE_SMOOTH = 0.2
FIT_DENSE_POINTS = 1200

PROJECT_ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = PROJECT_ROOT / "outputs_ratio_eval_curvature"
OUT_DIR.mkdir(parents=True, exist_ok=True)


# ============================================================
# Geometry helpers
# ============================================================
def polygon_area(pts: np.ndarray) -> float:
    x, y = pts[:, 0], pts[:, 1]
    return float(0.5 * abs(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1))))


def _close_polyline(points: np.ndarray) -> np.ndarray:
    points = np.asarray(points, dtype=float)
    if len(points) == 0:
        return points
    if np.allclose(points[0], points[-1]):
        return points
    return np.vstack([points, points[0]])


def _point_to_segments_min_dist(points: np.ndarray, seg_start: np.ndarray, seg_end: np.ndarray) -> np.ndarray:
    points = np.asarray(points, dtype=float)
    seg_start = np.asarray(seg_start, dtype=float)
    seg_end = np.asarray(seg_end, dtype=float)

    n_points = points.shape[0]
    out = np.empty((n_points,), dtype=float)

    v = seg_end - seg_start
    vv = np.sum(v * v, axis=1)
    vv = np.where(vv == 0.0, 1e-12, vv)

    chunk = 2048
    for i0 in range(0, n_points, chunk):
        p = points[i0:i0 + chunk]
        w = p[:, None, :] - seg_start[None, :, :]
        t = np.sum(w * v[None, :, :], axis=2) / vv[None, :]
        t = np.clip(t, 0.0, 1.0)
        proj = seg_start[None, :, :] + t[:, :, None] * v[None, :, :]
        d = np.linalg.norm(p[:, None, :] - proj, axis=2)
        out[i0:i0 + chunk] = np.min(d, axis=1)

    return out


def hausdorff_polyline(a_points: np.ndarray, b_points: np.ndarray) -> float:
    a = np.asarray(a_points, dtype=float)
    b = np.asarray(b_points, dtype=float)
    if len(a) == 0 or len(b) == 0:
        return float("nan")

    a_closed = _close_polyline(a)
    b_closed = _close_polyline(b)

    a_seg0, a_seg1 = a_closed[:-1], a_closed[1:]
    b_seg0, b_seg1 = b_closed[:-1], b_closed[1:]

    da = _point_to_segments_min_dist(a, b_seg0, b_seg1)
    db = _point_to_segments_min_dist(b, a_seg0, a_seg1)
    return float(max(np.max(da), np.max(db)))


# ============================================================
# Sampling / fitting
# ============================================================
def _dedupe_consecutive(points: np.ndarray, tol: float = 1e-8) -> np.ndarray:
    if len(points) <= 1:
        return points
    keep = [0]
    for i in range(1, len(points)):
        if np.linalg.norm(points[i] - points[keep[-1]]) > tol:
            keep.append(i)
    return points[keep]


def discrete_curvature(points: np.ndarray) -> np.ndarray:
    pts = np.asarray(points, dtype=float)
    if len(pts) < 3:
        return np.zeros((len(pts),), dtype=float)

    wrap = np.vstack([pts[-1], pts, pts[0]])
    v1 = wrap[1:-1] - wrap[:-2]
    v2 = wrap[2:] - wrap[1:-1]

    dot = np.sum(v1 * v2, axis=1)
    n1 = np.linalg.norm(v1, axis=1)
    n2 = np.linalg.norm(v2, axis=1)

    cos = dot / (n1 * n2 + 1e-8)
    return np.arccos(np.clip(cos, -1.0, 1.0))


def resample_closed_curvature_weighted(points: np.ndarray, n_out: int, alpha: float = CURVATURE_ALPHA) -> np.ndarray:
    pts0 = np.asarray(points, dtype=float)
    n_out = int(max(MIN_POINTS, n_out))
    n_total = len(pts0)
    if n_total == 0:
        return np.empty((0, 2), dtype=float)
    if n_total == 1:
        return np.repeat(pts0[:1], n_out, axis=0)

    pts = np.vstack([pts0, pts0[0]])
    seg_len = np.linalg.norm(np.diff(pts, axis=0), axis=1)

    curvature = discrete_curvature(pts0)
    edge_curv = 0.5 * (curvature + np.roll(curvature, -1))

    weight = seg_len * (1.0 + float(alpha) * edge_curv)
    if np.sum(weight) <= 1e-12:
        weight = np.where(seg_len > 0, seg_len, 1.0)

    cum_w = np.concatenate([[0.0], np.cumsum(weight)])
    total_w = cum_w[-1]
    if total_w <= 1e-12:
        return np.repeat(pts0[:1], n_out, axis=0)
    cum_w /= total_w

    targets = np.linspace(0.0, 1.0, n_out, endpoint=False)
    out = np.empty((n_out, 2), dtype=float)

    for i, t in enumerate(targets):
        j = np.searchsorted(cum_w, t, side="right") - 1
        j = int(np.clip(j, 0, n_total - 1))
        denom = cum_w[j + 1] - cum_w[j]
        if denom <= 1e-12:
            out[i] = pts[j]
        else:
            local = (t - cum_w[j]) / denom
            out[i] = (1.0 - local) * pts[j] + local * pts[j + 1]

    return out


def fit_closed_bspline_curve(points: np.ndarray, smooth: float, n_fit: int) -> np.ndarray:
    pts = np.asarray(points, dtype=float)
    pts = _dedupe_consecutive(pts)
    if len(pts) < 4:
        return pts

    x = np.r_[pts[:, 0], pts[0, 0]]
    y = np.r_[pts[:, 1], pts[0, 1]]

    k = min(3, len(pts) - 1)
    try:
        tck, _ = splprep([x, y], s=float(smooth), per=True, k=k)
    except Exception:
        return pts

    u_new = np.linspace(0.0, 1.0, int(max(50, n_fit)), endpoint=False)
    xs, ys = splev(u_new, tck)
    return np.column_stack([xs, ys])


def n_points_by_ratio(n_orig: int, ratio: float) -> int:
    return max(MIN_POINTS, int(round(n_orig * ratio)))


def _safe_name(value: float) -> str:
    return str(value).replace("-", "m").replace(".", "p")


def save_slice_compare_plot(
    out_path: Path,
    slice_index: int,
    z_mm: float,
    ratio: float,
    orig: np.ndarray,
    raw_simp: np.ndarray,
    fit_dense: np.ndarray,
    fit_samples: np.ndarray,
    raw_area_diff: float,
    raw_haus: float,
    fit_area_diff: float,
    fit_haus: float,
) -> None:
    orig_c = _close_polyline(orig)
    raw_c = _close_polyline(raw_simp)
    fit_dense_c = _close_polyline(fit_dense)

    plt.figure(figsize=(6.2, 6.2), dpi=160)
    plt.plot(orig_c[:, 0], orig_c[:, 1], color="black", linewidth=2.4, label="origin")
    plt.plot(raw_c[:, 0], raw_c[:, 1], color="#1f77b4", linewidth=1.8, label="raw(curv)")
    plt.plot(fit_dense_c[:, 0], fit_dense_c[:, 1], color="#d62728", linewidth=2.0, label="fit_curve")
    plt.scatter(raw_simp[:, 0], raw_simp[:, 1], s=9, color="#1f77b4", alpha=0.85)
    plt.scatter(fit_samples[:, 0], fit_samples[:, 1], s=12, color="#d62728", alpha=0.85, label="fit_samples")

    plt.axis("equal")
    plt.grid(alpha=0.25)
    plt.legend(loc="best")
    plt.title(
        f"slice={slice_index}, z={z_mm:.2f} mm, ratio={ratio:.2f}\\n"
        f"raw(curv): area_diff={raw_area_diff*100:.3f}%, haus={raw_haus:.3f} mm | "
        f"fit: area_diff={fit_area_diff*100:.3f}%, haus={fit_haus:.3f} mm"
    )
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


# ============================================================
# RTSTRUCT load
# ============================================================
def load_all_roi_slices(path: str, roi_name: str):
    ds = pydicom.dcmread(path)

    roi_number = next(
        r.ROINumber
        for r in ds.StructureSetROISequence
        if r.ROIName.lower() == roi_name.lower()
    )

    roi_contour = next(
        rc for rc in ds.ROIContourSequence
        if int(rc.ReferencedROINumber) == int(roi_number)
    )

    slices = []
    for idx, c in enumerate(roi_contour.ContourSequence):
        pts = np.array(c.ContourData).reshape(-1, 3)
        xy = pts[:, :2]
        z = float(pts[0, 2])
        area = polygon_area(xy)

        if area > MIN_AREA:
            slices.append(
                {
                    "index": idx,
                    "z": z,
                    "xy": xy,
                    "area": area,
                    "orig_points": len(xy),
                }
            )

    return sorted(slices, key=lambda s: s["z"])


# ============================================================
# Main
# ============================================================
def main():
    slices = load_all_roi_slices(RTSTRUCT_PATH, ROI_NAME)
    if not slices:
        print("No valid slices found.")
        return

    print(f"Loaded {len(slices)} slices for ROI={ROI_NAME}.\\n")
    print(
        f"Sampling: curvature-weighted, alpha={CURVATURE_ALPHA}; "
        f"Curve fitting: periodic B-spline, s={SPLINE_SMOOTH}, dense_points={FIT_DENSE_POINTS}\\n"
    )

    for ratio in RATIO_LIST:
        rows = []
        ratio_tag = f"ratio_{int(round(ratio * 100)):02d}"
        ratio_plot_dir = OUT_DIR / ratio_tag / "slice_plots"
        ratio_plot_dir.mkdir(parents=True, exist_ok=True)

        print(f"=== Ratio {ratio:.2f} ===")
        print("idx | z(mm) | orig_pts | target_pts | raw_area_diff(%) | raw_haus(mm) | fit_area_diff(%) | fit_haus(mm)")
        print("-" * 108)

        for s in slices:
            orig = s["xy"]
            area_orig = s["area"]
            n_target = n_points_by_ratio(s["orig_points"], ratio)

            raw_simp = resample_closed_curvature_weighted(orig, n_target, alpha=CURVATURE_ALPHA)
            raw_area_diff = abs(polygon_area(raw_simp) - area_orig) / area_orig
            raw_haus = hausdorff_polyline(orig, raw_simp)

            fit_samples = raw_simp
            fitted_dense = fit_closed_bspline_curve(
                fit_samples, smooth=SPLINE_SMOOTH, n_fit=FIT_DENSE_POINTS
            )
            fit_area_diff = abs(polygon_area(fitted_dense) - area_orig) / area_orig
            fit_haus = hausdorff_polyline(orig, fitted_dense)

            rows.append(
                {
                    "slice_index": s["index"],
                    "z_mm": s["z"],
                    "orig_points": s["orig_points"],
                    "ratio": ratio,
                    "target_points": n_target,
                    "sampling_method": "curvature_weighted",
                    "curvature_alpha": CURVATURE_ALPHA,
                    "raw_area_diff": raw_area_diff,
                    "raw_hausdorff_mm": raw_haus,
                    "fit_area_diff": fit_area_diff,
                    "fit_hausdorff_mm": fit_haus,
                }
            )

            print(
                f"{s['index']:3d} | {s['z']:6.2f} | {s['orig_points']:8d} | {n_target:10d} |"
                f" {raw_area_diff * 100:15.3f} | {raw_haus:11.3f} |"
                f" {fit_area_diff * 100:15.3f} | {fit_haus:10.3f}"
            )

            plot_name = (
                f"slice_{s['index']:03d}_z_{_safe_name(s['z'])}_"
                f"orig_{s['orig_points']:03d}_target_{n_target:03d}.png"
            )
            save_slice_compare_plot(
                out_path=ratio_plot_dir / plot_name,
                slice_index=s["index"],
                z_mm=s["z"],
                ratio=ratio,
                orig=orig,
                raw_simp=raw_simp,
                fit_dense=fitted_dense,
                fit_samples=fit_samples,
                raw_area_diff=raw_area_diff,
                raw_haus=raw_haus,
                fit_area_diff=fit_area_diff,
                fit_haus=fit_haus,
            )

        csv_path = OUT_DIR / f"{ratio_tag}_slice_metrics.csv"
        with csv_path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)

        raw_area_mean = np.mean([r["raw_area_diff"] for r in rows])
        raw_haus_mean = np.mean([r["raw_hausdorff_mm"] for r in rows])
        fit_area_mean = np.mean([r["fit_area_diff"] for r in rows])
        fit_haus_mean = np.mean([r["fit_hausdorff_mm"] for r in rows])

        print(f"Summary ratio={ratio:.2f}:")
        print(
            f"  RAW(curv) mean area_diff={raw_area_mean * 100:.3f}%, mean hausdorff={raw_haus_mean:.3f} mm\\n"
            f"  FIT mean area_diff={fit_area_mean * 100:.3f}%, mean hausdorff={fit_haus_mean:.3f} mm"
        )
        print(f"  CSV: {csv_path}")
        print(f"  Plots: {ratio_plot_dir}\\n")


if __name__ == "__main__":
    main()
