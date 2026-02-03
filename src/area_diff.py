import numpy as np
import pydicom
import matplotlib.pyplot as plt
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

def load_all_prostate_slices(path):
    ds = pydicom.dcmread(path)

    roi_number = next(
        r.ROINumber
        for r in ds.StructureSetROISequence
        if r.ROIName.lower() == "prostate"
    )

    roi_contour = next(
        rc for rc in ds.ROIContourSequence
        if int(rc.ReferencedROINumber) == int(roi_number)
    )

    slices = []
    for c in roi_contour.ContourSequence:
        pts = np.array(c.ContourData).reshape(-1, 3)
        xy = pts[:, :2]
        z = pts[0, 2]

        area = polygon_area(xy)
        if area > 1e-3:  # 过滤极小噪声
            slices.append({
                "xy": xy,
                "z": z,
                "area": area
            })

    return slices



# ============================================================
# 按面积选择
# ============================================================

def select_examples_by_area(slices):
    slices = sorted(slices, key=lambda s: s["area"])
    areas = np.array([s["area"] for s in slices])

    def pick(q):
        target = np.quantile(areas, q)
        idx = np.argmin(np.abs(areas - target))
        return slices[idx]

    return {
        "small":  pick(0.1),
        "medium": pick(0.6),
        "large":  pick(0.8)
    }



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
# 自适应采样
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
# Chaikin 平滑
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

def _close_polyline(points: np.ndarray) -> np.ndarray:
    points = np.asarray(points, dtype=float)
    if len(points) == 0:
        return points
    if np.allclose(points[0], points[-1]):
        return points
    return np.vstack([points, points[0]])


def _point_to_segments_min_dist(points: np.ndarray, seg_start: np.ndarray, seg_end: np.ndarray) -> np.ndarray:
    """For each point, compute min distance to a set of segments.

    points: (N,2)
    seg_start, seg_end: (M,2)
    returns: (N,) min distance
    """
    points = np.asarray(points, dtype=float)
    seg_start = np.asarray(seg_start, dtype=float)
    seg_end = np.asarray(seg_end, dtype=float)

    # Vectorized point-to-segment distance, done in chunks to limit peak memory.
    n_points = points.shape[0]
    out = np.empty((n_points,), dtype=float)

    # Heuristic chunk size; segments are typically <= 600 here.
    chunk = 2048
    v = seg_end - seg_start  # (M,2)
    vv = np.sum(v * v, axis=1)  # (M,)
    vv = np.where(vv == 0.0, 1e-12, vv)

    for i0 in range(0, n_points, chunk):
        p = points[i0 : i0 + chunk]  # (K,2)
        # Broadcast: (K,M,2)
        w = p[:, None, :] - seg_start[None, :, :]
        t = np.sum(w * v[None, :, :], axis=2) / vv[None, :]
        t = np.clip(t, 0.0, 1.0)
        proj = seg_start[None, :, :] + t[:, :, None] * v[None, :, :]
        d = np.linalg.norm(p[:, None, :] - proj, axis=2)  # (K,M)
        out[i0 : i0 + chunk] = np.min(d, axis=1)

    return out


def hausdorff_polyline(a_points: np.ndarray, b_points: np.ndarray) -> float:
    """Hausdorff distance between two CLOSED polylines (in mm).

    Uses point-to-segment distances rather than point-to-point distances,
    which is much more stable when the two curves have different sampling densities.
    """
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
# 主流程
# ============================================================

def main():
    slices = load_all_prostate_slices(RTSTRUCT_PATH)
    examples = select_examples_by_area(slices)

    for label, ex in examples.items():
        print(f"\nProcessing {label} example | area={ex['area']:.2f}, z={ex['z']:.2f}")

        out_dir = OUT_DIR / f"area_{label}"
        out_dir.mkdir(exist_ok=True)

        orig = ex["xy"]
        area_orig = polygon_area(orig)
        orig_smooth = chaikin(orig, 3)

        print("pts | area_err(%) | hausdorff(mm)")
        print("-" * 34)

        for n in POINT_LIST:
            simp = resample_adaptive_polyline(orig, n)
            simp_smooth = chaikin(simp, 3)

            area_err = abs(polygon_area(simp) - area_orig) / area_orig * 100
            h = hausdorff_polyline(orig, simp)

            print(f"{n:3d} | {area_err:10.3f} | {h:12.3f}")

            plt.figure(figsize=(6, 6), dpi=160)
            orig_plot = _close_polyline(orig_smooth)
            simp_plot = _close_polyline(simp_smooth)
            plt.plot(orig_plot[:,0], orig_plot[:,1],
                     color="black", linewidth=3, label="Original (smooth)")
            plt.plot(simp_plot[:,0], simp_plot[:,1],
                     linewidth=2, label=f"{n} pts (adaptive)")
            plt.scatter(simp[:,0], simp[:,1],
                        s=18, color="red", zorder=5)

            plt.axis("equal")
            plt.grid(alpha=0.3)
            plt.legend()
            plt.title(
                f"{label} area | {n} pts\n"
                f"Area err={area_err:.2f}%, Hausdorff={h:.2f} mm"
            )
            plt.tight_layout()
            plt.savefig(out_dir / f"compare_{n:03d}pts.png")
            plt.close()


if __name__ == "__main__":
    main()
