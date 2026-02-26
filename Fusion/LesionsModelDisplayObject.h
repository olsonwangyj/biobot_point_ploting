/******************************************************************************
	LesionsModelDisplayObject.h

	Date      : 28 Nov 2018
******************************************************************************/

#ifndef LESIONS_MODEL_DISPLAY_OBJECT_H
#define LESIONS_MODEL_DISPLAY_OBJECT_H

#include "qvDisplayObject.h"
#include <vtkRenderer.h>
#include <qmap.h>

class SubModelDisplayObject;
class Label2DDisplayObject;
class Label3DDisplayObject;
class ImageStack;
class inurbsSubModel;
class inurbsSurface;

class LesionsModelDisplayObject : qvDisplayObject
{
	Q_OBJECT

public:
	LesionsModelDisplayObject(qvDisplayObject* parent = 0);
	~LesionsModelDisplayObject();

	// submodel functions
	SubModelDisplayObject* CreateSubModelDisplayObject(inurbsSubModel* pSubModel);
	void RemoveSubModelDisplayObject(inurbsSubModel* pSubModel);
	void RemoveAllSubModelDisplayObjects();
	SubModelDisplayObject* GetSubModelDisplayObject(inurbsSubModel* pSubModel);

	// surface functions
	void UpdateSurfaceDisplayObject(int modeId, inurbsSubModel* pSubModel, inurbsSurface* pSurface, ImageStack* pImageStack);
	void SetSurfaceVisibleByPosition(double position, vtkRenderer* renderer = 0);

	// label functions
	Label2DDisplayObject* GetLabel2DDisplayObject(inurbsSubModel* pSubModel);
	Label3DDisplayObject* GetLabel3DDisplayObject(inurbsSubModel* pSubModel);
	void SetLabelsVisibleByPosition(double position, vtkRenderer* renderer);
	void UpdateAllLabelTexts();
	void UpdateCurveLabel(int modelId, inurbsSubModel* pSubModel, ImageStack* pImageStack);

	// curve functions
	void SetCurveVisibleByPosition(inurbsSubModel* pSubModel, double position, vtkRenderer* renderer = 0);
	void SetLesionsColor(inurbsSubModel* pActiveLesion);

protected:
	QMap<inurbsSubModel *, SubModelDisplayObject*>* m_pSubModelDisplayObjects;

};

#endif