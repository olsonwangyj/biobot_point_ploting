# Supplement: Key Algorithm Notes (Simple Explanation)

This is a short explanation of the key algorithms used in the RTSTRUCT contour work.

## 1. Read Contour Points from RTSTRUCT (Data Input)

### Steps

- use `pydicom.dcmread` to read the RTSTRUCT file
- in `StructureSetROISequence`, find the target `ROINumber` by `ROIName`
- in `ROIContourSequence`, find the contour set by `ReferencedROINumber`
- each `ContourSequence` has `ContourData`
- reshape `ContourData` to `(-1, 3)` to get `(x, y, z)` points
- use `xy` as 2D contour points
- use `z` to sort slices
- compute area with `polygon_area`
- remove very small noisy contours (`area < MIN_AREA`)

## 2. Two Evaluation Metrics: Area Error + Hausdorff

## 2.1 Area Error

Area is computed with the **shoelace formula**:

- `dot(x, roll(y)) - dot(y, roll(x))`

This is the standard polygon area formula.
The contour points only need to be in order.

## 2.2 Hausdorff Distance (Point-to-Polyline Version)

I use a **point-to-polyline** Hausdorff distance, not only point-to-point distance.
This is more stable.

### Idea

- treat one contour as many line segments (after `_close_polyline`)
- for each point `p`, compute the shortest distance to all segments
- take the minimum distance for this point
- one-way Hausdorff = max of these minimum distances
- symmetric Hausdorff = `max(h(A, B), h(B, A))`

### Core Math (Point to Segment)

For segment `A -> B` and point `p`, use projection:

```text
t = ((p - A) · (B - A)) / ||B - A||^2
```

Then clip `t` to `[0, 1]` so the closest point stays on the segment.

```text
proj = A + t * (B - A)
```

Distance is:

```text
||p - proj||
```

In code, `_point_to_segments_min_dist` uses vectorization and chunking to avoid high memory use.

## 3. Sampling Method 1: Equal Arc-Length Resampling (Baseline)

In the ratio experiment, `resample_closed_equal_distance` is the baseline.

### Steps

- close the contour
- compute each edge length `seg_len`
- build cumulative length `cum`
- get total perimeter `total`
- choose target positions with `linspace(0, total, n_out)`
- for each target, find which edge it falls on
- use linear interpolation on that edge

This means points are placed uniformly along the polyline perimeter.
The result is `raw_simp`.

## 4. Sampling Method 2: Curvature-Weighted Resampling (Key Optimization)

In another ratio experiment, I upgrade equal arc-length sampling to `resample_closed_curvature_weighted`.

## 4.1 Step 1: Discrete Curvature / Turn Angle

For each vertex `P_i`, use the two edge vectors:

```text
v1 = P_i - P_{i-1}
v2 = P_{i+1} - P_i
```

Compute the angle using dot product:

```text
cos(theta_i) = (v1 · v2) / (||v1|| ||v2||)
theta_i = arccos(...)
```

This is the `discrete_curvature` used in code.

## 4.2 Step 2: Map Vertex Curvature to Edge Curvature

For edge `i`, use the average curvature of its two end vertices:

```text
c_i = (theta_i + theta_{i+1}) / 2
```

## 4.3 Step 3: Build Weighted Edge Length

```text
w_i = L_i * (1 + alpha * c_i)
```

- `L_i` is edge length
- `alpha` controls how strong the extra sampling is at bends

## 4.4 Step 4: Weighted Parameterization with `cum_w`

Normalize cumulative weights to `[0, 1]`:

```text
cum_w[k] = (sum of weights before edge k) / (sum of all weights)
```

Then:

- sample target values uniformly on `[0, 1)`
- use `searchsorted` to find which edge each target belongs to
- interpolate on that edge

### Effect

Edges with larger weight (usually bends / high-curvature regions) take a larger interval in `[0, 1]`, so they get more sampled points.

## 5. Closed B-spline Fitting: From Few-Point Polyline to Smooth Closed Curve

After point compression, I use `splprep(per=True)` for **closed periodic B-spline fitting**.

### Steps

- input control points (from `raw_simp` or curvature-weighted sampled points)
- set `per=True` to enforce periodic fitting
  - the curve is closed
  - the derivative is also continuous at the closing point
- use `s = SPLINE_SMOOTH` to control fit vs smoothness
  - larger `s` means smoother curve but more deviation from control points
- get `tck` (knots + coefficients + degree)
- use `splev` to sample `FIT_DENSE_POINTS` points on the fitted curve
- the result is a dense polyline `fitted_dense`

### Important Evaluation Rule

Area error and Hausdorff are both computed on `fitted_dense`, because this dense curve is the final curve used later, not the control-point polyline.

## 6. Why Use Ratio Scan (How to Explain Results)

I do not only report one minimum `n`.
I scan multiple ratios (3% to 8%).

For each ratio, I output:

- per-slice `raw/fit` metrics
- comparison plots
- CSV files

This makes it easy to see:

- how error changes when compression becomes stronger
- whether curvature-weighted sampling controls Hausdorff better than equal arc-length sampling (especially at bends)
- whether spline smoothing improves or worsens error (depends on `s` and shape details)
