/******************************************************************************
	FusionVisualEngine.cpp

	Date      : 29 Oct 2018
******************************************************************************/

#include "Application.h"
#include "LesionModellingTransversalImageContext.h"
#include "LesionsModelDisplayObject.h"
#include "SubModelDisplayObject.h"
#include "SurfaceDisplayObject.h"
#include "CurveDisplayObject.h"
#include "inurbsSubModel.h"
#include "inurbsPlanarCurveStack.h"
#include "qvOrthoImageSlicePipeline.h"

FusionVisualEngine* FusionVisualEngine::m_pInstance = 0;

/******************************************************************************/
/* Constructors and destructors                                                                           
/******************************************************************************/

FusionVisualEngine::FusionVisualEngine()
{
	// new contexts
//	m_pDefaultContext = new qvContext(1);
	m_pVirtualContext = new VirtualContext;
	m_pModellingTransversalImageContext = new ModellingTransversalImageContext(CONTEXT_MODELLING_TRANSVERSAL);
	m_pLesionModellingTransversalImageContext = new LesionModellingTransversalImageContext(CONTEXT_LESION_TRANSVERSAL);
	m_pSagittalImageContext = new SagCorImageContext(CONTEXT_SAGITTAL,AXIS_X);
	m_pCoronalImageContext = new SagCorImageContext(CONTEXT_CORONAL, AXIS_Y);

	// new display objects
	m_pAnnotationScaleVTKDisplayObject = new AnnotationScaleVTKDisplayObject;
	m_pAnnotationDisplayObject = new AnnotationDisplayObject;
	m_pVolumeDisplayObject = new VolumeDisplayObject;
	m_pModelDisplayObject = new ModelDisplayObject;
	m_pLesionsModelDisplayObject = new LesionsModelDisplayObject;
}

FusionVisualEngine::~FusionVisualEngine()
{
	// remove display objects first before removing the contexts
	// as they are added to the renderers inside the contexts.
	// If the contexts are removed first, the destructors of the props
	// will crash when trying to remove them from the renderers.
	delete m_pAnnotationScaleVTKDisplayObject;
	delete m_pAnnotationDisplayObject;
	delete m_pVolumeDisplayObject;
	delete m_pModelDisplayObject;
	delete m_pLesionsModelDisplayObject;

	// remove contexts
//	delete m_pDefaultContext;
	delete m_pVirtualContext;
	delete m_pModellingTransversalImageContext;
	delete m_pLesionModellingTransversalImageContext;
	delete m_pSagittalImageContext;
	delete m_pCoronalImageContext;
}

FusionVisualEngine* FusionVisualEngine::GetInstance()
{
	if (m_pInstance == 0)
		m_pInstance = new FusionVisualEngine();

	return m_pInstance;
}

void FusionVisualEngine::DeleteInstance()
{
	if (m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = 0;
	}
}

void FusionVisualEngine::Init()
{
//	m_pDefaultContext->setBackground(34.0f/255, 34.0f/255, 34.0f/255);
//	m_pDefaultContext->updateWindow();

	// add display objects to modelling transversal image context
	m_pModellingTransversalImageContext->AddEntity_AnnotationScaleVTKDisplayObject(m_pAnnotationScaleVTKDisplayObject);
	m_pModellingTransversalImageContext->AddEntity_AnnotationDisplayObject(m_pAnnotationDisplayObject);
	m_pModellingTransversalImageContext->AddEntity_VolumeDisplayObject(m_pVolumeDisplayObject,false);
	m_pModellingTransversalImageContext->AddEntity(m_pModelDisplayObject);

	// add display objects to lesion modelling transversal image context
	m_pLesionModellingTransversalImageContext->AddEntity(m_pAnnotationScaleVTKDisplayObject);
	m_pLesionModellingTransversalImageContext->AddEntity_VolumeDisplayObject(m_pVolumeDisplayObject,false);
	m_pLesionModellingTransversalImageContext->AddEntity(m_pModelDisplayObject);
	m_pLesionModellingTransversalImageContext->AddEntity(m_pAnnotationDisplayObject);

	// add display objects to sagitall image context
	m_pSagittalImageContext->AddEntity(m_pAnnotationScaleVTKDisplayObject);
	m_pSagittalImageContext->AddEntity(m_pAnnotationDisplayObject, false);
	m_pSagittalImageContext->AddEntity(m_pVolumeDisplayObject);
	m_pSagittalImageContext->AddEntity(m_pModelDisplayObject);

	// add display objects to coronal image context
	m_pCoronalImageContext->AddEntity(m_pAnnotationScaleVTKDisplayObject);
	m_pCoronalImageContext->AddEntity(m_pAnnotationDisplayObject, false);
	m_pCoronalImageContext->AddEntity(m_pVolumeDisplayObject);

	// add display objects to virtual image context
	m_pVirtualContext->AddEntity(m_pVolumeDisplayObject);

	// init connections
	connect(m_pModellingTransversalImageContext, SIGNAL(reset(TransversalImageContext *)), this, SLOT(ResetTransversalView(TransversalImageContext *)));
	connect(m_pLesionModellingTransversalImageContext, SIGNAL(reset(TransversalImageContext *)), this, SLOT(ResetTransversalView(TransversalImageContext *)));
}

/******************************************************************************/
/* State functions                                                                        
/******************************************************************************/

void FusionVisualEngine::UpdateState(int state, int subState)
{
	switch (state)
	{
		case BaseSurgeryController::STATE_IMPORT_IMAGE:
		{
			switch (subState)
			{
				case BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED:
					m_pModellingTransversalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pSagittalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pCoronalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pVolumeDisplayObject->SetCursorGuidesVisible(false);
				break;

				default:
				break;
			}
		}
		break;

		case BaseSurgeryController::STATE_MODEL:
		{
			switch (subState)
			{
				double position;

				case BaseSurgeryController::STATE_MODEL_FIRST:
					//m_pModelDisplayObject->SetModelVolumeVisible(true);
					m_pVolumeDisplayObject->SetCursorGuidesVisible(true);
					m_pModelDisplayObject->SetLimitsVisible(false);
					m_pModelDisplayObject->UpdateProstateVolume();
					m_pModellingTransversalImageContext->SetTool(TOOL_VIEW_MODEL);
					m_pSagittalImageContext->SetTool(TOOL_VIEW_MODEL);
					m_pCoronalImageContext->SetTool(TOOL_VIEW_RESLICE);
					//GoToMidSlices();
					ResetAllViews();
				break;

				case BaseSurgeryController::STATE_MODEL_FIRSTCURVE_CREATED:
					m_pModelDisplayObject->SetLimitsVisible(true);
					m_pSagittalImageContext->updateWindow();
					break;

				case BaseSurgeryController::STATE_MODEL_AUTOMODELLING:
					m_pModelDisplayObject->SetLimitsVisible(false);
					m_pSagittalImageContext->updateWindow();
				break;

				case BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE:
					position = GetCurrentPlanePosition();
					m_pModelDisplayObject->SetCurveVisibleByPosition(GetCurrentModel(), position, m_pModellingTransversalImageContext->getRenderer(1));
					m_pModelDisplayObject->SetSurfaceVisibleByPosition(position, m_pModellingTransversalImageContext->getRenderer(1));
				break;

				default:
				break;
			}
		}
		break;

		case BaseSurgeryController::STATE_LESION:
		{
			switch (subState)
			{
				case BaseSurgeryController::STATE_LESION_FIRST:
					m_pLesionModellingTransversalImageContext->SetTool(TOOL_VIEW_LESION);
					m_pSagittalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pCoronalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pVolumeDisplayObject->SetCursorGuidesVisible(true);
					ResetAllViews();
				break;

				case BaseSurgeryController::STATE_LESION_MODEL_CREATED:
					m_pVolumeDisplayObject->SetCursorGuidesVisible(true);
				break;

				default:
				break;
			}
		}
		break;

		case BaseSurgeryController::STATE_FINISH:
			switch (subState)
			{
				case BaseSurgeryController::STATE_FINISH_FIRST:
					m_pLesionModellingTransversalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pSagittalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pCoronalImageContext->SetTool(TOOL_VIEW_RESLICE);
					m_pVolumeDisplayObject->SetCursorGuidesVisible(false);
					SetProstateModelVisible(true); // might have been turned off at lesion stage.
					UpdateSliceZ();
					ResetAllViews();
				break;

				default:
				break;
			}
		break;

		default:
		break;
	}
}

/******************************************************************************/
/* Context functions                                                                        
/******************************************************************************/

void FusionVisualEngine::SetTool(int context, int tool)
{
	switch (context)
	{
		case CONTEXT_MODELLING_TRANSVERSAL:
			m_pModellingTransversalImageContext->SetTool(tool);
		break;

		case CONTEXT_LESION_TRANSVERSAL:
			m_pLesionModellingTransversalImageContext->SetTool(tool);
		break;

		case CONTEXT_SAGITTAL:
			m_pSagittalImageContext->SetTool(tool);
		break;

		case CONTEXT_CORONAL:
			m_pCoronalImageContext->SetTool(tool);
		break;

		case CONTEXT_VIRTUAL:
			m_pVirtualContext->SetTool(tool);
		break;

		default:
		break;
	}
}

void FusionVisualEngine::ResetView(int context)
{
	switch (context)
	{
		case CONTEXT_MODELLING_TRANSVERSAL:
			m_pModellingTransversalImageContext->ResetView(m_pVolumeDisplayObject);
		break;

		case CONTEXT_LESION_TRANSVERSAL:
			m_pLesionModellingTransversalImageContext->ResetView(m_pVolumeDisplayObject);
		break;

		case CONTEXT_SAGITTAL:
			m_pSagittalImageContext->ResetView(m_pVolumeDisplayObject);
		break;

		case CONTEXT_CORONAL:
			m_pCoronalImageContext->ResetView(m_pVolumeDisplayObject);
		break;

		case CONTEXT_VIRTUAL:
			m_pVirtualContext->ResetView(m_pVolumeDisplayObject);
		break;

		default:
		break;
	}
}

void FusionVisualEngine::Reset2DViews()
{
	ResetView(CONTEXT_MODELLING_TRANSVERSAL);
	ResetView(CONTEXT_LESION_TRANSVERSAL);
	ResetView(CONTEXT_SAGITTAL);
	ResetView(CONTEXT_CORONAL);
}

void FusionVisualEngine::Reset3DView()
{
	ResetView(CONTEXT_VIRTUAL);
}

void FusionVisualEngine::TransversalImageContextViewChanged()
{
	BaseVisualEngine::TransversalImageContextViewChanged();

	int state = GetController()->GetState();
	switch (state)
	{
		case BaseSurgeryController::STATE_LESION:
			m_pVolumeDisplayObject->UpdateCursorGuidePositions(m_pLesionModellingTransversalImageContext);
			m_pLesionModellingTransversalImageContext->updateWindow();
		break;

		default:
		break;
	}
}

void FusionVisualEngine::Update2DWindows()
{
	int state = GetController()->GetState();

	if (state == BaseSurgeryController::STATE_IMPORT_IMAGE || state == BaseSurgeryController::STATE_MODEL)
		m_pModellingTransversalImageContext->updateWindow();
	else if (state == BaseSurgeryController::STATE_LESION || state == BaseSurgeryController::STATE_FINISH)
		m_pLesionModellingTransversalImageContext->updateWindow();

	m_pSagittalImageContext->updateWindow();
	m_pCoronalImageContext->updateWindow();	
}

void FusionVisualEngine::UpdateWindows()
{
	int iState = GetController()->GetState();

	switch (iState)
	{
		case BaseSurgeryController::STATE_IMPORT_IMAGE:
		case BaseSurgeryController::STATE_MODEL:
			m_pModellingTransversalImageContext->updateWindow();
		break;

		case BaseSurgeryController::STATE_LESION:
			m_pLesionModellingTransversalImageContext->updateWindow();
		break;

		case BaseSurgeryController::STATE_FINISH:
			m_pLesionModellingTransversalImageContext->updateWindow();
		break;

		default:
		break;
	}

	m_pSagittalImageContext->updateWindow();
	m_pCoronalImageContext->updateWindow();	
	m_pVirtualContext->updateWindow();
}

/******************************************************************************/
/* DisplayObject functions                                                                           
/******************************************************************************/
void FusionVisualEngine::UpdateSliceZ()
{
	double position = m_pVolumeDisplayObject->GetZSlicePipeline()->getSliceCoordinate();
	int state = GetController()->GetState();
	switch (state)
	{
		case BaseSurgeryController::STATE_MODEL:
		{
			m_pModelDisplayObject->SetCurveVisibleByPosition(GetCurrentModel(), position, m_pModellingTransversalImageContext->getRenderer(1));
			m_pModelDisplayObject->SetSurfaceVisibleByPosition(position, m_pModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetCurveVisibleByPosition(NULL, position, m_pModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetLabelsVisibleByPosition(position, m_pModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetSurfaceVisibleByPosition(position, m_pLesionModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetLesionsColor(NULL);
		}
		break;

		case BaseSurgeryController::STATE_LESION:
		{
			m_pLesionsModelDisplayObject->SetCurveVisibleByPosition(GetActiveLesion(),position, m_pLesionModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetLabelsVisibleByPosition(position, m_pLesionModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetSurfaceVisibleByPosition(position,m_pLesionModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetLesionsColor(GetActiveLesion());
		}
		break;

		case BaseSurgeryController::STATE_FINISH:
			m_pLesionsModelDisplayObject->SetCurveVisibleByPosition(NULL,position, m_pLesionModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetLabelsVisibleByPosition(position, m_pLesionModellingTransversalImageContext->getRenderer(1));
			m_pLesionsModelDisplayObject->SetLesionsColor(NULL);
		break;

		default:
		break;
	}
}

void FusionVisualEngine::UpdateSlice(int axis)
{
	int state = GetController()->GetState();
	if (state != BaseSurgeryController::STATE_IMPORT_IMAGE && state != BaseSurgeryController::STATE_MODEL && state != BaseSurgeryController::STATE_LESION && state != BaseSurgeryController::STATE_FINISH)
		return;

	switch (axis)
	{
		case AXIS_X:
			TransversalImageContextViewChanged();
		break;

		case AXIS_Y:
			TransversalImageContextViewChanged();
		break;

		case AXIS_Z:
			UpdateSliceZ();
		break;

		default:
		break;
	}
}

/******************************************************************************/
/* Signal functions                                                                        
/******************************************************************************/

void FusionVisualEngine::ResetTransversalView(TransversalImageContext* pContext)
{
	if (!pContext)
		return;

	if (pContext->GetContext() == CONTEXT_MODELLING_TRANSVERSAL || pContext->GetContext() == CONTEXT_LESION_TRANSVERSAL)
		TransversalImageContextViewChanged();
	else
		pContext->updateWindow();
}

/******************************************************************************/
/* Import Image functions                                                                        
/******************************************************************************/
// called when only the image pixels are changed. 
void FusionVisualEngine::UpdateImageImport() 
{
	if (m_pVolumeDisplayObject == NULL)
		return;

	m_pVolumeDisplayObject->UpdateImageImport();
	Update2DWindows();
}

/******************************************************************************/
/* Model related functions                                                                           
/******************************************************************************/

void FusionVisualEngine::AddSurfaceDisplayObject(SurfaceDisplayObject *surfaceDisplayObject)
{
	m_pModellingTransversalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pLesionModellingTransversalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pSagittalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pCoronalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pVirtualContext->AddEntity(surfaceDisplayObject);
}

void FusionVisualEngine::CreateCurveDisplayObjects(int modelId, inurbsSubModel* subModel)
{
	inurbsPlanarCurveStack* pCurveStack = subModel->GetCurveStack();
	if (!pCurveStack)
		return;

	int iNumCurves = pCurveStack->GetNumOfCurves();
	for (int i=0;i<iNumCurves;i++)
	{
		inurbsPlanarCurve* pCurve = pCurveStack->GetCurve(i);
		if (!pCurve)
			continue;

		CreateCurveDisplayObject(modelId, pCurve);
		UpdateCurveDisplayObject(modelId, pCurve);
	}
}

/******************************************************************************/
/* Lesion functions                                                                         
/******************************************************************************/

void FusionVisualEngine::CreateLesionSubModelDisplayObject(inurbsSubModel* lesionSubModel)
{
	// surface display object is created in creation of submodel display object
	SubModelDisplayObject *subModelDisplayObject = m_pLesionsModelDisplayObject->CreateSubModelDisplayObject(lesionSubModel);

	// add surface display object to all the contexts
	SurfaceDisplayObject *surfaceDisplayObject = subModelDisplayObject->GetSurfaceDisplayObject();
	surfaceDisplayObject->SetSurface(lesionSubModel->GetSurface());

	m_pModellingTransversalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pLesionModellingTransversalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pSagittalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pCoronalImageContext->AddEntity(surfaceDisplayObject, m_pVolumeDisplayObject);
	m_pVirtualContext->AddEntity(surfaceDisplayObject);


	// add label2D display Object to transversal contexts
	Label2DDisplayObject* pLabel2DDisplayObject = m_pLesionsModelDisplayObject->GetLabel2DDisplayObject(lesionSubModel);
	m_pLesionModellingTransversalImageContext->AddEntity_Label2DDisplayObject(pLabel2DDisplayObject);
	m_pModellingTransversalImageContext->AddEntity_Label2DDisplayObject(pLabel2DDisplayObject);

	// add label3D display object to virtual contexts
	Label3DDisplayObject* pLabel3DDisplayObject = m_pLesionsModelDisplayObject->GetLabel3DDisplayObject(lesionSubModel);
	m_pVirtualContext->AddEntity(pLabel3DDisplayObject);

	// If this is not called here, vtk will render a warning that complains about input being null
	surfaceDisplayObject->UpdateSurface();

	SetActiveLesion(lesionSubModel);
}

void FusionVisualEngine::DeleteLesionSubModelDisplayObject(inurbsSubModel* pSubModel)
{
	inurbsSubModel* pNextLesion = GetController()->GetNextLesionSubModel(pSubModel);

	m_pLesionsModelDisplayObject->RemoveSubModelDisplayObject(pSubModel);

	if (pNextLesion == pSubModel)
		SetActiveLesion(NULL);
	else
	{
		SetActiveLesion(pNextLesion);
		ShowActiveLesionCentre();
	}
}

void FusionVisualEngine::CreateLesionCurveDisplayObjects(inurbsSubModel* pSubModel)
{
	if (!pSubModel)
		return;

	inurbsPlanarCurveStack* pCurveStack = pSubModel->GetCurveStack();
	if (!pCurveStack)
		return;

	int iNumCurves = pCurveStack->GetNumOfCurves();
	for (int i=0;i<iNumCurves;i++)
	{
		inurbsPlanarCurve* pCurve = pCurveStack->GetCurve(i);
		CreateLesionCurveDisplayObject(pSubModel,pCurve);
		UpdateLesionCurveDisplayObject(pSubModel,pCurve);
	}

	UpdateSliceZ();
}

void FusionVisualEngine::GotoNextLesion()
{
	// get currnet lesion
	inurbsSubModel* pCurLesion = m_pLesionModellingTransversalImageContext->GetActiveLesion();
	if (!pCurLesion)
		return;

	inurbsSubModel* pNextLesion = GetController()->GetNextLesionSubModel(pCurLesion);
	if (pNextLesion)
	{
		SetActiveLesion(pNextLesion);
		ShowActiveLesionCentre();
	}	
}

void FusionVisualEngine::GotoPrevLesion()
{
	// get currnet lesion
	inurbsSubModel* pCurLesion = m_pLesionModellingTransversalImageContext->GetActiveLesion();
	if (!pCurLesion)
		return;

	inurbsSubModel* pPrevLesion = GetController()->GetPrevLesionSubModel(pCurLesion);
	if (pPrevLesion)
	{
		SetActiveLesion(pPrevLesion);
		ShowActiveLesionCentre();
	}
}

void FusionVisualEngine::ShowActiveLesionCentre()
{
	double fCentre[3];
	
	inurbsSubModel* pActiveLesion = GetActiveLesion();
	if (!pActiveLesion)
		return;

	pActiveLesion->GetCentre(fCentre);
	m_pVolumeDisplayObject->SetFocalPoint(fCentre);
	Update2DWindows();
}

void FusionVisualEngine::UpdateAllLabelTexts()
{
	m_pLesionsModelDisplayObject->UpdateAllLabelTexts();
	UpdateSliceZ();
	UpdateWindows();
}

void FusionVisualEngine::UpdateLesionSurfaceDisplayObject(inurbsSubModel* pSubModel, inurbsSurface* surface)
{
	//m_pModelDisplayObject->UpdateProstateVolume();

	int id = GetController()->GetLesionSubModelId(pSubModel);
	m_pLesionsModelDisplayObject->UpdateSurfaceDisplayObject(id, pSubModel,surface,m_pVolumeDisplayObject->GetImageStack());

	UpdateSliceZ();
	UpdateWindows();
}

void FusionVisualEngine::UpdateLesionCurveLabel(inurbsSubModel* pSubModel)
{
	//m_pModelDisplayObject->UpdateProstateVolume();

	int id = GetController()->GetLesionSubModelId(pSubModel);
	m_pLesionsModelDisplayObject->UpdateCurveLabel(id, pSubModel,  m_pVolumeDisplayObject->GetImageStack());

	UpdateSliceZ();
	UpdateWindows();
}


void FusionVisualEngine::CreateLesionCurveDisplayObject(inurbsSubModel* pSubModel, inurbsPlanarCurve* pCurve)
{
	// get sub model display object
	SubModelDisplayObject *subModelDisplayObject = m_pLesionsModelDisplayObject->GetSubModelDisplayObject(pSubModel);

	// create curve display object
	CurveDisplayObject *curveDisplayObject = subModelDisplayObject->CreateCurveDisplayObject(pCurve);

	// add curve entity to contexts
	m_pLesionModellingTransversalImageContext->AddEntity(curveDisplayObject);
	m_pModellingTransversalImageContext->AddEntity(curveDisplayObject);
	m_pSagittalImageContext->AddEntity(curveDisplayObject,m_pVolumeDisplayObject);
	m_pCoronalImageContext->AddEntity(curveDisplayObject,m_pVolumeDisplayObject);
	m_pVirtualContext->AddEntity(curveDisplayObject);

	curveDisplayObject->SetCurveColor(1.0,0.0,0.0);

	// do not need to update window at this point, there is nothing to be drawn. 
	//m_pModellingTransversalImageContext->updateWindow();
}

void FusionVisualEngine::UpdateLesionCurveDisplayObject(inurbsSubModel* pSubModel, inurbsPlanarCurve* pCurve)
{
	// get sub model display object
	SubModelDisplayObject *subModelDisplayObject = m_pLesionsModelDisplayObject->GetSubModelDisplayObject(pSubModel);

	subModelDisplayObject->UpdateCurveDisplayObject(pCurve);

	Update2DWindows();
}

void FusionVisualEngine::DeleteLesionDisplayCurve()
{
	double pos = m_pVolumeDisplayObject->GetZSlicePipeline()->getSliceCoordinate();
	inurbsSubModel* pSubModel = m_pLesionModellingTransversalImageContext->GetActiveLesion();

	// get the curve
	inurbsPlanarCurve *curve = pSubModel->GetCurve(pos);
	if (!curve)
		return;
	
	DeleteLesionCurveDisplayObject(pSubModel,curve);
	GetController()->DeleteLesionCurve(pSubModel,pos);
}

bool FusionVisualEngine::IsTransversalDisplayContainLesionCurve()
{
	double pos = m_pVolumeDisplayObject->GetZSlicePipeline()->getSliceCoordinate();
	inurbsSubModel* pSubModel = m_pLesionModellingTransversalImageContext->GetActiveLesion();

	inurbsCurve* pCurve = pSubModel->GetCurve(pos);
	
	if (pCurve)
		return true;
	else
		return false;
}

void FusionVisualEngine::DeleteLesionCurveDisplayObject(inurbsSubModel* pSubModel, inurbsPlanarCurve* pCurve)
{
	// get sub model display object
	SubModelDisplayObject *subModelDisplayObject = m_pLesionsModelDisplayObject->GetSubModelDisplayObject(pSubModel);

	// remove curve display object
	subModelDisplayObject->RemoveCurveDisplayObject(pCurve);

	UpdateSliceZ();
	UpdateWindows();
}

void FusionVisualEngine::SetActiveLesion(inurbsSubModel* pSubModel)
{
	m_pLesionModellingTransversalImageContext->SetActiveLesion(pSubModel);
	UpdateSliceZ();
	UpdateWindows();
}

inurbsSubModel* FusionVisualEngine::GetActiveLesion()
{
	return m_pLesionModellingTransversalImageContext->GetActiveLesion();
}

void FusionVisualEngine::SetProstateModelVisible(bool bVisible)
{
	m_pModelDisplayObject->SetSurfaceVisible(bVisible);
	m_pModelDisplayObject->SetCurveVisible(bVisible);
	UpdateWindows();
}

double FusionVisualEngine::GetLesionVolume(inurbsSubModel* pSubModel)
{
	SubModelDisplayObject *subModelDisplayObject = m_pLesionsModelDisplayObject->GetSubModelDisplayObject(pSubModel);
	if(subModelDisplayObject)
		return subModelDisplayObject->GetSurfaceVolume();

	return 0;
}

void FusionVisualEngine::UpdateLesionInfo(QString sLesionInfo)
{
	m_pModelDisplayObject->UpdateLesionInfo(sLesionInfo);
	UpdateWindows();
}

void FusionVisualEngine::CleanLesion()
{
	m_pLesionsModelDisplayObject->RemoveAllSubModelDisplayObjects();
	Update2DWindows();
}