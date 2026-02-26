/******************************************************************************
    LesionModellingTransversalImageContext.h

    Date      : 26 Nov 2018
 ******************************************************************************/


#ifndef LESION_TRANSVERSAL_IMAGE_CONTEXT_H
#define LESION_TRANSVERSAL_IMAGE_CONTEXT_H

#include "TransversalImageContext.h"

class LesionModelInteractorStyle;
class ModelDisplayObject;
class SurfaceDisplayObject;
class CurveDisplayObject;
class AnnotationDisplayObject;
class inurbsSubModel;

class LesionModellingTransversalImageContext : public TransversalImageContext
{	
	Q_OBJECT

public:
	LesionModellingTransversalImageContext(int context);
	~LesionModellingTransversalImageContext();

	// add entity
	void AddEntity_VolumeDisplayObject(VolumeDisplayObject* volumeDisplayObject, bool bImageTextVisible);
	void AddEntity(ModelDisplayObject* modelDisplayObject);
	void AddEntity(SurfaceDisplayObject* surfaceDisplayObject, VolumeDisplayObject* volumeDisplayObject);
	void AddEntity(CurveDisplayObject* curveDisplayObject);
	void AddEntity(AnnotationScaleVTKDisplayObject* annotationScaleVTKDisplayObject);
	void AddEntity(AnnotationDisplayObject* annotationDisplayObject);

	// set tool
	void SetTool(int id);

	// overwritten functions
	bool GetWindowWorldBounds(double bounds[4]);
	void DisplayToWorld(int x, int y, double &wx, double &wy);

	// other functions
	void SetActiveLesion(inurbsSubModel* pSubModel);
	inurbsSubModel* GetActiveLesion();
	LesionModelInteractorStyle* GetLesionModelInteractorStyle() { return m_pLesionModelInteractorStyle; }

protected:
	LesionModelInteractorStyle* m_pLesionModelInteractorStyle;
};


#endif