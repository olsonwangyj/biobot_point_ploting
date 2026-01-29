from pathlib import Path
import pydicom

# 1. 找到一个 RTSTRUCT 文件
rtstruct_path = next(Path(r'd:\point_plotting_reserch').rglob('*.RTSTRUCT*.dcm'))

# 2. 读取 DICOM
ds = pydicom.dcmread(str(rtstruct_path))

# 3. 打印 ROI 列表
print("ROIs in this RTSTRUCT:")
for r in ds.StructureSetROISequence:
    print(f"  ROINumber={r.ROINumber}, ROIName={r.ROIName}")

# 4. 取第一个 ROI 的第一个轮廓
roi_contour = ds.ROIContourSequence[0]
contour = roi_contour.ContourSequence[0]

# 5. 打印关键字段
print("\nContour information:")
print("  ContourGeometricType:", contour.ContourGeometricType)
print("  NumberOfContourPoints:", contour.NumberOfContourPoints)
print("  len(ContourData):", len(contour.ContourData))
