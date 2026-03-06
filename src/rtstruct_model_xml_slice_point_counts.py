from __future__ import annotations

import argparse
import csv
import math
import re
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
import pydicom


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DATASET_ROOT = PROJECT_ROOT / "Siemens testing data results on RTStruct"
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "outputs_model_xml_slice_point_counts"

USE_FUSION_REPOSITIONED_WORLD_ORIGIN = True
FUSION_ORIGIN_Y_OFFSET = 30.0
FUSION_ORIGIN_Z_OFFSET = -100.0
SLICE_Z_TOL = 1e-3


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compare per-slice RTSTRUCT contour point counts against Fusion model.xml point counts."
        )
    )
    parser.add_argument(
        "--dataset-root",
        type=Path,
        default=DEFAULT_DATASET_ROOT,
        help=f"Dataset root. Default: {DEFAULT_DATASET_ROOT}",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Output directory. Default: {DEFAULT_OUTPUT_DIR}",
    )
    return parser.parse_args()


def safe_slug(text: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "_", text.strip())
    return slug.strip("._") or "case"


def _safe_get_float_list(ds, name: str, n: int | None = None):
    if not hasattr(ds, name):
        return None
    values = [float(x) for x in getattr(ds, name)]
    if n is not None and len(values) != n:
        return None
    return values


def polygon_area(points: np.ndarray) -> float:
    pts = np.asarray(points, dtype=float)
    if len(pts) < 3:
        return 0.0
    x = pts[:, 0]
    y = pts[:, 1]
    return float(0.5 * abs(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1))))


def _close_polyline(points: np.ndarray) -> np.ndarray:
    pts = np.asarray(points, dtype=float)
    if len(pts) == 0:
        return pts
    if np.allclose(pts[0], pts[-1]):
        return pts
    return np.vstack([pts, pts[0]])


def _point_to_segments_min_dist(
    points: np.ndarray, seg_start: np.ndarray, seg_end: np.ndarray
) -> np.ndarray:
    points = np.asarray(points, dtype=float)
    seg_start = np.asarray(seg_start, dtype=float)
    seg_end = np.asarray(seg_end, dtype=float)

    if len(points) == 0 or len(seg_start) == 0:
        return np.empty((0,), dtype=float)

    out = np.empty((len(points),), dtype=float)
    seg_vec = seg_end - seg_start
    seg_sq = np.sum(seg_vec * seg_vec, axis=1)
    seg_sq = np.where(seg_sq == 0.0, 1e-12, seg_sq)

    chunk_size = 2048
    for i0 in range(0, len(points), chunk_size):
        chunk = points[i0:i0 + chunk_size]
        rel = chunk[:, None, :] - seg_start[None, :, :]
        t = np.sum(rel * seg_vec[None, :, :], axis=2) / seg_sq[None, :]
        t = np.clip(t, 0.0, 1.0)
        proj = seg_start[None, :, :] + t[:, :, None] * seg_vec[None, :, :]
        dist = np.linalg.norm(chunk[:, None, :] - proj, axis=2)
        out[i0:i0 + chunk_size] = np.min(dist, axis=1)
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

    dist_ab = _point_to_segments_min_dist(a, b_seg0, b_seg1)
    dist_ba = _point_to_segments_min_dist(b, a_seg0, a_seg1)
    return float(max(np.max(dist_ab), np.max(dist_ba)))


def load_rtstruct_rois(rtstruct_path: Path) -> list[dict]:
    ds = pydicom.dcmread(str(rtstruct_path))
    if not hasattr(ds, "StructureSetROISequence") or not hasattr(ds, "ROIContourSequence"):
        raise RuntimeError(f"RTSTRUCT missing ROI sequences: {rtstruct_path}")

    roi_num_to_name: dict[int, str] = {}
    for roi in ds.StructureSetROISequence:
        roi_num_to_name[int(roi.ROINumber)] = str(getattr(roi, "ROIName", f"ROI_{roi.ROINumber}"))

    rois: list[dict] = []
    for roi_index, rc in enumerate(ds.ROIContourSequence):
        roi_number = int(rc.ReferencedROINumber)
        roi_name = roi_num_to_name.get(roi_number, f"ROI_{roi_number}")
        contours = []
        if not hasattr(rc, "ContourSequence"):
            continue
        for contour_index, contour in enumerate(rc.ContourSequence):
            data = np.array(contour.ContourData, dtype=float).reshape(-1, 3)
            if len(data) < 3:
                continue
            ref_uid = ""
            if hasattr(contour, "ContourImageSequence") and len(contour.ContourImageSequence) > 0:
                ref_uid = str(getattr(contour.ContourImageSequence[0], "ReferencedSOPInstanceUID", ""))
            contours.append(
                {
                    "contour_index": contour_index,
                    "points_xyz": data,
                    "ref_sop_uid": ref_uid,
                    "num_points": len(data),
                }
            )
        rois.append(
            {
                "roi_index": roi_index,
                "roi_number": roi_number,
                "roi_name": roi_name,
                "roi_type": "Prostate" if roi_index == 0 else "Lesion",
                "contours": contours,
            }
        )
    return rois


def scan_case_dicoms(case_root: Path) -> list[dict]:
    infos: list[dict] = []
    for dicom_path in case_root.rglob("*.dcm"):
        try:
            ds = pydicom.dcmread(str(dicom_path), stop_before_pixels=True, force=True)
        except Exception:
            continue

        modality = str(getattr(ds, "Modality", ""))
        sop_uid = str(getattr(ds, "SOPInstanceUID", ""))
        if not sop_uid:
            continue

        info = {
            "path": dicom_path,
            "modality": modality,
            "sop_uid": sop_uid,
            "series_uid": str(getattr(ds, "SeriesInstanceUID", "")),
            "instance_number": int(getattr(ds, "InstanceNumber", 0) or 0),
        }
        if modality != "RTSTRUCT":
            iop = _safe_get_float_list(ds, "ImageOrientationPatient", 6)
            ipp = _safe_get_float_list(ds, "ImagePositionPatient", 3)
            pixel_spacing = _safe_get_float_list(ds, "PixelSpacing", 2)
            rows = int(getattr(ds, "Rows", 0) or 0)
            cols = int(getattr(ds, "Columns", 0) or 0)
            if iop and ipp and pixel_spacing and rows > 0 and cols > 0:
                row_dir = np.array(iop[:3], dtype=float)
                col_dir = np.array(iop[3:], dtype=float)
                normal = np.cross(row_dir, col_dir)
                info.update(
                    {
                        "iop": np.array(iop, dtype=float),
                        "ipp": np.array(ipp, dtype=float),
                        "pixel_spacing": np.array(pixel_spacing, dtype=float),
                        "rows": rows,
                        "cols": cols,
                        "slice_thickness": float(getattr(ds, "SliceThickness", 0.0) or 0.0),
                        "spacing_between_slices": float(
                            getattr(ds, "SpacingBetweenSlices", 0.0) or 0.0
                        ),
                        "normal": normal,
                        "slice_pos_along_normal": float(np.dot(np.array(ipp, dtype=float), normal)),
                    }
                )
        infos.append(info)
    return infos


def build_stack_meta(case_root: Path, ref_sop_uids: set[str]) -> dict:
    dicom_infos = scan_case_dicoms(case_root)
    image_infos = [
        info
        for info in dicom_infos
        if info.get("modality") != "RTSTRUCT" and "slice_pos_along_normal" in info
    ]
    if not image_infos:
        raise RuntimeError(f"No image DICOM slices found under {case_root}")

    ref_matches = [info for info in image_infos if info["sop_uid"] in ref_sop_uids]
    if not ref_matches:
        raise RuntimeError(
            f"Could not find any referenced image slices for RTSTRUCT under {case_root}"
        )

    series_counts = Counter(info["series_uid"] for info in ref_matches if info["series_uid"])
    if series_counts:
        selected_series_uid = series_counts.most_common(1)[0][0]
        series_infos = [info for info in image_infos if info["series_uid"] == selected_series_uid]
    else:
        selected_series_uid = ""
        series_infos = ref_matches

    series_infos.sort(key=lambda info: (info["slice_pos_along_normal"], info["instance_number"]))
    uid_to_index = {info["sop_uid"]: index for index, info in enumerate(series_infos)}
    uid_to_info = {info["sop_uid"]: info for info in series_infos}

    missing_refs = sorted(ref_sop_uids.difference(uid_to_info))
    if missing_refs:
        raise RuntimeError(
            f"Selected image series misses {len(missing_refs)} referenced SOPInstanceUID values"
        )

    if len(series_infos) >= 2:
        dz = np.diff([info["slice_pos_along_normal"] for info in series_infos])
        dz = np.array([abs(v) for v in dz if abs(v) > 1e-6], dtype=float)
        if len(dz) > 0:
            slice_spacing = float(np.median(dz))
        else:
            slice_spacing = float(
                series_infos[0]["spacing_between_slices"] or series_infos[0]["slice_thickness"] or 1.0
            )
    else:
        slice_spacing = float(
            series_infos[0]["spacing_between_slices"] or series_infos[0]["slice_thickness"] or 1.0
        )

    spacing_xy = np.array(
        [series_infos[0]["pixel_spacing"][1], series_infos[0]["pixel_spacing"][0]], dtype=float
    )
    width = int(series_infos[0]["cols"])
    height = int(series_infos[0]["rows"])
    num_slices = len(series_infos)

    if USE_FUSION_REPOSITIONED_WORLD_ORIGIN:
        world_origin = np.array(
            [
                -(spacing_xy[0] * width / 2.0),
                -(spacing_xy[1] * height / 2.0) + FUSION_ORIGIN_Y_OFFSET,
                -(slice_spacing * num_slices / 2.0) + FUSION_ORIGIN_Z_OFFSET,
            ],
            dtype=float,
        )
    else:
        world_origin = np.array([0.0, 0.0, 0.0], dtype=float)

    return {
        "case_root": case_root,
        "selected_series_uid": selected_series_uid,
        "slices": series_infos,
        "uid_to_info": uid_to_info,
        "uid_to_index": uid_to_index,
        "num_slices": num_slices,
        "slice_spacing": slice_spacing,
        "world_origin": world_origin,
        "image_width": width,
        "image_height": height,
        "spacing_xy": spacing_xy,
    }


def transform_physical_to_pixel_index(slice_info: dict, point_xyz: np.ndarray) -> np.ndarray:
    point = np.asarray(point_xyz, dtype=float)
    iop = slice_info["iop"]
    ipp = slice_info["ipp"]
    row_dir = iop[:3]
    col_dir = iop[3:]
    row_spacing = float(slice_info["pixel_spacing"][0])
    col_spacing = float(slice_info["pixel_spacing"][1])

    rel = point - ipp
    col = float(np.dot(rel, row_dir) / (col_spacing + 1e-12))
    row = float(np.dot(rel, col_dir) / (row_spacing + 1e-12))
    return np.array([int(np.rint(col)), int(np.rint(row))], dtype=int)


def contour_xyz_to_model_xy_worldz(
    contour_xyz: np.ndarray, ref_uid: str, stack_meta: dict
) -> tuple[np.ndarray, float]:
    if ref_uid not in stack_meta["uid_to_info"]:
        raise KeyError(f"Referenced SOPInstanceUID not found in selected image series: {ref_uid}")

    slice_info = stack_meta["uid_to_info"][ref_uid]
    slice_index = int(stack_meta["uid_to_index"][ref_uid])
    num_slices = int(stack_meta["num_slices"])
    sx, sy = stack_meta["spacing_xy"]
    image_height = int(stack_meta["image_height"])
    world_origin = stack_meta["world_origin"]
    slice_spacing = float(stack_meta["slice_spacing"])

    world_z = (num_slices - slice_index - 1) * slice_spacing + float(world_origin[2])

    points_xy = np.empty((len(contour_xyz), 2), dtype=float)
    for index, point_xyz in enumerate(contour_xyz):
        pixel_index = transform_physical_to_pixel_index(slice_info, point_xyz)
        x = pixel_index[0] * sx + float(world_origin[0])
        y = (image_height - pixel_index[1] - 1) * sy + float(world_origin[1])
        points_xy[index] = [x, y]
    return points_xy, float(world_z)


def parse_point_xy(text: str) -> tuple[float, float]:
    raw = (text or "").strip()
    if not raw:
        raise ValueError("Empty point-x-y text")
    if "," in raw:
        left, right = raw.split(",", 1)
        return float(left), float(right)
    parts = raw.split()
    if len(parts) != 2:
        raise ValueError(f"Unsupported point-x-y format: {raw}")
    return float(parts[0]), float(parts[1])


def load_model_xml(model_xml_path: Path) -> list[dict]:
    root = ET.parse(model_xml_path).getroot()
    submodels = []
    for submodel_el in root.findall("sub-model"):
        submodel = {
            "id": int(submodel_el.findtext("id", default="-1")),
            "type": submodel_el.findtext("type", default=""),
            "curves": [],
        }
        for curve_el in submodel_el.findall("curve"):
            points_el = curve_el.find("points")
            point_values = []
            if points_el is not None:
                for point_el in points_el.findall("point-x-y"):
                    x, y = parse_point_xy(point_el.text or "")
                    point_values.append((x, y))
            points_xy = np.asarray(point_values, dtype=float)
            submodel["curves"].append(
                {
                    "position_z": float(curve_el.findtext("position-z", default="nan")),
                    "points_xy": points_xy,
                    "point_count": len(points_xy),
                    "area": polygon_area(points_xy),
                }
            )
        submodels.append(submodel)
    return submodels


def match_curve_by_z(target_z: float, curves: list[dict], z_tol: float = SLICE_Z_TOL):
    best_curve = None
    best_delta = None
    for curve in curves:
        delta = abs(float(curve["position_z"]) - float(target_z))
        if delta > z_tol:
            continue
        if best_delta is None or delta < best_delta:
            best_curve = curve
            best_delta = delta
    return best_curve, best_delta


def build_roi_slice_stats(rois: list[dict], stack_meta: dict) -> list[dict]:
    roi_stats = []
    for roi in rois:
        slice_groups: dict[float, list[dict]] = defaultdict(list)
        for contour in roi["contours"]:
            ref_uid = contour["ref_sop_uid"]
            if not ref_uid:
                continue
            points_xy, world_z = contour_xyz_to_model_xy_worldz(
                contour["points_xyz"], ref_uid, stack_meta
            )
            z_key = round(world_z, 6)
            slice_groups[z_key].append(
                {
                    "slice_z": float(world_z),
                    "ref_sop_uid": ref_uid,
                    "points_xy": points_xy,
                    "point_count": int(contour["num_points"]),
                    "area": polygon_area(points_xy),
                }
            )

        slices = []
        for z_key in sorted(slice_groups):
            entries = slice_groups[z_key]
            largest_entry = max(entries, key=lambda item: item["point_count"])
            slices.append(
                {
                    "slice_z": float(largest_entry["slice_z"]),
                    "rtstruct_contour_count_on_slice": len(entries),
                    "rtstruct_total_points_on_slice": int(
                        sum(item["point_count"] for item in entries)
                    ),
                    "rtstruct_largest_contour_points": int(largest_entry["point_count"]),
                    "rtstruct_points_xy": largest_entry["points_xy"],
                    "rtstruct_area": float(largest_entry["area"]),
                    "ref_sop_uid": largest_entry["ref_sop_uid"],
                }
            )

        roi_stats.append(
            {
                "roi_index": roi["roi_index"],
                "roi_number": roi["roi_number"],
                "roi_name": roi["roi_name"],
                "roi_type": roi["roi_type"],
                "slices": slices,
            }
        )
    return roi_stats


def build_pair_metrics(roi: dict, submodel: dict) -> dict | None:
    matched_pairs = []
    for slice_stat in roi["slices"]:
        model_curve, z_delta = match_curve_by_z(slice_stat["slice_z"], submodel["curves"])
        if model_curve is None:
            continue
        hd = hausdorff_polyline(slice_stat["rtstruct_points_xy"], model_curve["points_xy"])
        matched_pairs.append(
            {
                "slice_z_rtstruct": float(slice_stat["slice_z"]),
                "slice_z_model": float(model_curve["position_z"]),
                "z_delta": float(z_delta or 0.0),
                "hausdorff_mm": float(hd),
            }
        )

    if not matched_pairs:
        return None

    return {
        "overlap_slices": len(matched_pairs),
        "mean_hausdorff_mm": float(
            np.mean([pair["hausdorff_mm"] for pair in matched_pairs], dtype=float)
        ),
        "matched_pairs": matched_pairs,
    }


def choose_best_assignments(rois: list[dict], submodels: list[dict]) -> dict[int, dict]:
    pair_metrics: dict[tuple[int, int], dict] = {}
    for roi in rois:
        for submodel in submodels:
            metrics = build_pair_metrics(roi, submodel)
            if metrics is not None:
                pair_metrics[(roi["roi_index"], submodel["id"])] = metrics

    best_score = None
    best_assignments: list[tuple[int, int]] = []

    def score_of(total_overlap: int, total_matches: int, total_cost: float):
        return (total_overlap, total_matches, -round(total_cost, 6))

    def dfs(
        model_index: int,
        used_roi_indexes: set[int],
        assignments: list[tuple[int, int]],
        total_overlap: int,
        total_cost: float,
    ) -> None:
        nonlocal best_score, best_assignments

        if model_index >= len(submodels):
            total_matches = len(assignments)
            score = score_of(total_overlap, total_matches, total_cost)
            if best_score is None or score > best_score:
                best_score = score
                best_assignments = list(assignments)
            return

        dfs(model_index + 1, used_roi_indexes, assignments, total_overlap, total_cost)

        submodel = submodels[model_index]
        for roi in rois:
            roi_index = roi["roi_index"]
            if roi_index in used_roi_indexes:
                continue
            metrics = pair_metrics.get((roi_index, submodel["id"]))
            if metrics is None:
                continue
            assignments.append((roi_index, submodel["id"]))
            used_roi_indexes.add(roi_index)
            dfs(
                model_index + 1,
                used_roi_indexes,
                assignments,
                total_overlap + int(metrics["overlap_slices"]),
                total_cost + float(metrics["mean_hausdorff_mm"]),
            )
            used_roi_indexes.remove(roi_index)
            assignments.pop()

    dfs(0, set(), [], 0, 0.0)

    matched: dict[int, dict] = {}
    submodel_by_id = {submodel["id"]: submodel for submodel in submodels}
    for roi_index, submodel_id in best_assignments:
        matched[roi_index] = {
            "submodel": submodel_by_id[submodel_id],
            "metrics": pair_metrics[(roi_index, submodel_id)],
        }
    return matched


def discover_case_pairs(dataset_root: Path) -> list[dict]:
    case_pairs = []
    for case_root in sorted(path for path in dataset_root.iterdir() if path.is_dir()):
        model_xmls = sorted(case_root.rglob("model.xml"))
        rtstruct_files = []
        for dicom_path in case_root.rglob("*.dcm"):
            try:
                ds = pydicom.dcmread(str(dicom_path), stop_before_pixels=True, force=True)
            except Exception:
                continue
            if str(getattr(ds, "Modality", "")) == "RTSTRUCT":
                rtstruct_files.append(dicom_path)

        if not model_xmls or not rtstruct_files:
            continue

        case_pairs.append(
            {
                "case_root": case_root,
                "rtstruct_path": sorted(rtstruct_files)[0],
                "model_xml_path": model_xmls[0],
                "rtstruct_candidates": len(rtstruct_files),
                "model_xml_candidates": len(model_xmls),
            }
        )
    return case_pairs


def process_case(case_info: dict, output_dir: Path) -> tuple[list[dict], dict]:
    case_root = case_info["case_root"]
    rtstruct_path = case_info["rtstruct_path"]
    model_xml_path = case_info["model_xml_path"]

    rois = load_rtstruct_rois(rtstruct_path)
    ref_sop_uids = {
        contour["ref_sop_uid"]
        for roi in rois
        for contour in roi["contours"]
        if contour["ref_sop_uid"]
    }
    stack_meta = build_stack_meta(case_root, ref_sop_uids)
    roi_stats = build_roi_slice_stats(rois, stack_meta)
    model_submodels = load_model_xml(model_xml_path)

    case_rows: list[dict] = []
    matched_roi_to_model: dict[int, dict] = {}
    for roi_type in ("Prostate", "Lesion"):
        rois_of_type = [roi for roi in roi_stats if roi["roi_type"] == roi_type]
        submodels_of_type = [submodel for submodel in model_submodels if submodel["type"] == roi_type]
        matched_roi_to_model.update(choose_best_assignments(rois_of_type, submodels_of_type))

    for roi in roi_stats:
        roi_match = matched_roi_to_model.get(roi["roi_index"])
        matched_submodel = roi_match["submodel"] if roi_match else None
        match_metrics = roi_match["metrics"] if roi_match else None

        for slice_stat in roi["slices"]:
            matched_curve = None
            z_delta = None
            if matched_submodel is not None:
                matched_curve, z_delta = match_curve_by_z(
                    slice_stat["slice_z"], matched_submodel["curves"]
                )

            if matched_submodel is None:
                status = "no_matched_model_submodel"
            elif matched_curve is None:
                status = "matched_submodel_but_slice_missing"
            else:
                status = "matched"

            case_rows.append(
                {
                    "case_name": case_root.name,
                    "case_root": str(case_root),
                    "rtstruct_path": str(rtstruct_path),
                    "model_xml_path": str(model_xml_path),
                    "selected_series_uid": stack_meta["selected_series_uid"],
                    "roi_index": roi["roi_index"],
                    "roi_number": roi["roi_number"],
                    "roi_name": roi["roi_name"],
                    "roi_type": roi["roi_type"],
                    "matched_model_submodel_id": (
                        matched_submodel["id"] if matched_submodel is not None else ""
                    ),
                    "matched_model_submodel_type": (
                        matched_submodel["type"] if matched_submodel is not None else ""
                    ),
                    "match_overlap_slices": (
                        match_metrics["overlap_slices"] if match_metrics is not None else ""
                    ),
                    "match_mean_hausdorff_mm": (
                        f"{match_metrics['mean_hausdorff_mm']:.6f}"
                        if match_metrics is not None
                        else ""
                    ),
                    "slice_z_rtstruct": f"{slice_stat['slice_z']:.6f}",
                    "slice_z_model": (
                        f"{matched_curve['position_z']:.6f}" if matched_curve is not None else ""
                    ),
                    "slice_z_delta": (
                        f"{float(z_delta):.6f}" if z_delta is not None else ""
                    ),
                    "rtstruct_contour_count_on_slice": slice_stat[
                        "rtstruct_contour_count_on_slice"
                    ],
                    "rtstruct_total_points_on_slice": slice_stat[
                        "rtstruct_total_points_on_slice"
                    ],
                    "rtstruct_largest_contour_points": slice_stat[
                        "rtstruct_largest_contour_points"
                    ],
                    "model_point_count": (
                        matched_curve["point_count"] if matched_curve is not None else ""
                    ),
                    "status": status,
                }
            )

    output_dir.mkdir(parents=True, exist_ok=True)
    per_case_csv = output_dir / f"{safe_slug(case_root.name)}_slice_point_counts.csv"
    write_csv(per_case_csv, case_rows)

    summary = {
        "case_name": case_root.name,
        "case_root": str(case_root),
        "rtstruct_path": str(rtstruct_path),
        "model_xml_path": str(model_xml_path),
        "rtstruct_roi_count": len(roi_stats),
        "model_submodel_count": len(model_submodels),
        "selected_series_uid": stack_meta["selected_series_uid"],
        "selected_series_slice_count": stack_meta["num_slices"],
        "selected_series_slice_spacing": f"{stack_meta['slice_spacing']:.6f}",
        "rows_written": len(case_rows),
        "output_csv": str(per_case_csv),
        "rtstruct_candidates": case_info["rtstruct_candidates"],
        "model_xml_candidates": case_info["model_xml_candidates"],
    }
    return case_rows, summary


def write_csv(csv_path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with csv_path.open("w", newline="", encoding="utf-8-sig") as fp:
        writer = csv.DictWriter(fp, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    args = parse_args()
    dataset_root = args.dataset_root.resolve()
    output_dir = args.output_dir.resolve()

    if not dataset_root.exists():
        raise FileNotFoundError(f"Dataset root does not exist: {dataset_root}")

    case_pairs = discover_case_pairs(dataset_root)
    if not case_pairs:
        raise RuntimeError(
            f"No case folders containing both RTSTRUCT and model.xml were found under {dataset_root}"
        )

    all_rows: list[dict] = []
    summaries: list[dict] = []

    print(f"Discovered {len(case_pairs)} paired case(s) under {dataset_root}")
    for case_info in case_pairs:
        case_root = case_info["case_root"]
        print(f"[CASE] {case_root.name}")
        rows, summary = process_case(case_info, output_dir)
        all_rows.extend(rows)
        summaries.append(summary)
        print(
            "  "
            f"rows={summary['rows_written']}, "
            f"series_slices={summary['selected_series_slice_count']}, "
            f"csv={summary['output_csv']}"
        )

    write_csv(output_dir / "all_cases_slice_point_counts.csv", all_rows)
    write_csv(output_dir / "case_summary.csv", summaries)
    print(f"Wrote combined CSV: {output_dir / 'all_cases_slice_point_counts.csv'}")
    print(f"Wrote summary CSV: {output_dir / 'case_summary.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
