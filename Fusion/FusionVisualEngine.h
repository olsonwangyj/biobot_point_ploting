/******************************************************************************
	FusionVisualEngine.h

	Date      : 29 Oct 2018
******************************************************************************/

#ifndef FUSION_VISUAL_ENGINE_H
#define FUSION_VISUAL_ENGINE_H

#include "BaseVisualEngine.h"
#include "qvContext.h"
#include "VolumeDisplayObject.h"
#include "ModelDisplayObject.h"
#include "ImageStack.h"
#include "ModellingTransversalImageContext.h"
#include "AnnotationScaleVTKDisplayObject.h"
#include "AnnotationDisplayObject.h"
#include "SagCorImageContext.h"
#include "VirtualContext.h"

class LesionModellingTransversalImageContext;
class LesionsModelDisplayObject;

class FusionVisualEngine : public BaseVisualEngine  
{
	Q_OBJECT

public:

	static FusionVisualEngine* GetInstance();
	static void DeleteInstance();

	FusionVisualEngine();
	virtual ~FusionVisualEngine();

	// init functions
	void Init();

	// get functions
	LesionModellingTransversalImageContext* GetLesionModellingTransversalImageContext() { return m_pLesionModellingTransversalImageContext;}

	// context functions
	void SetTool(int context, int tool);
	void ResetView(int context);
	void Reset2DViews();
	void Reset3DView();
	void TransversalImageContextViewChanged();
	void Update2DWindows();
	void UpdateWindows();

	// display object functions
	void UpdateSliceZ();

	// import image functions
	void UpdateImageImport();

	// Model related functions
	void AddSurfaceDisplayObject(SurfaceDisplayObject *surfaceDisplayObject);
	void CreateCurveDisplayObjects(int modelId, inurbsSubModel* subModel);

	// lesion related functions
	void CreateLesionSubModelDisplayObject(inurbsSubModel* subModel);
	void DeleteLesionSubModelDisplayObject(inurbsSubModel* pSubModel);
	void CreateLesionCurveDisplayObjects(inurbsSubModel* pSubModel);
	void GotoNextLesion();
	void GotoPrevLesion();
	void ShowActiveLesionCentre();
	void UpdateAllLabelTexts();
	void UpdateLesionSurfaceDisplayObject(inurbsSubModel* pSubModel, inurbsSurface* surface);
	void UpdateLesionCurveLabel(inurbsSubModel* pSubModel);
	void CreateLesionCurveDisplayObject(inurbsSubModel* pSubModel, inurbsPlanarCurve* pCurve);
	void UpdateLesionCurveDisplayObject(inurbsSubModel* pSubModel, inurbsPlanarCurve* pCurve);
	void DeleteLesionCurveDisplayObject(inurbsSubModel* pSubModel, inurbsPlanarCurve* pCurve);
	bool IsTransversalDisplayContainLesionCurve();
	void DeleteLesionDisplayCurve();
	void SetActiveLesion(inurbsSubModel* pSubModel);
	inurbsSubModel* GetActiveLesion();
	void SetProstateModelVisible(bool bVisible); // for lesion stage
	double GetLesionVolume(inurbsSubModel* pSubModel);
	void UpdateLesionInfo(QString sLesionInfo);
	void CleanLesion();

protected:
	static FusionVisualEngine* m_pInstance;

	// contexts
	LesionModellingTransversalImageContext* m_pLesionModellingTransversalImageContext;

	// display objects
	LesionsModelDisplayObject* m_pLesionsModelDisplayObject;

public slots:
	void UpdateState(int state, int subState);
	void ResetTransversalView(TransversalImageContext* pContext);
	void UpdateSlice(int axis);
};

#endif