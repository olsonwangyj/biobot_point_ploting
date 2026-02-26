/******************************************************************************
	LesionModellingTransversalImageContext.cpp

	Date      : 26 Nov 2018
 ******************************************************************************/

#include <vtkActor.h>
#include <vtkPlane.h>
#include <vtkTextActor.h>
#include <vtkProperty.h>
#include <vtkLegendScaleActor.h>
#include <vtkAxisActor2D.h>

#include "Application.h"
#include "LesionModellingTransversalImageContext.h"
#include "LesionModelInteractorSytle.h"
#include "DistanceMeasurementImageInteractorStyle.h"
#include "ZoomPanImageInteractorStyle.h"
#include "SurfaceDisplayObject.h"
#include "CurveDisplayObject.h"
#include "qvOrthoImageSlicePipeline.h"

 /******************************************************************************/
 /* Constructors and Destructors
 /******************************************************************************/

LesionModellingTransversalImageContext::LesionModellingTransversalImageContext(int context)
	: TransversalImageContext(context)
{
	m_pLesionModelInteractorStyle = LesionModelInteractorStyle::New();
	m_pLesionModelInteractorStyle->SetPlane(ResliceInteractorStyle::PLANE_TRANSVERSAL);
	m_pLesionModelInteractorStyle->SetContext(context);

}

LesionModellingTransversalImageContext::~LesionModellingTransversalImageContext()
{
	m_pLesionModelInteractorStyle->Delete();
}

/******************************************************************************/
/* Set tool functions
/******************************************************************************/

void LesionModellingTransversalImageContext::AddEntity_VolumeDisplayObject(VolumeDisplayObject* volumeDisplayObject,bool bImageTextVisible)
{
	TransversalImageContext::AddEntity_VolumeDisplayObject(volumeDisplayObject,bImageTextVisible);

	vtkActor* actor;
	// add cursor pipelines
	if(m_iContext == CONTEXT_LESION_TRANSVERSAL)
	{
		actor = volumeDisplayObject->GetLeftCursorGuidePipeline()->addActor(getRenderer(1));
		actor->GetProperty()->SetColor(0.0,1.0,0.0);
		actor = volumeDisplayObject->GetRightCursorGuidePipeline()->addActor(getRenderer(1));
		actor->GetProperty()->SetColor(0.0,1.0,0.0);
		actor = volumeDisplayObject->GetAnteriorCursorGuidePipeline()->addActor(getRenderer(1));
		actor->GetProperty()->SetColor(0.0,1.0,0.0);
		actor = volumeDisplayObject->GetPosteriorCursorGuidePipeline()->addActor(getRenderer(1));
		actor->GetProperty()->SetColor(0.0,1.0,0.0);
	}
}

void LesionModellingTransversalImageContext::AddEntity(ModelDisplayObject* modelDisplayObject)
{
	// add volume text
	vtkTextActor* textActor = modelDisplayObject->GetModelVolumePipeline()->addTextActor(getRenderer(0));
	textActor->GetPositionCoordinate()->SetValue(0.99,0.99);
}

void LesionModellingTransversalImageContext::AddEntity(SurfaceDisplayObject* surfaceDisplayObject,VolumeDisplayObject* volumeDisplayObject)
{
	//qvCutterPipeline* cutterPipeline = surfaceDisplayObject->GetMeshPipeline()->addCutter("AXIS_Z", volumeDisplayObject->GetZSlicePipeline()->getSlicePlane());
	//cutterPipeline->addActor(getRenderer(1));
	qvCutterPipeline* cutterPipeline = surfaceDisplayObject->GetMeshPipeline()->getCutterPipeline("AXIS_Z");

	if(cutterPipeline == 0)
		cutterPipeline = surfaceDisplayObject->GetMeshPipeline()->addCutter("AXIS_Z",volumeDisplayObject->GetZSlicePipeline()->getSlicePlane());
	cutterPipeline->addActor(getRenderer(1));

}

void LesionModellingTransversalImageContext::AddEntity(CurveDisplayObject* curveDisplayObject)
{
	curveDisplayObject->GetPointsPipeline()->addActor(getRenderer(1));
	curveDisplayObject->GetFirstPointPipeline()->addActor(getRenderer(1));
	curveDisplayObject->GetCurvePipeline()->addActor(getRenderer(1));
}

void LesionModellingTransversalImageContext::AddEntity(AnnotationScaleVTKDisplayObject* annotationScaleVTKDisplayObject)
{
	vtkLegendScaleActor *legendScaleActor = annotationScaleVTKDisplayObject->GetScaleVTKPipeline()->addLegendScaleActor(getRenderer(1));
	legendScaleActor->SetTopAxisVisibility(false);
	legendScaleActor->SetRightAxisVisibility(false);
	legendScaleActor->SetLegendVisibility(false);
	// default label mode setting is distance mode
	//legendScaleActor->SetLabelModeToXYCoordinates();
	//legendScaleActor->

	vtkAxisActor2D *bottomAxis =  legendScaleActor->GetBottomAxis();
	bottomAxis->SetLabelFormat("%0.1f");
	// this function affects the accuracy of the scale
	//bottomAxis->AdjustLabelsOn();

	vtkAxisActor2D *leftAxis = legendScaleActor->GetLeftAxis();
	leftAxis->SetLabelFormat("%0.1f");
	//leftAxis->AdjustLabelsOn();
}

void LesionModellingTransversalImageContext::AddEntity(AnnotationDisplayObject *annotationDisplayObject)
{
	vtkRenderer *pRenderer = getRenderer(1);

	vtkTextActor* textActor = annotationDisplayObject->GetAnteriorTextPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(0.53,0.95);

	textActor = annotationDisplayObject->GetLeftTextPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(0.98,0.55);

	textActor = annotationDisplayObject->GetPosteriorTextPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(0.53,0.07);

	textActor = annotationDisplayObject->GetRightTextPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(0.06,0.55);

	double fIdentityX = 0.91;
	textActor = annotationDisplayObject->GetProstateIdentityPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(fIdentityX,0.10);

	textActor = annotationDisplayObject->GetLesionIdentityPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(fIdentityX,0.08);

	textActor = annotationDisplayObject->GetSelectedIdentityPipeline()->addTextActor(pRenderer);
	textActor->GetPositionCoordinate()->SetValue(fIdentityX,0.06);
}

/******************************************************************************/
/* Set tool functions
/******************************************************************************/

void LesionModellingTransversalImageContext::SetTool(int id)
{
	switch(id)
	{
	case TOOL_VIEW_RESLICE:
		setInteractorObserver(m_pResliceInteractorStyle);
		break;

	case TOOL_VIEW_ZOOM:
		setInteractorObserver(m_pZoomPanImageInteractorStyle);
		m_pZoomPanImageInteractorStyle->SetCurrentTool(TOOL_VIEW_ZOOM);
		break;

	case TOOL_VIEW_PAN:
		setInteractorObserver(m_pZoomPanImageInteractorStyle);
		m_pZoomPanImageInteractorStyle->SetCurrentTool(TOOL_VIEW_PAN);
		break;

	case TOOL_VIEW_LESION:
		setInteractorObserver(m_pLesionModelInteractorStyle);
		m_pLesionModelInteractorStyle->SetCurrentRenderer(getRenderer());// for getWindowWorldBounds to update cursors before any mouse movement. 
		break;

	case TOOL_VIEW_MEASURE_DISTANCE:
		setInteractorObserver(m_pDistanceMeasureInteractorStyle);
		break;

	default:
		break;
	}
}


/******************************************************************************/
/* Other functions
/******************************************************************************/

bool LesionModellingTransversalImageContext::GetWindowWorldBounds(double bounds[4])
{
	return m_pLesionModelInteractorStyle->GetWindowWorldBounds(bounds);
}

void LesionModellingTransversalImageContext::DisplayToWorld(int x,int y,double &wx,double &wy)
{
	m_pLesionModelInteractorStyle->DisplayToWorld(x,y,wx,wy);
}

void LesionModellingTransversalImageContext::SetActiveLesion(inurbsSubModel* pSubModel)
{
	m_pLesionModelInteractorStyle->SetActiveLesion(pSubModel);
}

inurbsSubModel* LesionModellingTransversalImageContext::GetActiveLesion()
{
	return m_pLesionModelInteractorStyle->GetActiveLesion();
}
