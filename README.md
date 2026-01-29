# point_plotting_reserch

This workspace contains Siemens MR DICOM series and RTSTRUCT (RT Structure Set) examples.

## What the RTSTRUCT stores

For this dataset, the RTSTRUCT contains `ContourSequence` items with `ContourGeometricType = CLOSED_PLANAR` and `ContourData` (x,y,z in patient coordinates, millimeters). These are **boundary contour points**, not a full interior point cloud.

## Scripts

- `src/analyze_rtstruct.py`: extract contours for an ROI, simplify boundary points (RDP), and compute boundary point density stats.
- `src/geom_utils.py`: geometry helpers (RDP simplification, polygon area, density stats).
- `src/plot_simplification_compare.py`: draw original vs simplified contours for multiple target point counts (easy visual comparison).
- `src/plot_density_compare.py`: visualize where boundary points are dense/sparse (local spacing), and compare with a simplified contour.

## Quick start

Create a venv and install dependencies:

```bash
pip install -r requirements.txt
```

Run analysis for one patient:

```bash
python src/analyze_rtstruct.py --patient N11830957 --roi "Prostate" --out outputs
```

Try a fixed simplification tolerance (in mm):

```bash
python src/analyze_rtstruct.py --patient N11830957 --roi "Prostate" --epsilon 2.0 --out outputs_eps2
```

Compare different target point counts (overlay plots in patient XY space):

```bash
python src/plot_simplification_compare.py --patient N11830957 --roi "Prostate" --points 20,40,80,160 --out outputs
```

Visualize boundary density (dense vs sparse) and compare to simplified:

```bash
python src/plot_density_compare.py --patient N11830957 --roi "Prostate" --simplify-max-points 80 --out outputs
```

Outputs:

- `outputs/summary.json`: per-slice metrics and density stats.
