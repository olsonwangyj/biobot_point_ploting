/******************************************************************************
	LesionsModelDisplayObject.cpp

	Date      : 28 Nov 2018
******************************************************************************/

#include "Application.h"
#include "LesionsModelDisplayObject.h"
#include "SubModelDisplayObject.h"
#include "SurfaceDisplayObject.h"
#include "CurveDisplayObject.h"
#include "Label2DDisplayObject.h"
#include "Label3DDisplayObject.h"
#include "inurbsSubModel.h"

/******************************************************************************/
/* Constructors and Destructors                                                                            
/******************************************************************************/

LesionsModelDisplayObject::LesionsModelDisplayObject(qvDisplayObject* parent)
  : qvDisplayObject(parent)
{
	m_pSubModelDisplayObjects = new QMap<inurbsSubModel *, SubModelDisplayObject*>();
}

LesionsModelDisplayObject::~LesionsModelDisplayObject()
{
	RemoveAllSubModelDisplayObjects();
	delete m_pSubModelDisplayObjects;
}

/******************************************************************************/
/* Submodel display object functions                                                                            
/******************************************************************************/

void LesionsModelDisplayObject::RemoveSubModelDisplayObject(inurbsSubModel* pSubModel)
{
	SubModelDisplayObject* subModelDisplayObject = m_pSubModelDisplayObjects->value(pSubModel);

	if (subModelDisplayObject != 0)
	{
		m_pSubModelDisplayObjects->remove(pSubModel);
		delete subModelDisplayObject;
	}
}

void LesionsModelDisplayObject::RemoveAllSubModelDisplayObjects()
{
	QMutableMapIterator<inurbsSubModel*, SubModelDisplayObject*> it(*m_pSubModelDisplayObjects);
	while (it.hasNext())
	{
		SubModelDisplayObject* subModelDisplayObject = it.next().value();
		it.remove();
		delete subModelDisplayObject;
	}
}

SubModelDisplayObject* LesionsModelDisplayObject::CreateSubModelDisplayObject(inurbsSubModel* pSubModel)
{
	// remove if exist
	RemoveSubModelDisplayObject(pSubModel);

	// create new 
	SubModelDisplayObject* subModelDisplayObject = new SubModelDisplayObject(this);
	m_pSubModelDisplayObjects->insert(pSubModel, subModelDisplayObject);
	subModelDisplayObject->SetSubModel(pSubModel);

	subModelDisplayObject->GetSurfaceDisplayObject()->SetSurfaceColor(1.0,0.0,0.0); // set to red

	// create 2D label display 
	Label2DDisplayObject* label2DDisplayObject = subModelDisplayObject->CreateLabel2D();
	label2DDisplayObject->GetLabelPipeline()->setScale(2.0);
	label2DDisplayObject->GetLabelPipeline()->setText("");

	// creaet 3D label display
	Label3DDisplayObject* label3DDisplayObject = subModelDisplayObject->CreateLabel3D();
	label2DDisplayObject->GetLabelPipeline()->setScale(2.0);
	label3DDisplayObject->GetLabelPipeline()->setText("");

	return subModelDisplayObject;
}

SubModelDisplayObject* LesionsModelDisplayObject::GetSubModelDisplayObject(inurbsSubModel *pSubModel)
{
	return m_pSubModelDisplayObjects->value(pSubModel);	
}

/******************************************************************************/
/*  Surface functions
/******************************************************************************/

void LesionsModelDisplayObject::UpdateSurfaceDisplayObject(int modelId, inurbsSubModel* pSubModel, inurbsSurface* pSurface, ImageStack* pImageStack)
{
	// get sub model display object
	SubModelDisplayObject *subModelDisplayObject = GetSubModelDisplayObject(pSubModel);

	// get surface display object
	SurfaceDisplayObject* surfaceDisplayObject = subModelDisplayObject->GetSurfaceDisplayObject();

	if (surfaceDisplayObject)
	{
		surfaceDisplayObject->SetSurface(pSurface);
		surfaceDisplayObject->UpdateSurface();
	}

	// update 2D label display object
	Label2DDisplayObject* label2DDisplayObject = subModelDisplayObject->GetLabel2DDisplayObject();
	label2DDisplayObject->CalculatePosition(pSubModel,pImageStack);
	label2DDisplayObject->SetPosition();
	QString text3 = QString("L%1").arg(modelId+1);
	label2DDisplayObject->GetLabelPipeline()->setText(text3.toLatin1());

	// update 3D label display object
	Label3DDisplayObject* label3DDisplayObject = subModelDisplayObject->GetLabel3DDisplayObject();
	label3DDisplayObject->CalculatePosition(surfaceDisplayObject->GetSurfacePolyData());
	label3DDisplayObject->SetPosition();
	label3DDisplayObject->GetLabelPipeline()->setText(text3.toLatin1());
}

void LesionsModelDisplayObject::SetSurfaceVisibleByPosition(double position, vtkRenderer* renderer)
{
	QMapIterator<inurbsSubModel *, SubModelDisplayObject*> its(*m_pSubModelDisplayObjects);
	while(its.hasNext())
	{
		its.next();
		its.value()->SetSurfaceVisibleByPosition(position, renderer);
	}
}

/******************************************************************************/
/*  Label functions
/******************************************************************************/

Label2DDisplayObject* LesionsModelDisplayObject::GetLabel2DDisplayObject(inurbsSubModel* pSubModel)
{
	SubModelDisplayObject* pSubModelDisplayObject = GetSubModelDisplayObject(pSubModel);
	if (!pSubModelDisplayObject)
		return NULL;

	return pSubModelDisplayObject->GetLabel2DDisplayObject();
}

Label3DDisplayObject* LesionsModelDisplayObject::GetLabel3DDisplayObject(inurbsSubModel* pSubModel)
{
	SubModelDisplayObject* pSubModelDisplayObject = GetSubModelDisplayObject(pSubModel);
	if (!pSubModelDisplayObject)
		return NULL;

	return pSubModelDisplayObject->GetLabel3DDisplayObject();
}

void LesionsModelDisplayObject::SetLabelsVisibleByPosition(double position, vtkRenderer* renderer)
{
	QMutableMapIterator<inurbsSubModel*, SubModelDisplayObject*> it(*m_pSubModelDisplayObjects);
	while (it.hasNext())
	{
		it.next();
		inurbsSubModel* pSubModel = it.key();
		SubModelDisplayObject* subModelDisplayObject = GetSubModelDisplayObject(pSubModel);
		if (!subModelDisplayObject)
			continue;

		subModelDisplayObject->SetLabelVisibleByPosition(position, renderer);
	}
}

void LesionsModelDisplayObject::UpdateAllLabelTexts()
{
	// need to update label texts
	QMutableMapIterator<inurbsSubModel*, SubModelDisplayObject*> it(*m_pSubModelDisplayObjects);
	while (it.hasNext())
	{
		it.next();
		inurbsSubModel* pSubModel = it.key();
		int id = pSubModel->GetID();

		SubModelDisplayObject* subModelDisplayObject = GetSubModelDisplayObject(pSubModel);
		if (!subModelDisplayObject)
			continue;

		Label2DDisplayObject* label2DDisplayObject = subModelDisplayObject->GetLabel2DDisplayObject();
		if (!label2DDisplayObject)
			continue;

		QString text3 = QString("L%1").arg(id+1);
		label2DDisplayObject->GetLabelPipeline()->setText(text3.toLatin1());

		Label3DDisplayObject* label3DDisplayObject = subModelDisplayObject->GetLabel3DDisplayObject();
		label3DDisplayObject->GetLabelPipeline()->setText(text3.toLatin1());
	}
}

void LesionsModelDisplayObject::UpdateCurveLabel(int modelId, inurbsSubModel* pSubModel, ImageStack* pImageStack)
{
	// get sub model display object
	SubModelDisplayObject *subModelDisplayObject = GetSubModelDisplayObject(pSubModel);

	CurveDisplayObject *pCurveDisplayObject = subModelDisplayObject->GetCurveDisplayObject(pSubModel->GetCurve(0)); // get first curve

	// update 2D label display object
	Label2DDisplayObject* label2DDisplayObject = subModelDisplayObject->GetLabel2DDisplayObject();
	label2DDisplayObject->CalculatePosition(pCurveDisplayObject->GetCurvePipeline()->getPolyData(), pImageStack);
	label2DDisplayObject->SetPosition();
	QString text3 = QString("L%1").arg(modelId + 1);
	label2DDisplayObject->GetLabelPipeline()->setText(text3.toLatin1());

	// update 3D label display object
	Label3DDisplayObject* label3DDisplayObject = subModelDisplayObject->GetLabel3DDisplayObject();
	label3DDisplayObject->CalculatePosition(pCurveDisplayObject->GetCurvePipeline()->getPolyData());
	label3DDisplayObject->SetPosition();
	label3DDisplayObject->GetLabelPipeline()->setText(text3.toLatin1());

}


/******************************************************************************/
/*  Curve functions
/******************************************************************************/

void LesionsModelDisplayObject::SetCurveVisibleByPosition(inurbsSubModel* pSubModel, double position, vtkRenderer* renderer)
{
	QMapIterator<inurbsSubModel *, SubModelDisplayObject*> its(*m_pSubModelDisplayObjects);
	while(its.hasNext())
	{
		its.next();
		its.value()->SetCurveVisibleByPosition(position, renderer);

		if (pSubModel != its.key()) // set the points to be invisible if it is not the current model
		{
			CurveDisplayObject* curveDisplayObject = its.value()->GetCurveDisplayObject(position);
			if (curveDisplayObject) // if curves from other models exist, set the points invisible. 
				curveDisplayObject->SetPointsVisible(false,renderer);
		}
	}
}

void LesionsModelDisplayObject::SetLesionsColor(inurbsSubModel* pActiveLesion)
{
	QMapIterator<inurbsSubModel *, SubModelDisplayObject*> its(*m_pSubModelDisplayObjects);
	while(its.hasNext())
	{
		its.next();
		inurbsSubModel* pSubModel = its.key();
		SubModelDisplayObject* pSubModelDisplayObject = its.value();
		if (pActiveLesion == pSubModel)
		{
			pSubModelDisplayObject->SetSubModelColor(1.0,0.0,0.0); // set red if it is active model
		}
		else
		{
			pSubModelDisplayObject->SetSubModelColor(1.0,1.0,0.0); // set yellow if otherwise
		}
	}
}
