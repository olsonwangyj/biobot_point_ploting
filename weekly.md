# Weekly Report (2026-02-24 ~ 2026-03-02)

This week I focused on `src/rtstruct_to_model_xml_adaptive.py`: converting RTSTRUCT ROI contours into a Fusion-like `model.xml`, and adaptively reducing the number of points per contour while keeping shape error under control.

## 1. Goals

- Read closed contour points per ROI slice from RTSTRUCT
- Convert contour points into a Fusion-like coordinate system (to align with the downstream model import logic)
- Compress points for each contour (reduce/control point count) while constraining shape error with interpretable metrics
- Output `outputs_model_xml_test/model.xml` for local import/inspection

## 2. Key Implementation Notes (`rtstruct_to_model_xml_adaptive.py`)

### 2.1 Three point-count policies (`POINT_POLICY`)

- `ratio`: keep a ratio of points (`n = round(n_orig * RATIO)`)
- `fixed_points`: fixed number of points (`FIXED_POINTS`)
- `metric_threshold`: automatically search for the minimum `n` that satisfies metric thresholds (main focus this week)

### 2.2 Metric-threshold search (`metric_threshold`)

For each contour, increase `n` starting from `n = MIN_POINTS`:

- Use curvature-weighted adaptive resampling to generate `n` points (denser around bends)
- Compute, against the original contour:
  - relative area error `AREA_DIFF_TOL`
  - Hausdorff distance (mm) `HAUSDORFF_TOL`
- Return the first `n` that satisfies both thresholds
- If no `n` satisfies the thresholds, fall back based on `METRIC_NO_MATCH_STRATEGY` (ratio / fixed / max_points)

### 2.3 Optional exported curve form (`EXPORT_MODE`)

- `adaptive_raw`: export the sampled control polyline directly
- `adaptive_fit_dense`: sample control points first, then fit a closed B-spline and export a dense curve (for smoother contours)

### 2.4 Fusion-like coordinate conversion (`COORD_MODE="fusion_like"`)

- Locate the corresponding slice via the contour’s `ReferencedSOPInstanceUID`
- Use DICOM `IOP/IPP/PixelSpacing` to map physical coordinates to pixel indices, then to Fusion-like world coordinates
- Use a Fusion-like repositioned world origin (with `FUSION_ORIGIN_*_OFFSET`)

## 3. Results (two threshold settings)

The generated shapes under the two settings are very similar. I am still tuning the algorithm details and threshold values, and need to validate stability across more ROIs/slices.

### 3.1 Setting A

- `POINT_POLICY = "metric_threshold"`
- `AREA_DIFF_TOL = 0.1`
- `HAUSDORFF_TOL = 0.8`

![metric_threshold (AREA_DIFF_TOL=0.1, HAUSDORFF_TOL=0.8)](image.png)

### 3.2 Setting B

- `POINT_POLICY = "metric_threshold"`
- `AREA_DIFF_TOL = 0.2`
- `HAUSDORFF_TOL = 1.0`

![metric_threshold (AREA_DIFF_TOL=0.2, HAUSDORFF_TOL=1.0)](<image copy.png>)

## 4. Next Week Plan

- Expand to more cases (different ROIs and shape complexities) and summarize the distribution of “minimum `n` that meets thresholds”
- Systematically sweep `AREA_DIFF_TOL / HAUSDORFF_TOL / CURVATURE_ALPHA / SPLINE_SMOOTH` to find a more robust default parameter set
