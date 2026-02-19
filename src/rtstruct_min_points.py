import numpy as np
import pydicom
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

AREA_TOL = 0.005
HAUSDORFF_TOL = 0.5
MIN_AREA = 1e-3
MIN_POINTS = 3

SPLINE_SMOOTH = 0.2
FIT_DENSE_POINTS = 1200


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
                    "xy": xy,
                    "z": z,
                    "area": area,
                }
            )

    return slices


# ============================================================
# Adaptive resampling
# ============================================================
def discrete_curvature(points: np.ndarray) -> np.ndarray:
    pts = np.vstack([points[-1], points, points[0]])
    v1 = pts[1:-1] - pts[:-2]
    v2 = pts[2:] - pts[1:-1]

    dot = np.sum(v1 * v2, axis=1)
    n1 = np.linalg.norm(v1, axis=1)
    n2 = np.linalg.norm(v2, axis=1)

    cos = dot / (n1 * n2 + 1e-8)
    return np.arccos(np.clip(cos, -1.0, 1.0))


def resample_adaptive_polyline(points: np.ndarray, n: int, alpha: float = 5.0) -> np.ndarray:
    n_total = len(points)
    pts = np.vstack([points, points[0]])

    seg_len = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    curvature = discrete_curvature(points)
    edge_curv = 0.5 * (curvature + np.roll(curvature, -1))

    weight = seg_len * (1 + alpha * edge_curv)
    cum_w = np.concatenate([[0.0], np.cumsum(weight)])
    cum_w /= cum_w[-1]

    targets = np.linspace(0, 1, n, endpoint=False)

    res = []
    for t in targets:
        i = np.searchsorted(cum_w, t) - 1
        i = np.clip(i, 0, n_total - 1)
        local = (t - cum_w[i]) / (cum_w[i + 1] - cum_w[i] + 1e-8)
        res.append((1 - local) * pts[i] + local * pts[i + 1])

    return np.array(res)


def _dedupe_consecutive(points: np.ndarray, tol: float = 1e-8) -> np.ndarray:
    if len(points) <= 1:
        return points
    keep = [0]
    for i in range(1, len(points)):
        if np.linalg.norm(points[i] - points[keep[-1]]) > tol:
            keep.append(i)
    return points[keep]


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


# ============================================================
# Geometry metrics
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

    chunk = 2048
    v = seg_end - seg_start
    vv = np.sum(v * v, axis=1)
    vv = np.where(vv == 0.0, 1e-12, vv)

    for i0 in range(0, n_points, chunk):
        p = points[i0 : i0 + chunk]
        w = p[:, None, :] - seg_start[None, :, :]
        t = np.sum(w * v[None, :, :], axis=2) / vv[None, :]
        t = np.clip(t, 0.0, 1.0)
        proj = seg_start[None, :, :] + t[:, :, None] * v[None, :, :]
        d = np.linalg.norm(p[:, None, :] - proj, axis=2)
        out[i0 : i0 + chunk] = np.min(d, axis=1)

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
# Main flow
# ============================================================
def _best_points_for_slice(points: np.ndarray):
    area_orig = polygon_area(points)
    max_points = max(MIN_POINTS, len(points))

    for n in range(MIN_POINTS, max_points + 1):
        reduced_points = resample_adaptive_polyline(points, n)
        fitted_curve = fit_closed_bspline_curve(
            reduced_points,
            smooth=SPLINE_SMOOTH,
            n_fit=FIT_DENSE_POINTS,
        )
        area_err = abs(polygon_area(fitted_curve) - area_orig) / area_orig
        h = hausdorff_polyline(points, fitted_curve)
        if area_err <= AREA_TOL and h <= HAUSDORFF_TOL:
            return n, area_err, h

    return None, None, None


def main():
    slices = load_all_roi_slices(RTSTRUCT_PATH, ROI_NAME)
    if not slices:
        print("No valid slices found.")
        return

    slices = sorted(slices, key=lambda s: s["z"])

    print(
        "\nidx | z(mm) | area | orig_pts | best_pts | "
        "fit_area_err(%) | fit_hausdorff(mm)"
    )
    print("-" * 86)

    best_list = []
    missing = 0
    total_orig_points = 0

    for s in slices:
        orig_pts = len(s["xy"])
        total_orig_points += orig_pts
        best_n, area_err, h = _best_points_for_slice(s["xy"])

        if best_n is None:
            missing += 1
            print(
                f"{s['index']:3d} | {s['z']:6.2f} | {s['area']:7.2f} |"
                f" {orig_pts:8d} | {'None':>8} | {'--':>15} | {'--':>16}"
            )
        else:
            best_list.append(best_n)
            print(
                f"{s['index']:3d} | {s['z']:6.2f} | {s['area']:7.2f} |"
                f" {orig_pts:8d} | {best_n:8d} | {area_err * 100:15.3f} | {h:16.3f}"
            )

    print("\n==== Summary ====")
    print(f"Total original points (all slices) = {total_orig_points}")

    if missing == 0 and best_list:
        overall_min_points = max(best_list)
        print(
            f"All slices pass thresholds.\n"
            f"Minimal points for this RTSTRUCT = {overall_min_points}\n"
            f"(fit_area_err <= {AREA_TOL * 100:.1f}%, "
            f"Hausdorff <= {HAUSDORFF_TOL:.1f} mm)"
        )
    else:
        print(
            f"Some slices did not meet thresholds.\n"
            f"Passed slices: {len(best_list)} / {len(slices)}"
        )


if __name__ == "__main__":
    main()
