/******************************************************************************
    LesionModelInteractorStyle.cpp

    Date      : 26 Nov 2018
 ******************************************************************************/

#include <vtkObjectFactory.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

#include "Application.h"
#include "LesionModelInteractorSytle.h"
#include "inurbsSubModel.h"

// macro to implement the New function
vtkStandardNewMacro(LesionModelInteractorStyle);

/******************************************************************************/
/* Constructors and Destructors                                                                            
/******************************************************************************/

LesionModelInteractorStyle::LesionModelInteractorStyle() : CursorBoundResliceInteractorStyle()
{
	m_iPickedPointIndex = -1;
	m_pCurCurve = NULL;
	m_pActiveLesion = NULL;
	m_doubleClickTimer.start();
	m_pInitialPoints = NULL;
	m_iNumInitialPoints = 0;
}

LesionModelInteractorStyle::~LesionModelInteractorStyle()
{
	if (m_pInitialPoints)
		delete [] m_pInitialPoints;
}

/******************************************************************************/
/* Event handlers                                                                            
/******************************************************************************/

void LesionModelInteractorStyle::OnLeftButtonDown()
{
	CONTROLLER* pSurgeryController = GetController();
	int state = pSurgeryController->GetState();
	int subState = pSurgeryController->GetSubState();
	if (state != BaseSurgeryController::STATE_LESION)
		return;

	emit clearWarningLabelMessage();

	CursorBoundResliceInteractorStyle::OnLeftButtonDown();

	if (subState == BaseSurgeryController::STATE_LESION_FIRST) // no lesion created, interactor shall behave as cursor bound reslicer
		return;

	if (m_iPickedAxis != -1)  // axis picked
		return;

	int x = this->Interactor->GetEventPosition()[0];
	int y = this->Interactor->GetEventPosition()[1];
	double pickedPoint[4];
    this->ComputeDisplayToWorld(x, y, 0, pickedPoint);

	// check if this picked point is out of allowable bounds
	double* modelBounds = GetVisualEngine()->GetCursorModelBounds();
	if (!modelBounds)
		return;

	if (pickedPoint[0] < modelBounds[0]-MODEL_MOVE_DELTA_BOUNDS ||
		pickedPoint[0] > modelBounds[1]+MODEL_MOVE_DELTA_BOUNDS ||
		pickedPoint[1] < modelBounds[2]-MODEL_MOVE_DELTA_BOUNDS ||
		pickedPoint[1] > modelBounds[3]+MODEL_MOVE_DELTA_BOUNDS)
		return;

	double curPos = GetVisualEngine()->GetCurrentPlanePosition();

	int repeatCount = this->Interactor->GetRepeatCount();
	m_iPickedPointIndex = pSurgeryController->GetPickedLesionPointIndex(m_pActiveLesion,pickedPoint[0],pickedPoint[1],curPos, &m_pCurCurve);
	StoreInitialPoints();
	if (m_iPickedPointIndex != -1) //selected a point
	{
		qint64 dt = m_doubleClickTimer.elapsed();
		//printf("time elapsed = %d\n",dt);
		if (repeatCount > 0 && dt < DOUBLE_CLICK_ELAPSED_TIME) // double click to delete point
		{
			if (m_pCurCurve)
			{
				StartDeletePoint();
				if (m_pCurCurve->GetNumOfPoints() == 1) // only one point
				{
					GetVisualEngine()->DeleteLesionDisplayCurve();
					m_pCurCurve = NULL;
				}
				else
				{
					m_pCurCurve->RemovePointAt(m_iPickedPointIndex);
					GetVisualEngine()->UpdateLesionCurveDisplayObject(m_pActiveLesion,m_pCurCurve);
				}
			}
		}
		else // picked some point, now start to move
		{
			StartMovePoint();
		}
	}
	else
	{
		// check for adding iso curve conditions
		if (m_pActiveLesion && m_pActiveLesion->GetSurface()->IsSurfaceValid() && !(GetVisualEngine()->IsTransversalDisplayContainLesionCurve()))
		{
			// add curve points
			m_iPickedPointIndex = pSurgeryController->AddCurveToLesionModel(m_pActiveLesion,pickedPoint[0],pickedPoint[1],curPos,&m_pCurCurve);
			if (m_iPickedPointIndex != -1)
			{
				StoreInitialPoints();
				StartMovePoint();
				return;
			}
		}

		// add point
		m_iPickedPointIndex = GetController()->AddLesionPoint(m_pActiveLesion,pickedPoint[0],pickedPoint[1],curPos,&m_pCurCurve);
		StartMovePoint();
	}

	m_doubleClickTimer.start();

}

void LesionModelInteractorStyle::OnMouseMove()
{
	CONTROLLER* pSurgeryController = GetController();
	int state = pSurgeryController->GetState();
	int subState = pSurgeryController->GetSubState();
	if (state != BaseSurgeryController::STATE_LESION)
		return;

	int x = this->Interactor->GetEventPosition()[0];
	int y = this->Interactor->GetEventPosition()[1];
	this->FindPokedRenderer(x, y);

	if (this->CurrentRenderer == 0)
		return;

	CursorBoundResliceInteractorStyle::OnMouseMove();

	if (subState == BaseSurgeryController::STATE_LESION_FIRST) // no lesion created, interactor shall behave as cursor bound reslicer
		return;

	switch (this->State) 
	{
		case VTKIS_MOVE_POINT:			
			MovePoint();
			this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
		break;

		default:
		break;
	}
}

void LesionModelInteractorStyle::OnLeftButtonUp()
{
	ResliceInteractorStyle::OnLeftButtonUp();

	int subState = GetController()->GetSubState();
	if (subState == BaseSurgeryController::STATE_LESION_FIRST) // no lesion created, interactor shall behave as cursor bound reslicer
		return;

	if (m_iPickedAxis != -1) // some axis picked, no need to handle here
		return;

	switch (this->State) 
    {
		case VTKIS_MOVE_POINT:
		{
			this->EndMovePoint();
			if (m_iNumInitialPoints == 3 && m_pCurCurve->GetNumOfPoints() < 3) // less than 3 points now, restore
			{
				m_pCurCurve->SetPoints(m_pInitialPoints,m_iNumInitialPoints);
				GetVisualEngine()->UpdateLesionCurveDisplayObject(m_pActiveLesion,m_pCurCurve);
				emit setLabelMessage(MSGLABEL_MODEL_CONTOUR_MIN_POINTS);
			}
		}
		break;

		case VTKIS_DELETE_POINT:
		{
			this->EndDeletePoint();
			if (m_iNumInitialPoints == 3 && m_pCurCurve->GetNumOfPoints() < 3) // less than 3 points now, restore
			{
				m_pCurCurve->SetPoints(m_pInitialPoints,m_iNumInitialPoints);
				GetVisualEngine()->UpdateLesionCurveDisplayObject(m_pActiveLesion,m_pCurCurve);
				emit setLabelMessage(MSGLABEL_MODEL_CONTOUR_MIN_POINTS);
			}
		}
		break;

		default:
		break;
	}

	GetController()->BuildLesionSurface(m_pActiveLesion);
}


/******************************************************************************/
/* Move point functions                                                                            
/******************************************************************************/

void LesionModelInteractorStyle::StartMovePoint() 
{
	if(this->State != VTKIS_NONE) 
		return;
	StartState(VTKIS_MOVE_POINT);
}

void LesionModelInteractorStyle::EndMovePoint() 
{
	if (this->State != VTKIS_MOVE_POINT) 
		return;	
	StopState();
}

void LesionModelInteractorStyle::MovePoint()
{
	if (this->CurrentRenderer == NULL || m_iPickedPointIndex == -1 || m_pCurCurve == 0)
	{
		return;
	}

	double pickedPoint[4];
	this->ComputeDisplayToWorld(Interactor->GetEventPosition()[0], Interactor->GetEventPosition()[1], 0, pickedPoint);


	double* modelBounds = GetVisualEngine()->GetCursorModelBounds();
	if (!modelBounds)
		return;

	if (pickedPoint[0] < (modelBounds[0]-MODEL_MOVE_DELTA_BOUNDS))
		pickedPoint[0] = modelBounds[0]-MODEL_MOVE_DELTA_BOUNDS;
	else if (pickedPoint[0] > (modelBounds[1]+MODEL_MOVE_DELTA_BOUNDS))
		pickedPoint[0] = modelBounds[1]+MODEL_MOVE_DELTA_BOUNDS;

	if (pickedPoint[1] < modelBounds[2]-MODEL_MOVE_DELTA_BOUNDS)
		pickedPoint[1] = modelBounds[2]-MODEL_MOVE_DELTA_BOUNDS;
	else if (pickedPoint[1] > modelBounds[3]+MODEL_MOVE_DELTA_BOUNDS)
		pickedPoint[1] = modelBounds[3]+MODEL_MOVE_DELTA_BOUNDS;


	// if the new position coincide with another point, the current point will be deleted. 
	if (!m_pCurCurve->SetPointAt(m_iPickedPointIndex, pickedPoint[0], pickedPoint[1]))
	{
		m_iPickedPointIndex = -1;
	}
	
	GetVisualEngine()->UpdateLesionCurveDisplayObject(m_pActiveLesion,m_pCurCurve);
	//this->Interactor->Render();
}

/******************************************************************************/
/*  Delete point functions                                                                          
/******************************************************************************/

void LesionModelInteractorStyle::StartDeletePoint()
{
  if(this->State != VTKIS_NONE) 
  {
    return;
  }
  this->StartState(VTKIS_DELETE_POINT);
}

void LesionModelInteractorStyle::EndDeletePoint()
{
  if (this->State != VTKIS_DELETE_POINT) 
  {
    return;
  }

  this->StopState();
}

/******************************************************************************/
/*  Other functions                                                                          
/******************************************************************************/

void LesionModelInteractorStyle::StoreInitialPoints()
{
	if (!m_pCurCurve)
	{
		m_iNumInitialPoints = 0;
		if (m_pInitialPoints)
			delete [] m_pInitialPoints;
		m_pInitialPoints = 0;
		return;
	}
	
	if (m_pInitialPoints)
		delete [] m_pInitialPoints;
	m_iNumInitialPoints = m_pCurCurve->GetNumOfPoints();
	m_pInitialPoints = new inurbsPoint[m_iNumInitialPoints];
	QList<inurbsPoint*>* pointList = m_pCurCurve->GetPoints();
	
	for(int i = 0; i < m_iNumInitialPoints; i++)
	{
		inurbsPoint& point = m_pInitialPoints[i];

		point.x = pointList->at(i)->x;
		point.y = pointList->at(i)->y;
		point.z = pointList->at(i)->z;
	}

}