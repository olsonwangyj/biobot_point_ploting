import math
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
import pydicom
from scipy.interpolate import splprep, splev


# ============================================================
# Config (edit here)
# ============================================================
RTSTRUCT_PATH = (
    r"D:\point_plotting_reserch\Siemens testing data results on RTStruct\N11780398"
    r"\AIRC Research Prostate MR - RTSTRUCT_NotForClinicalUse"
    r"\_.RTSTRUCT.prostate.3030.0.2025.12.09.07.30.49.960.11930327.dcm"
)

# Root folder containing image DICOM slices referenced by RTSTRUCT contours
IMAGE_ROOT = r"D:\point_plotting_reserch\Siemens testing data results on RTStruct\N11780398"

# Output XML for local testing (always write to project root)
PROJECT_ROOT = Path(__file__).resolve().parent.parent
OUT_XML = PROJECT_ROOT / "outputs_model_xml_test" / "model.xml"

# Sampling mode: "ratio" or "fixed_points"
POINT_POLICY = "ratio"
RATIO = 0.08
FIXED_POINTS = 48
MIN_POINTS = 3
CURVATURE_ALPHA = 5.0

# Optional: export fitted dense curve instead of sampled control polygon
EXPORT_MODE = "adaptive_raw"  # "adaptive_raw" or "adaptive_fit_dense"
SPLINE_SMOOTH = 0.2
FIT_DENSE_POINTS = 200

# Keep only the largest contour on the same slice for the same ROI (Fusion-like behavior)
KEEP_LARGEST_CONTOUR_PER_SLICE = True

# XML style (aligned to sample model.xml)
PROSTATE_XML_ID = 0
LESION_XML_ID_CONSTANT = 4
POINT_XY_TEXT_FORMAT = "comma"  # sample uses "x,y"
DECIMALS = 4
MODEL_FILE_VERSION = "1"
MODEL_TYPE = "NURBS"
DEFAULT_LESION_RISK_SCORE = "5"

# Coordinate mode for local testing
# "fusion_like" uses referenced image SOP + pixel index + y flip + slice index z
# "rtstruct_physical" uses RTSTRUCT physical x/y/z directly
COORD_MODE = "fusion_like"

# Fusion ImportDICOMImages() volume origin re-position (from Fusion/FusionSurgery.cpp)
USE_FUSION_REPOSITIONED_WORLD_ORIGIN = True
FUSION_ORIGIN_Y_OFFSET = 30.0
FUSION_ORIGIN_Z_OFFSET = -100.0


# ============================================================
# Geometry helpers
# ============================================================
def polygon_area(pts: np.ndarray) -> float:
    pts = np.asarray(pts, dtype=float)
    if len(pts) < 3:
        return 0.0
    x, y = pts[:, 0], pts[:, 1]
    return float(0.5 * abs(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1))))


def _close_polyline(points: np.ndarray) -> np.ndarray:
    points = np.asarray(points, dtype=float)
    if len(points) == 0:
        return points
    if np.allclose(points[0], points[-1]):
        return points
    return np.vstack([points, points[0]])


def discrete_curvature(points: np.ndarray) -> np.ndarray:
    pts = np.asarray(points, dtype=float)
    n = len(pts)
    if n < 3:
        return np.zeros((n,), dtype=float)
    wrap = np.vstack([pts[-1], pts, pts[0]])
    v1 = wrap[1:-1] - wrap[:-2]
    v2 = wrap[2:] - wrap[1:-1]
    dot = np.sum(v1 * v2, axis=1)
    n1 = np.linalg.norm(v1, axis=1)
    n2 = np.linalg.norm(v2, axis=1)
    cos = dot / (n1 * n2 + 1e-8)
    return np.arccos(np.clip(cos, -1.0, 1.0))


def resample_adaptive_polyline(points: np.ndarray, n: int, alpha: float = 5.0) -> np.ndarray:
    pts0 = np.asarray(points, dtype=float)
    n = int(max(MIN_POINTS, n))
    if len(pts0) == 0:
        return np.empty((0, 2), dtype=float)
    if len(pts0) == 1:
        return np.repeat(pts0[:1], n, axis=0)

    n_total = len(pts0)
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
        return np.repeat(pts0[:1], n, axis=0)
    cum_w /= total_w

    targets = np.linspace(0.0, 1.0, n, endpoint=False)
    out = np.empty((n, 2), dtype=float)

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


def _dedupe_consecutive(points: np.ndarray, tol: float = 1e-8) -> np.ndarray:
    pts = np.asarray(points, dtype=float)
    if len(pts) <= 1:
        return pts
    keep = [0]
    for i in range(1, len(pts)):
        if np.linalg.norm(pts[i] - pts[keep[-1]]) > tol:
            keep.append(i)
    return pts[keep]


def fit_closed_bspline_curve(points: np.ndarray, smooth: float, n_fit: int) -> np.ndarray:
    pts = _dedupe_consecutive(np.asarray(points, dtype=float))
    if len(pts) < 4:
        return pts
    x = np.r_[pts[:, 0], pts[0, 0]]
    y = np.r_[pts[:, 1], pts[0, 1]]
    k = min(3, len(pts) - 1)
    try:
        tck, _ = splprep([x, y], s=float(smooth), per=True, k=k)
    except Exception:
        return pts
    u = np.linspace(0.0, 1.0, int(max(20, n_fit)), endpoint=False)
    xs, ys = splev(u, tck)
    return np.column_stack([xs, ys])


# ============================================================
# RTSTRUCT parsing
# ============================================================
def load_rtstruct_rois(rtstruct_path: str):
    ds = pydicom.dcmread(rtstruct_path)

    if not hasattr(ds, "StructureSetROISequence") or not hasattr(ds, "ROIContourSequence"):
        raise RuntimeError("RTSTRUCT missing StructureSetROISequence or ROIContourSequence")

    roi_num_to_name = {}
    for roi in ds.StructureSetROISequence:
        roi_num_to_name[int(roi.ROINumber)] = str(getattr(roi, "ROIName", f"ROI_{roi.ROINumber}"))

    rois = []
    for rc in ds.ROIContourSequence:
        roi_num = int(rc.ReferencedROINumber)
        roi_name = roi_num_to_name.get(roi_num, f"ROI_{roi_num}")
        contours = []
        if not hasattr(rc, "ContourSequence"):
            continue
        for ci, contour in enumerate(rc.ContourSequence):
            data = np.array(contour.ContourData, dtype=float).reshape(-1, 3)
            if len(data) < 3:
                continue
            ref_uid = ""
            if hasattr(contour, "ContourImageSequence") and len(contour.ContourImageSequence) > 0:
                ref_uid = str(getattr(contour.ContourImageSequence[0], "ReferencedSOPInstanceUID", ""))
            contours.append(
                {
                    "index": ci,
                    "points_xyz": data,
                    "ref_sop_uid": ref_uid,
                    "num_points": len(data),
                }
            )
        rois.append(
            {
                "roi_number": roi_num,
                "roi_name": roi_name,
                "contours": contours,
            }
        )
    return rois


# ============================================================
# Image geometry (Fusion-like coordinate conversion support)
# ============================================================
def _safe_get_float_list(ds, name, n=None):
    if not hasattr(ds, name):
        return None
    vals = [float(x) for x in getattr(ds, name)]
    if n is not None and len(vals) != n:
        return None
    return vals


def collect_image_slices(image_root: str):
    root = Path(image_root)
    files = list(root.rglob("*.dcm"))
    infos = []
    for fp in files:
        try:
            ds = pydicom.dcmread(str(fp), stop_before_pixels=True, force=True)
        except Exception:
            continue
        modality = str(getattr(ds, "Modality", ""))
        if modality == "RTSTRUCT":
            continue
        sop_uid = str(getattr(ds, "SOPInstanceUID", ""))
        if not sop_uid:
            continue
        iop = _safe_get_float_list(ds, "ImageOrientationPatient", 6)
        ipp = _safe_get_float_list(ds, "ImagePositionPatient", 3)
        px = _safe_get_float_list(ds, "PixelSpacing", 2)
        rows = int(getattr(ds, "Rows", 0) or 0)
        cols = int(getattr(ds, "Columns", 0) or 0)
        if not (iop and ipp and px and rows > 0 and cols > 0):
            continue
        info = {
            "path": str(fp),
            "sop_uid": sop_uid,
            "iop": np.array(iop, dtype=float),
            "ipp": np.array(ipp, dtype=float),
            "pixel_spacing": np.array(px, dtype=float),  # [row, col]
            "rows": rows,
            "cols": cols,
            "instance_number": int(getattr(ds, "InstanceNumber", 0) or 0),
            "slice_thickness": float(getattr(ds, "SliceThickness", 0.0) or 0.0),
            "spacing_between_slices": float(getattr(ds, "SpacingBetweenSlices", 0.0) or 0.0),
        }
        # sort key by projected z along normal
        row_dir = info["iop"][:3]
        col_dir = info["iop"][3:]
        normal = np.cross(row_dir, col_dir)
        info["normal"] = normal
        info["slice_pos_along_normal"] = float(np.dot(info["ipp"], normal))
        infos.append(info)

    if not infos:
        raise RuntimeError(f"No image slices found under {image_root}")

    infos.sort(key=lambda x: (x["slice_pos_along_normal"], x["instance_number"]))

    uid_to_index = {info["sop_uid"]: i for i, info in enumerate(infos)}
    uid_to_info = {info["sop_uid"]: info for info in infos}

    # Estimate slice spacing
    if len(infos) >= 2:
        dz = np.diff([x["slice_pos_along_normal"] for x in infos])
        dz = np.array([abs(v) for v in dz if abs(v) > 1e-6], dtype=float)
        if len(dz) > 0:
            slice_spacing = float(np.median(dz))
        else:
            slice_spacing = float(infos[0]["spacing_between_slices"] or infos[0]["slice_thickness"] or 1.0)
    else:
        slice_spacing = float(infos[0]["spacing_between_slices"] or infos[0]["slice_thickness"] or 1.0)

    spacing_xy = np.array([infos[0]["pixel_spacing"][1], infos[0]["pixel_spacing"][0]], dtype=float)
    width = int(infos[0]["cols"])
    height = int(infos[0]["rows"])
    num_slices = len(infos)

    slice_spacing3 = slice_spacing
    if USE_FUSION_REPOSITIONED_WORLD_ORIGIN:
        world_origin = np.array(
            [
                -(spacing_xy[0] * width / 2.0),
                -(spacing_xy[1] * height / 2.0) + FUSION_ORIGIN_Y_OFFSET,
                -(slice_spacing3 * num_slices / 2.0) + FUSION_ORIGIN_Z_OFFSET,
            ],
            dtype=float,
        )
    else:
        world_origin = np.array([0.0, 0.0, 0.0], dtype=float)

    stack_meta = {
        "slices": infos,
        "uid_to_index": uid_to_index,
        "uid_to_info": uid_to_info,
        "num_slices": num_slices,
        "slice_spacing": slice_spacing,
        "world_origin": world_origin,
        "image_width": width,
        "image_height": height,
        "spacing_xy": spacing_xy,
    }
    return stack_meta


def transform_physical_to_pixel_index(slice_info: dict, p_xyz: np.ndarray) -> np.ndarray:
    p = np.asarray(p_xyz, dtype=float)
    iop = slice_info["iop"]
    ipp = slice_info["ipp"]
    row_dir = iop[:3]
    col_dir = iop[3:]
    row_spacing = float(slice_info["pixel_spacing"][0])  # DICOM PixelSpacing[0]
    col_spacing = float(slice_info["pixel_spacing"][1])  # DICOM PixelSpacing[1]

    v = p - ipp
    # DICOM mapping:
    # P = IPP + col * PixelSpacing[1] * row_dir + row * PixelSpacing[0] * col_dir
    col = float(np.dot(v, row_dir) / (col_spacing + 1e-12))
    row = float(np.dot(v, col_dir) / (row_spacing + 1e-12))

    # Mimic ITK TransformPhysicalPointToIndex (integer index)
    col_idx = int(np.rint(col))
    row_idx = int(np.rint(row))
    return np.array([col_idx, row_idx], dtype=int)


def contour_xyz_to_model_xy_worldz(contour_xyz: np.ndarray, ref_uid: str, stack_meta: dict) -> tuple[np.ndarray, float]:
    pts = np.asarray(contour_xyz, dtype=float)

    if COORD_MODE == "rtstruct_physical":
        xy = pts[:, :2].copy()
        world_z = float(pts[0, 2])
        return xy, world_z

    if not ref_uid or ref_uid not in stack_meta["uid_to_info"]:
        raise KeyError(f"Referenced SOP UID not found in image series: {ref_uid}")

    slice_info = stack_meta["uid_to_info"][ref_uid]
    slice_idx = int(stack_meta["uid_to_index"][ref_uid])
    num_slices = int(stack_meta["num_slices"])
    sx, sy = stack_meta["spacing_xy"]
    h = int(stack_meta["image_height"])
    world_origin = stack_meta["world_origin"]
    slice_spacing = float(stack_meta["slice_spacing"])

    world_z = (num_slices - slice_idx - 1) * slice_spacing + float(world_origin[2])

    out_xy = np.empty((len(pts), 2), dtype=float)
    for i, p in enumerate(pts):
        pix = transform_physical_to_pixel_index(slice_info, p)
        x = pix[0] * sx + float(world_origin[0])
        y = (h - pix[1] - 1) * sy + float(world_origin[1])
        out_xy[i] = [x, y]

    return out_xy, float(world_z)


# ============================================================
# XML writing (best-effort Fusion-like structure)
# ============================================================
def _format_num(v: float) -> str:
    return f"{float(v):.{DECIMALS}f}"


def _xy_text(x: float, y: float) -> str:
    if POINT_XY_TEXT_FORMAT == "comma":
        return f"{_format_num(x)},{_format_num(y)}"
    return f"{_format_num(x)} {_format_num(y)}"


def indent_xml(elem, level=0):
    i = "\n" + level * "  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        for child in elem:
            indent_xml(child, level + 1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


def build_model_xml(submodels: list[dict]) -> ET.ElementTree:
    root = ET.Element("model")
    ET.SubElement(root, "file-version").text = MODEL_FILE_VERSION
    ET.SubElement(root, "type").text = MODEL_TYPE

    for sm in submodels:
        sm_el = ET.SubElement(root, "sub-model")
        ET.SubElement(sm_el, "id").text = str(sm["id"])
        ET.SubElement(sm_el, "type").text = sm["type"]
        if sm["type"] == "Lesion":
            ET.SubElement(sm_el, "risk-score").text = str(sm.get("risk_score", DEFAULT_LESION_RISK_SCORE))

        for curve in sorted(sm["curves"], key=lambda c: c["position_z"]):
            c_el = ET.SubElement(sm_el, "curve")
            ET.SubElement(c_el, "position-z").text = _format_num(curve["position_z"])
            points_el = ET.SubElement(c_el, "points")
            for x, y in curve["points_xy"]:
                ET.SubElement(points_el, "point-x-y").text = _xy_text(x, y)

    indent_xml(root)
    return ET.ElementTree(root)


# ============================================================
# Conversion pipeline (RTSTRUCT -> model-like XML)
# ============================================================
def n_target_from_policy(n_orig: int) -> int:
    if POINT_POLICY == "ratio":
        return max(MIN_POINTS, int(round(n_orig * float(RATIO))))
    if POINT_POLICY == "fixed_points":
        return max(MIN_POINTS, int(FIXED_POINTS))
    raise ValueError(f"Unknown POINT_POLICY: {POINT_POLICY}")


def sample_contour_for_export(xy: np.ndarray) -> np.ndarray:
    n_target = n_target_from_policy(len(xy))
    sampled = resample_adaptive_polyline(xy, n_target, alpha=CURVATURE_ALPHA)
    if EXPORT_MODE == "adaptive_fit_dense":
        return fit_closed_bspline_curve(sampled, smooth=SPLINE_SMOOTH, n_fit=FIT_DENSE_POINTS)
    return sampled


def convert_rtstruct_to_model_xml():
    rois = load_rtstruct_rois(RTSTRUCT_PATH)
    print(f"Loaded RTSTRUCT: {len(rois)} ROIs")

    stack_meta = None
    if COORD_MODE == "fusion_like":
        stack_meta = collect_image_slices(IMAGE_ROOT)
        print(
            f"Loaded image stack: {stack_meta['num_slices']} slices, "
            f"slice_spacing={stack_meta['slice_spacing']:.4f}, "
            f"xy_spacing={stack_meta['spacing_xy'][0]:.4f}/{stack_meta['spacing_xy'][1]:.4f}"
        )
        print(
            "Fusion-like world origin = "
            f"({stack_meta['world_origin'][0]:.4f}, {stack_meta['world_origin'][1]:.4f}, {stack_meta['world_origin'][2]:.4f})"
        )

    submodels = []
    for roi_idx, roi in enumerate(rois):
        roi_name = roi["roi_name"]
        contours = roi["contours"]
        if not contours:
            continue

        if roi_idx == 0:
            sm_id = PROSTATE_XML_ID
            sm_type = "Prostate"
        else:
            sm_id = LESION_XML_ID_CONSTANT + (roi_idx - 1)
            sm_type = "Lesion"

        curve_map = {}
        for contour in contours:
            pts_xyz = contour["points_xyz"]
            ref_uid = contour["ref_sop_uid"]
            try:
                xy, world_z = contour_xyz_to_model_xy_worldz(pts_xyz, ref_uid, stack_meta) if stack_meta else (pts_xyz[:, :2], float(pts_xyz[0, 2]))
            except Exception as e:
                print(f"[WARN] Skip contour ROI={roi_name} idx={contour['index']}: {e}")
                continue

            if len(xy) < 3:
                continue

            sampled_xy = sample_contour_for_export(xy)
            if len(sampled_xy) < 3:
                continue

            area = polygon_area(sampled_xy)
            if area <= 1e-9:
                continue

            # Keep largest contour on same slice (Fusion-like behavior)
            z_key = round(world_z, 6)
            item = {
                "position_z": world_z,
                "points_xy": sampled_xy,
                "num_points": len(sampled_xy),
                "area": area,
                "orig_num_points": len(xy),
            }
            if not KEEP_LARGEST_CONTOUR_PER_SLICE:
                curve_map[(z_key, contour["index"])] = item
            else:
                prev = curve_map.get(z_key)
                if prev is None or item["orig_num_points"] > prev["orig_num_points"]:
                    curve_map[z_key] = item

        curves = list(curve_map.values())
        curves = [c for c in curves if len(c["points_xy"]) >= 3]
        if not curves:
            print(f"[INFO] ROI '{roi_name}' -> no valid curves after conversion")
            continue

        submodels.append(
            {
                "id": sm_id,
                "type": sm_type,
                "roi_name": roi_name,
                "risk_score": DEFAULT_LESION_RISK_SCORE if sm_type == "Lesion" else None,
                "curves": curves,
            }
        )
        print(
            f"ROI[{roi_idx}] '{roi_name}' -> submodel {sm_type}(id={sm_id}), "
            f"curves={len(curves)}"
        )

    if not submodels:
        raise RuntimeError("No submodels/curves generated")

    OUT_XML.parent.mkdir(parents=True, exist_ok=True)
    tree = build_model_xml(submodels)
    tree.write(OUT_XML, encoding="utf-8", xml_declaration=True)
    print(f"\nWrote model XML: {OUT_XML.resolve()}")
    print(
        f"Config: policy={POINT_POLICY}, ratio={RATIO}, fixed_points={FIXED_POINTS}, "
        f"alpha={CURVATURE_ALPHA}, export_mode={EXPORT_MODE}, coord_mode={COORD_MODE}"
    )


if __name__ == "__main__":
    convert_rtstruct_to_model_xml()
