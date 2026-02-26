/******************************************************************************
    FusionMainWindow.cpp

    Date      : 26 Oct 2018
 ******************************************************************************/

#include <QApplication>
#include <QFiledialog>
#include <itkTextOutput.h>
#include <vtkResliceImageViewer.h>

#include "PDP.h"
#include "Application.h"
#include "FusionMainWindow.h"
#include "ui_BaseMainWindow.h"
#include "CommonClasses.h"
#include "CreateCaseDialog.h"
#include "FusionSettingDialog.h"
#include "UrologyModel.h"
#include "LesionModellingTransversalImageContext.h"
#include "LesionModelInteractorSytle.h"
#include "DistanceMeasurementImageInteractorStyle.h"
#include "inurbsSubModel.h"
#include "OpenDICOMSeriesDialog.h"
#include "OpenCaseDialog.h"
#include "EncryptPasswordDialog.h"
#include "DecryptPasswordDialog.h"
#include "Crypto.h"
#include "ChooseLanguageDialog.h"
#include "QtGuiStyle.h"

#include <QMath.h>

/******************************************************************************/
/* Constructos and Destructors                                                                           
/******************************************************************************/

FusionMainWindow* FusionMainWindow::m_pInstance = 0;

FusionMainWindow::FusionMainWindow(QWidget *parent) : BaseMainWindow(parent)
{
	m_pVisualEngine = NULL;
	m_pSurgeryController = NULL;

	itk::OutputWindow::SetInstance(itk::TextOutput::New()); // set the output window to console
	vtkObject::GlobalWarningDisplayOff();

	m_bIsDicomDir = false;
	m_sSelectDir = "";
	m_sTempDir = "";
	m_sDecryptDicomPassword = "";

	// initialize as empty list
	m_imageFiles = QStringList();

	m_pDicomDirImporter = new DicomDirImporter;
	m_pSeriesModel = NULL; 
	m_pStackedWidget = nullptr;
	m_pLayout2DView = nullptr;
	m_pTransversal = nullptr;
	m_pCoronalWidget = nullptr;
	m_pSagitalWidget = nullptr;
	m_pSagCoronalWidget = nullptr;
	m_ptrtransversalWidget = nullptr;
	m_currentSagittalViewer = nullptr;	
	m_pCurSagitalWidget = nullptr;
	m_pCurCoronalWidget = nullptr;
	m_currentCoronalViewer = nullptr;
}

FusionMainWindow::~FusionMainWindow()
{
	if (m_pSeriesModel)
		delete m_pSeriesModel;

	if (m_pDicomDirImporter)
		delete m_pDicomDirImporter;

	// remove temp files
#ifdef PDP_DATA
	RemoveDir(GetFusionTempDir());
#endif
	FusionVisualEngine::DeleteInstance();
	FusionSurgeryController::DeleteInstance();
}

FusionMainWindow* FusionMainWindow::GetInstance()
{
	if (m_pInstance == 0)
		m_pInstance =  new FusionMainWindow();

	return m_pInstance;
}

void FusionMainWindow::DeleteInstance()
{
	if (m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = 0;
	}
}

/******************************************************************************/
/* Init functions                                                                         
/******************************************************************************/
void FusionMainWindow::InitConnections()
{
	BaseMainWindow::InitConnections();

	// connect interactor styles
	connect(m_pVisualEngine->GetLesionModellingTransversalImageContext()->GetLesionModelInteractorStyle(), SIGNAL(setLabelMessage(int)), this, SLOT(ShowMessageLabel(int)));
	connect(m_pVisualEngine->GetLesionModellingTransversalImageContext()->GetLesionModelInteractorStyle(), SIGNAL(clearWarningLabelMessage()), this, SLOT(ClearWarningMessageLabel()));
	connect(m_pVisualEngine->GetLesionModellingTransversalImageContext()->GetDistanceMeasurementImageInteractorStyle(), SIGNAL(doubleClicked()),this, SLOT(DisableMeasurementTool()));

	connect(ui->lesionShowModelSwitch, SIGNAL(valueChanged(int)), this, SLOT(LesionShowModelValueChanged(int)));
}

void FusionMainWindow::InitWidgets()
{
	BaseMainWindow::InitWidgets();

	setMinimumSize(1024, 960);

	// left panel
	ui->stageStart->setToolTip(tr("Create or open case"));
	ui->stageFinish->setToolTip(tr("Export patient case"));

	// top bar
	ui->needleLabel->setVisible(false);
	ui->typeLabel->setVisible(false);
	ui->modalityLabel->setVisible(false);

	// info bar
	ShowInfoBar(false);
	ui->progressBar->setTextVisible(true);
	ui->progressBar->hide();

	// center widgets
	ui->gridFrameMechanicalCalib->setVisible(false);

	// right panel
	ui->startInitRobot->setVisible(false);
	ui->startInitNeedleGuide->setVisible(false);
	ui->startApproveCase->setVisible(false);
	ui->modelTypeGroupBox->setVisible(false);
	ui->finishViewReport->setVisible(false);
	ui->finishCloseCase->setVisible(false);
	ui->finishExportReport->setVisible(false);
	ui->groupBoxStartGoto->setVisible(false);
	ui->windowLevelFrame->setVisible(false);
	ui->planDebugFrame->setVisible(false);
	ui->modelResetModel->setToolTip(tr("Delete all contours and discard prostate model"));
	ui->modelApproveModel->setText(tr("Approve Model"));
	ui->modelApproveModel->setToolTip(tr("Approve prostate model and proceed to Model Lesion"));

	QtGuiStyle::SetComboBoxHeight(ui->lesionPirads);
	UpdateSwitchStyle(ui->lesionShowModelSwitch);

	// function bar
	ui->functionLiveView->setVisible(false);
}

// back to previous stage
void FusionMainWindow::on_stageStart_clicked()
{
	Log("on_stageStart_clicked");
	GoBackToState(BaseSurgeryController::STATE_START);
}

void FusionMainWindow::on_stageImageImport_clicked()
{
	Log("on_stageImportImage_clicked");
	GoBackToState(BaseSurgeryController::STATE_IMPORT_IMAGE);
}

void FusionMainWindow::on_stageModel_clicked()
{
	Log("on_stageModel_clicked");
	GoBackToState(BaseSurgeryController::STATE_MODEL);
}

void FusionMainWindow::on_stageModelLesion_clicked()
{
	Log("on_stageModelLesion_clicked");
	GoBackToState(BaseSurgeryController::STATE_LESION);
}

void FusionMainWindow::on_stageFinish_clicked()
{
	Log("on_stageFinish_clicked");
	GoBackToState(BaseSurgeryController::STATE_FINISH);
}

void FusionMainWindow::GoBackToState(int state)
{
	if(m_pSurgeryController->GetState() == state)
		return;

	// show message box to clear data
	if(state == BaseSurgeryController::STATE_START)
	{
		if(ShowMessageBox(MSGBOX_WARNING_CLOSE_CASE) == QMessageBox::No)
			return;
		m_pSurgeryController->DeleteImageStack();
		ResetDicomDirImporter();
	}
	else if(state == BaseSurgeryController::STATE_IMPORT_IMAGE)
	{
		if(m_pSurgeryController->GetSubStateFlag(BaseSurgeryController::STATE_MODEL_FIRSTPOINT_CREATED) || m_pSurgeryController->GetSubStateFlag(BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE))
		{
			if(ShowMessageBox(MSGBOX_WARNING_CLEAR_DATA) == QMessageBox::No)
				return;
		}
	}
	else if(state == BaseSurgeryController::STATE_MODEL)
	{
		//if(m_pSurgeryController->GetSubStateFlag(BaseSurgeryController::STATE_LESION_FIRSTPOINT_CREATED) || m_pSurgeryController->GetSubStateFlag(BaseSurgeryController::STATE_LESION_CREATED))
		if (m_pSurgeryController->IsContainIncompleteLesionCurves())
		{
			if(ShowMessageBox(MSGBOX_WARNING_CLEAR_INCOMPLETE_CURVE) == QMessageBox::No)
				return;
		}
	}

	if(m_pSurgeryController->GetState() == BaseSurgeryController::STATE_LESION && ui->lesionShowModelSwitch->value() == 0)
		SetSwitchValue(ui->lesionShowModelSwitch, 1);

	// go back to stage
	Log(QString("go back to %1").arg(m_pSurgeryController->GetStateName(state, false)), UtlLogger::TEXT_INFO);
	ReleaseMeasurementTool();
	int iSubState = m_pSurgeryController->GoBackToState(state);

	if(state == BaseSurgeryController::STATE_START)
	{
		m_pSurgeryController->CloseCase();
		Log("case closed", UtlLogger::TEXT_INFO);

		ui->patientNameLabel->setText(tr("Patient: -"));
		ui->hospitalNameLabel->setText(tr("Hospital: -"));
	}
	else
	{
		SwitchViewTo(DISPLAY_2D);

		if(iSubState == BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED)
			EnableNormalButton(ui->imageImportSelectSeries, false);
	}
}

/******************************************************************************/
/* resize functions                                                                         
/******************************************************************************/

void FusionMainWindow::resizeEvent(QResizeEvent* event)
{
    BaseMainWindow::resizeEvent(event);

	if(!m_pVisualEngine) return;

	int iStackedWidgetW, iStackedWidgetH;
	int iTransversalWidth, iSagCorWidthWidget, iSagCorHeightWidget;
	GetLayoutSize(iStackedWidgetW, iStackedWidgetH, iTransversalWidth, iSagCorWidthWidget, iSagCorHeightWidget);

	// 3D view
	ResizeVtkWidget(ui->threeDViewWidget, iStackedWidgetW, iStackedWidgetH, m_pVisualEngine->GetVirtualContext());

	// 2D views
	ResizeVtkWidget(ui->modellingImageContextWidget, iTransversalWidth, iStackedWidgetH, m_pVisualEngine->GetModellingTransversalImageContext());
	ui->transversalModelImageChangeSlice->raise();
	ResizeVtkWidget(ui->lesionImageContextWidget, iTransversalWidth, iStackedWidgetH, m_pVisualEngine->GetLesionModellingTransversalImageContext());
	ui->transversalLesionImageChangeSlice->raise();
	ResizeVtkWidget(ui->sagittalImageContextWidget, iSagCorWidthWidget, iSagCorHeightWidget, m_pVisualEngine->GetSagittalImageContext());
	ui->sagittalImageChangeSlice->raise();
	ResizeVtkWidget(ui->coronalImageContextWidget, iSagCorWidthWidget, iSagCorHeightWidget, m_pVisualEngine->GetCoronalImageContext());
	ui->coronalImageChangeSlice->raise();
}

/******************************************************************************/
/* State functions                                                                         
/******************************************************************************/

void FusionMainWindow::UpdateState(int state, int subState)
{
	BaseMainWindow::UpdateState(state, subState);

	switch (state)
	{
		case BaseSurgeryController::STATE_START:
			SetGraphicPanelStateAndDisplay(state,subState,DISPLAY_DEFAULT);
		break;

		case BaseSurgeryController::STATE_IMPORT_IMAGE:
			switch (subState)
			{
				case BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST:
					SetGraphicPanelStateAndDisplay(state,subState,DISPLAY_DEFAULT);
				break;

				case BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED:
					SetGraphicPanelStateAndDisplay(state,subState,DISPLAY_2D);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_MODEL:
			switch (subState)
			{
				case BaseSurgeryController::STATE_MODEL_FIRST:
				case BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE:
					SetGraphicPanelStateAndDisplay(state,subState,DISPLAY_2D);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_LESION:
			switch (subState)
			{
				case BaseSurgeryController::STATE_LESION_FIRST:
				case BaseSurgeryController::STATE_LESION_MODEL_CREATED:
					SetGraphicPanelStateAndDisplay(state,subState,DISPLAY_2D);
					SetSwitchValue(ui->lesionShowModelSwitch, 1);
					break;

				default:
				break;
			}
		break;

		default:
		break;
	}

	SetDefaultPanelControl();
	SetStageLayout(state, subState);
	SetRightFrame(state, subState);
	SetFunctionFrame(state, subState);
	SetMessageLabel(state, subState);
}

void FusionMainWindow::SetStageLayout(int state, int subState)
{
	EnableStageLayout(!m_pSurgeryController->IsAutoModelling());

	switch (state)
	{
		case BaseSurgeryController::STATE_START:
			ShowStageButton(ui->stageStart, BUTTON_HIGHLIGHT);
			ShowStageButton(ui->stageImageImport, BUTTON_DISABLE);
			ShowStageButton(ui->stageModel, BUTTON_DISABLE);
			ShowStageButton(ui->stageModelLesion, BUTTON_DISABLE);
			ShowStageButton(ui->stageFinish, BUTTON_DISABLE);
		break;

		case BaseSurgeryController::STATE_IMPORT_IMAGE:
			ShowStageButton(ui->stageStart, BUTTON_ENABLE);
			ShowStageButton(ui->stageImageImport, BUTTON_HIGHLIGHT);
			ShowStageButton(ui->stageModel, BUTTON_DISABLE);
			ShowStageButton(ui->stageModelLesion, BUTTON_DISABLE);
			ShowStageButton(ui->stageFinish, BUTTON_DISABLE);
		break;

		case BaseSurgeryController::STATE_MODEL:
			ShowStageButton(ui->stageStart, BUTTON_ENABLE);
			ShowStageButton(ui->stageImageImport, BUTTON_ENABLE);
			ShowStageButton(ui->stageModel, BUTTON_HIGHLIGHT);
			ShowStageButton(ui->stageModelLesion, BUTTON_DISABLE);
			ShowStageButton(ui->stageFinish, BUTTON_DISABLE);
		break;

		case BaseSurgeryController::STATE_LESION:
			ShowStageButton(ui->stageStart, BUTTON_ENABLE);
			ShowStageButton(ui->stageImageImport, BUTTON_ENABLE);
			ShowStageButton(ui->stageModel, BUTTON_ENABLE);
			ShowStageButton(ui->stageModelLesion, BUTTON_HIGHLIGHT);
			ShowStageButton(ui->stageFinish, BUTTON_DISABLE);
		break;

		case BaseSurgeryController::STATE_FINISH:
			ShowStageButton(ui->stageStart, BUTTON_ENABLE);
			ShowStageButton(ui->stageImageImport, BUTTON_ENABLE);
			ShowStageButton(ui->stageModel, BUTTON_ENABLE);
			ShowStageButton(ui->stageModelLesion, BUTTON_ENABLE);
			ShowStageButton(ui->stageFinish, BUTTON_HIGHLIGHT);
		break;

		default:
		break;
	}
}

void FusionMainWindow::SetRightFrame(int state, int subState)
{
	EnableActionStackedWidget(!m_pSurgeryController->IsAutoModelling());

	ui->windowLevelFrame->setVisible(state == BaseSurgeryController::STATE_MODEL || state == BaseSurgeryController::STATE_FUSION);

	switch (state)
	{
		case BaseSurgeryController::STATE_START:
		{
			ui->actionStackedWidget->setCurrentWidget(ui->startLayout);
			ui->windowLevelFrame->setVisible(false);

			switch (subState)
			{
				case BaseSurgeryController::STATE_START_FIRST:
					EnableNormalButton(ui->startCreateCase, true); 
					EnableNormalButton(ui->startOpenCase, true);
				break;

				default:
				break;
			}
		}
		break;

		case BaseSurgeryController::STATE_IMPORT_IMAGE:
		{
			ui->actionStackedWidget->setCurrentWidget(ui->imageImportLayout);
			ui->windowLevelFrame->setVisible(false);

			switch (subState)
			{
				case BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST:
					EnableNormalButton(ui->imageImportMRIImages);
					EnableNormalButton(ui->imageImportSelectSeries, false);
					EnableNormalButton(ui->imageImportFlipAntPos, false);
					EnableNormalButton(ui->imageImportFlipApexBase, false);
					EnableNormalButton(ui->imageImportResetImage, false);
					EnableNormalButton(ui->imageImportApproveImage, false);
				break;

				case BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED:
					EnableNormalButton(ui->imageImportFlipAntPos);
					EnableNormalButton(ui->imageImportFlipApexBase);
					EnableNormalButton(ui->imageImportResetImage);
					EnableNormalButton(ui->imageImportApproveImage);
				break;

				default:
				break;
			}
		}
		break;

		case BaseSurgeryController::STATE_MODEL:
			ui->actionStackedWidget->setCurrentWidget(ui->modelLayout);
			ui->windowLevelFrame->setVisible(true);

			switch (subState)
			{
				case BaseSurgeryController::STATE_MODEL_FIRST:
					DisableOtherModelTypes();
					ui->groupBoxModel->setEnabled(false);
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelPreviousContour,false);
					EnableNormalButton(ui->modelNextContour,false);
					EnableNormalButton(ui->modelDeleteCurve,false);
					EnableNormalButton(ui->modelResetModel,false);
					EnableNormalButton(ui->modelApproveModel,false);
					EnableNormalButton(ui->modelLoadRTStruct);
					SetModelType(MODEL_PROSTATE);
				break;

				case BaseSurgeryController::STATE_MODEL_FIRSTPOINT_CREATED:
					DisableOtherModelTypes();
					ui->groupBoxModel->setEnabled(false);
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelPreviousContour,false);
					EnableNormalButton(ui->modelNextContour,false);
					EnableNormalButton(ui->modelDeleteCurve,false);
					EnableNormalButton(ui->modelResetModel,false);
					EnableNormalButton(ui->modelApproveModel,false);
					EnableNormalButton(ui->modelLoadRTStruct, false);
				break;

				case BaseSurgeryController::STATE_MODEL_FIRSTCURVE_CREATED:
					ui->groupBoxModel->setEnabled(true);
					EnableNormalButton(ui->modelAutoModel);
					EnableNormalButton(ui->modelPreviousContour);
					EnableNormalButton(ui->modelNextContour);
					EnableNormalButton(ui->modelDeleteCurve);
					EnableNormalButton(ui->modelResetModel,false);
					EnableNormalButton(ui->modelApproveModel,false);					
				break;

				case BaseSurgeryController::STATE_MODEL_AUTOMODELLING:
					ui->groupBoxModel->setEnabled(false);
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelPreviousContour,false);
					EnableNormalButton(ui->modelNextContour,false);
					EnableNormalButton(ui->modelDeleteCurve,false);
					EnableNormalButton(ui->modelResetModel,false);
					EnableNormalButton(ui->modelApproveModel,false);					
				break;

				case BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE:
					EnableAllModelTypes();
					SetProgressValue(0);
					ui->groupBoxModel->setEnabled(true);
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelLoadRTStruct, false);
					EnableNormalButton(ui->modelPreviousContour);
					EnableNormalButton(ui->modelNextContour);
					EnableNormalButton(ui->modelDeleteCurve);
					EnableNormalButton(ui->modelResetModel);
					EnableNormalButton(ui->modelApproveModel);
				break;

				case BaseSurgeryController::STATE_MODEL_SUBMODEL_FIRST:
					ui->groupBoxModel->setEnabled(false);
					EnableAllModelTypes();
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelPreviousContour,false);
					EnableNormalButton(ui->modelNextContour,false);
					EnableNormalButton(ui->modelDeleteCurve,false);
					EnableNormalButton(ui->modelResetModel,false);
					EnableNormalButton(ui->modelApproveModel,false);
				break;

				case BaseSurgeryController::STATE_MODEL_SUBMODEL_FIRSTCURVE_CREATED:
					ui->groupBoxModel->setEnabled(true);
					DisableOtherModelTypes(); // some create a whole curve at once, e.g., urethra
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelPreviousContour);
					EnableNormalButton(ui->modelNextContour);
					EnableNormalButton(ui->modelDeleteCurve);
					EnableNormalButton(ui->modelResetModel,false);
					EnableNormalButton(ui->modelApproveModel,false);
				break;

				case BaseSurgeryController::STATE_MODEL_SUBMODEL_MODEL_CREATED:
					ui->groupBoxModel->setEnabled(true);
					EnableAllModelTypes();
					EnableNormalButton(ui->modelAutoModel,false);
					EnableNormalButton(ui->modelPreviousContour);
					EnableNormalButton(ui->modelNextContour);
					EnableNormalButton(ui->modelDeleteCurve);
					EnableNormalButton(ui->modelResetModel);
					EnableNormalButton(ui->modelApproveModel);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_LESION:
			ui->actionStackedWidget->setCurrentWidget(ui->lesionLayout);
			ui->windowLevelFrame->setVisible(true);

			switch (subState)
			{
				case BaseSurgeryController::STATE_LESION_FIRST:
					EnableNormalButton(ui->lesionCreateLesion);
					EnableNormalButton(ui->lesionDeleteLesion,false);
					EnableNormalButton(ui->lesionDeleteContour,false);
					ui->modelLesionPiradsLabel->setEnabled(false);
					ui->lesionPirads->setEnabled(false);
					ui->lesionRemarks->setEnabled(true);
					ui->lesionRemarks->clear();
					ui->lesionShowModelLabel->setEnabled(true);
					ui->lesionShowModelSwitch->setEnabled(true);
					EnableNormalButton(ui->lesionPreviousLesion,false);
					EnableNormalButton(ui->lesionNextLesion,false);
					EnableNormalButton(ui->lesionApproveLesionModels,false);

				break;

				case BaseSurgeryController::STATE_LESION_CREATED:
					EnableNormalButton(ui->lesionCreateLesion,false);
					EnableNormalButton(ui->lesionDeleteLesion);
					EnableNormalButton(ui->lesionDeleteContour);
					ui->modelLesionPiradsLabel->setEnabled(true);
					ui->lesionPirads->setEnabled(true);
					ui->lesionShowModelLabel->setEnabled(true);
					ui->lesionShowModelSwitch->setEnabled(true);
					EnableNormalButton(ui->lesionPreviousLesion,false);
					EnableNormalButton(ui->lesionNextLesion,false);
					EnableNormalButton(ui->lesionApproveLesionModels,false);
				break;

				case BaseSurgeryController::STATE_LESION_FIRSTCURVE_CREATED:
					//EnableButton(ui->lesionCreateLesion,false);
					EnableNormalButton(ui->lesionCreateLesion);
					EnableNormalButton(ui->lesionDeleteLesion);
					EnableNormalButton(ui->lesionDeleteContour);
					ui->modelLesionPiradsLabel->setEnabled(true);
					ui->lesionPirads->setEnabled(true);
					ui->lesionShowModelLabel->setEnabled(true);
					ui->lesionShowModelSwitch->setEnabled(true);
					//EnableButton(ui->lesionPreviousLesion,false);
					//EnableButton(ui->lesionNextLesion,false);
					SetLesionNavigationButtons(); // previous and next
					EnableNormalButton(ui->lesionApproveLesionModels,false);
				break;

				case BaseSurgeryController::STATE_LESION_MODEL_CREATED:
					SetLesionNavigationButtons(); // previous and next
					EnableNormalButton(ui->lesionCreateLesion);
					EnableNormalButton(ui->lesionDeleteLesion);
					EnableNormalButton(ui->lesionDeleteContour);
					ui->modelLesionPiradsLabel->setEnabled(true);
					ui->lesionPirads->setEnabled(true);
					ui->lesionShowModelLabel->setEnabled(true);
					ui->lesionShowModelSwitch->setEnabled(true);
					EnableNormalButton(ui->lesionApproveLesionModels);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_FINISH:
			ui->actionStackedWidget->setCurrentWidget(ui->finishLayout);
			ui->windowLevelFrame->setVisible(false);
		break;

		default:
		break;
	}
}

void FusionMainWindow::SetFunctionFrame(int state, int subState)
{
	ui->functionFrame->setEnabled(!m_pSurgeryController->IsAutoModelling());

	switch (state)
	{
		case BaseSurgeryController::STATE_START:
			EnableFunctionButton(ui->functionMenuSystem);
			EnableFunctionButton(ui->functionMenuTool);
			EnableFunctionButton(ui->function2DView, false);
			EnableFunctionButton(ui->function3DView, false);
			EnableFunctionButton(ui->functionPrintScreen, false);
			EnableFunctionButton(ui->functionPan, false);
			EnableFunctionButton(ui->functionZoom, false);
			EnableFunctionButton(ui->functionRotate, false);
			EnableFunctionButton(ui->functionResetView, false);
			EnableMeasurementToolButton(false);
		break;

		case BaseSurgeryController::STATE_IMPORT_IMAGE:
			switch (subState)
			{
				case BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST:
					EnableFunctionButton(ui->function2DView, false);
					EnableFunctionButton(ui->function3DView, false);
					EnableFunctionButton(ui->functionPrintScreen, false);
					EnableFunctionButton(ui->functionPan, false);
					EnableFunctionButton(ui->functionZoom, false);
					EnableFunctionButton(ui->functionRotate, false);
					EnableFunctionButton(ui->functionResetView, false);
					EnableMeasurementToolButton(false);
				break;

				case BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED:
					EnableFunctionButton(ui->function2DView);
					EnableFunctionButton(ui->function3DView);
					EnableFunctionButton(ui->functionPrintScreen);
					EnableFunctionButton(ui->functionPan);
					EnableFunctionButton(ui->functionZoom);
					EnableFunctionButton(ui->functionResetView);
					EnableMeasurementToolButton(true);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_MODEL:
			switch (subState)
			{
				case BaseSurgeryController::STATE_MODEL_FIRST:
				case BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE:
					EnableFunctionButton(ui->function2DView);
					EnableFunctionButton(ui->function3DView);
					EnableFunctionButton(ui->functionPrintScreen);
					EnableFunctionButton(ui->functionPan);
					EnableFunctionButton(ui->functionZoom);
					EnableFunctionButton(ui->functionResetView);
					EnableMeasurementToolButton(true);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_LESION:
			switch (subState)
			{
				case BaseSurgeryController::STATE_LESION_FIRST:
				case BaseSurgeryController::STATE_LESION_MODEL_CREATED:
					EnableFunctionButton(ui->function2DView);
					EnableFunctionButton(ui->function3DView);
					EnableFunctionButton(ui->functionPrintScreen);
					EnableFunctionButton(ui->functionPan);
					EnableFunctionButton(ui->functionZoom);
					EnableFunctionButton(ui->functionResetView);
					EnableMeasurementToolButton(true);
				break;

				default:
				break;
			}
		break;

		default:
		break;
	}
}

void FusionMainWindow::SetMessageLabel(int state, int subState)
{
	switch (state)
	{
		case BaseSurgeryController::STATE_START:
			switch (subState)
			{
				case BaseSurgeryController::STATE_START_FIRST:
					ShowMessageLabel(MSGLABEL_UROFUSION_TO_START);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_IMPORT_IMAGE:
			switch (subState)
			{
				case BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST:
					ShowMessageLabel(MSGLABEL_CLICK_IMPORT);
				break;

				case BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED:
					ShowMessageLabel(MSGLABEL_CLICK_IMAGE_LOADED);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_MODEL:

			switch(subState)
			{
				case BaseSurgeryController::STATE_MODEL_FIRST:
					ShowMessageLabel(MSGLABEL_MODEL_TO_START);
				break;

				case BaseSurgeryController::STATE_MODEL_FIRSTCURVE_CREATED:
					ShowMessageLabel(MSGLABEL_MODEL_TO_SET_APEX_BASE_LIMIT);
				break;

				case BaseSurgeryController::STATE_MODEL_APEX_SET:
					ShowMessageLabel(MSGLABEL_MODEL_TO_SET_APEX_BASE_LIMIT);
				break;

				case BaseSurgeryController::STATE_MODEL_BASE_SET:
					ShowMessageLabel(MSGLABEL_MODEL_TO_SET_APEX_BASE_LIMIT);
				break;

				case BaseSurgeryController::STATE_MODEL_APEX_BASE_SET:
					ShowMessageLabel(MSGLABEL_MODEL_TO_AUTOMODEL);
				break;

				case BaseSurgeryController::STATE_MODEL_AUTOMODELLING:
					ShowMessageLabel(MSGLABEL_MODEL_AUTOMODELLING);
				break;

				case BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE:
				case BaseSurgeryController::STATE_MODEL_SUBMODEL_MODEL_CREATED:
					ShowMessageLabel(MSGLABEL_MODEL_TO_APPROVE);
					SetProgressValue(0); // put this here first
				break;

				case BaseSurgeryController::STATE_MODEL_SUBMODEL_FIRSTCURVE_CREATED:
					ShowMessageLabel(MSGLABEL_MODEL_MIN_NUM_CONTOURS);
				break;

				case BaseSurgeryController::STATE_MODEL_SUBMODEL_FIRST:
				case BaseSurgeryController::STATE_MODEL_SUBMODEL_FIRSTPOINT_CREATED:
					ShowMessageLabel(MSGLABEL_MODEL_SUBMODEL_TO_ADD);
				break;

				default:
				break;
			}
		break;

		case BaseSurgeryController::STATE_LESION:
			if (subState == BaseSurgeryController::STATE_LESION_FIRST)
				ShowMessageLabel(MSGLABEL_LESION_TO_START);
			else if (subState == BaseSurgeryController::STATE_LESION_CREATED || subState == BaseSurgeryController::STATE_LESION_FIRSTCURVE_CREATED)
				ShowMessageLabel(MSGLABEL_LESION_TO_CREATE);
			else if (subState == BaseSurgeryController::STATE_LESION_MODEL_CREATED)
				ShowMessageLabel(MSGLABEL_LESION_MODEL_CREATED);
		break;

		case BaseSurgeryController::STATE_FINISH:
			if (subState == BaseSurgeryController::STATE_FINISH_FIRST)
				ShowMessageLabel(MSGLABEL_FUSION_CASE_TO_EXPORT);
		break;

		default:
		break;
	}
}

void FusionMainWindow::EnterLesionStage()
{
	//inurbsSubModel* pCurLesion;
	//pCurLesion = m_pVisualEngine->GetActiveLesion();

	m_pSurgeryController->RemoveInvalidModels();
	m_pSurgeryController->SetFirstLesionAsActive();
	if (m_pSurgeryController->GetNumLesionSubModels() >= 1)
	{
		// already lesion is present
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_LESION, BaseSurgeryController::STATE_LESION_MODEL_CREATED);
		m_pSurgeryController->UpdateLesionInfo();
		m_pVisualEngine->ShowActiveLesionCentre();
	}
	else
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_LESION, BaseSurgeryController::STATE_LESION_FIRST);

	m_pVisualEngine->UpdateSliceZ();
	m_pVisualEngine->UpdateWindows();

}

/******************************************************************************/
/* Graphic panel functions                                                                         
/******************************************************************************/

void FusionMainWindow::SwitchViewTo(int iView)
{
	int iState = m_pSurgeryController->GetState();
	int iSubState = m_pSurgeryController->GetSubState();

	DisableMeasurementTool();

	SetGraphicPanelStateAndDisplay(iState,iSubState,iView);
	switch (iView)
	{
		case DISPLAY_2D:
			EnableMeasurementToolButton(true);
		break;

		case DISPLAY_3D:
			EnableMeasurementToolButton(false);
		break;

		default:
		break;
	}

	m_pVisualEngine->UpdateWindows();
}

void FusionMainWindow::on_startCreateCase_clicked()
{
	Log("on_startCreateCase_clicked");

	if (!m_pSurgeryController)
		return;
	
	CreateCaseDialog dialog(this);

	if (Translation::m_iCurrentLanguage == Translation::LANG_ZH_CN)
		dialog.setLocale(QLocale::Chinese);

	dialog.SetMinPasswordLength(m_UserAccount.GetPasswordMinLength());
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

	dialog.SetPatientNationality(m_pSurgeryController->GetPatientNationality());
	dialog.Init();
	dialog.adjustSize();
	dialog.setFixedSize(dialog.frameSize());

	if (dialog.exec() == QDialog::Rejected)
		return;

	QString sPatientDataFolder = m_pSurgeryController->GetPatientDataFolder();
	if(!m_pSurgeryController->CreateSurgery(sPatientDataFolder, dialog.GetCaseId(), m_sAppVersion + "." + m_sAppSvn))
	{
		ShowMessageBox(MSGBOX_CASE_CREATION_FAILED);
		return;
	}

	FusionSurgery *pSurgery = m_pSurgeryController->GetSurgery();
	pSurgery->SetPatientName(dialog.GetPatientName());
	pSurgery->SetPatientId(dialog.GetPatientId());
	pSurgery->SetPatientDOB(dialog.GetPatientDOB());
	pSurgery->SetPatientNationality(dialog.GetPatientNationality());
	pSurgery->SetSurgeonName(dialog.GetSurgeonName());
	pSurgery->SetSurgeonClinic(dialog.GetSurgeonClinic());
	//pSurgery->SetSurgeryType(dialog.GetSurgeryType());
	pSurgery->SetPatientGender(tr("Male"));
	pSurgery->SetEncryptCasePassword(dialog.GetEncryptCasePassword());
	pSurgery->Write();

#ifdef PDP_USER
	pSurgery->SetCreatorUser(m_UserAccount.GetLoginUsername(), m_UserAccount.GetLoginUserType());
	pSurgery->WriteUserXml();
#endif

	m_pSurgeryController->SetPatientNationality(dialog.GetPatientNationality());

	ui->patientNameLabel->setText(tr("Patient: ") + dialog.GetPatientName());
	ui->hospitalNameLabel->setText(tr("Hospital: ") + dialog.GetSurgeonClinic());

	GetLogger()->SetCaseLogger(pSurgery->GetCasePath());

	QString sLog = "case '" + pSurgery->GetSurgeryId() + "' created";
	if(m_UserAccount.GetLoginUsername() != "")
		sLog += " by " + m_UserAccount.GetLoginUsername();

	Log(sLog, UtlLogger::TEXT_INFO);

	// for progress bar in the import image stage
	connect(m_pSurgeryController->GetSurgery(), SIGNAL(progressChanged(int)), this, SLOT(SetProgressValue(int)));
	m_pSurgeryController->SetState(FusionSurgeryController::STATE_IMPORT_IMAGE, FusionSurgeryController::STATE_IMPORT_IMAGE_FIRST);

	LoadDicomSequecne();
}

// Load selected mri sequence
void FusionMainWindow::LoadDicomSequecne()
{
	if (m_selectedSeriesFiles.count() > 1) {
		UploadMultiSequenceDicom();
	}
	else if (m_selectedSeriesFiles.count() == 1)
	{
		if (m_ptrtransversalWidget != nullptr)
		{
			MultiSeqWidgetInit();
			//del transversalWidget from QStackedWidgetdel
			m_pTransversal->removeWidget(m_ptrtransversalWidget);
			delete m_ptrtransversalWidget;
			m_ptrtransversalWidget = nullptr;

			QWidget* modellingImageContextWidget = m_pTransversal->findChild<QWidget*>("modellingImageContextWidget");
			m_pTransversal->setCurrentWidget(modellingImageContextWidget);

			m_currentCoronalViewer->Delete();
			m_currentCoronalViewer = nullptr;
			delete m_pCurCoronalWidget;
			m_pCurCoronalWidget = nullptr;			
			
			m_currentSagittalViewer->Delete();
			m_currentSagittalViewer = nullptr;
			delete m_pCurSagitalWidget;
			m_pCurSagitalWidget = nullptr;

			m_pCoronalWidget->setVisible(true);
			m_pSagitalWidget->setVisible(true);
		}
		UploadImportedDicom();
	}		
	
}

void FusionMainWindow::on_startOpenCase_clicked()
{
	Log("on_startOpenCase_clicked");

	QString sDrive = "C:/";
	QFileInfoList driveList = QDir::drives();
	for (int i = 0; i < driveList.length(); i++)
	{
		QString sPatientDataPath = driveList[i].filePath() + SYSTEM_PATIENTDATA_RELATIVE_PATH;
		if(QDir(sPatientDataPath).exists())
		{
			sDrive = driveList[i].filePath();
			break;
		}
	}

	OpenCaseDialog dialog(this, sDrive, m_UserAccount.GetLoginUserType());
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	dialog.setFixedSize(dialog.frameSize());
	QString sDecryptCasePassword;

	if (dialog.exec() == QDialog::Rejected) 
		return;
	else
	{
		// first set the decrypt password to the same as the password used to encrypt the case
		sDecryptCasePassword = dialog.GetDecryptCasePassword();

#ifdef PDP_DATA
		// if password is "", it means that the case is unencrypted or it is encrypted with hardcoded password.
		if (dialog.GetDecryptCasePassword() == "") 
		{
			EncryptPasswordDialog epDialog(this, dialog.GetDecryptCasePassword());
			epDialog.SetMinPasswordLength(m_UserAccount.GetPasswordMinLength());
			epDialog.setWindowFlags(epDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
			epDialog.adjustSize();
			epDialog.setFixedSize(epDialog.frameSize());
			if (epDialog.exec() == QDialog::Rejected) // cancel
				return;
			else 
			{
				// encrypt case
				if (m_pSurgeryController->EncryptCase(dialog.GetSelectedFolderPath(), dialog.GetDecryptCasePassword(), epDialog.GetEncryptCasePassword()))
				{
					// if successfully encrypt, the loading will use the new password to decrypt. 
					sDecryptCasePassword = epDialog.GetEncryptCasePassword();
				}					
			}
		}
#endif
	}

	QString sCaseFolder = dialog.GetSelectedFolderPath();

	// load UroFusion case		
	if (!m_pSurgeryController->OpenCase(sCaseFolder, sDecryptCasePassword))
		ShowMessageBox(MSGBOX_INVALID_CASE);

	// show patient details
	UpdatePatientDetails();

	// for progress bar in the import image stage
	int iState = m_pSurgeryController->GetState();
	if (iState >= BaseSurgeryController::STATE_IMPORT_IMAGE)
		connect(m_pSurgeryController->GetSurgery(), SIGNAL(progressChanged(int)), this, SLOT(SetProgressValue(int)));
	if (iState >= BaseSurgeryController::STATE_MODEL)
	{
		connect(m_pSurgeryController->GetAutoModel(), SIGNAL(progressChanged(int)), this, SLOT(SetProgressValue(int)));
		ResetWindowLevel(true);
	}

	m_pVisualEngine->ResetAllViews();


	FusionSurgery *pSurgery = m_pSurgeryController->GetSurgery();
	GetLogger()->SetCaseLogger(pSurgery->GetCasePath(), sDecryptCasePassword);

	QString sLog = "case '" + pSurgery->GetSurgeryId() + "' opened";
	if(m_UserAccount.GetLoginUsername() != "")
		sLog += " by " + m_UserAccount.GetLoginUsername();

	Log(sLog, UtlLogger::TEXT_INFO);
}

void FusionMainWindow::UpdatePatientDetails()
{
	FusionSurgery *pSurgery = m_pSurgeryController->GetSurgery();
	if(pSurgery)
	{
		ui->patientNameLabel->setText(tr("Patient: ") + pSurgery->GetPatientName());
		ui->hospitalNameLabel->setText(tr("Hospital: ") + pSurgery->GetSurgeonClinic());
	}
	else
	{
		ui->patientNameLabel->setText(tr("Patient: -"));
		ui->hospitalNameLabel->setText(tr("Hospital: -"));
	}
}

/******************************************************************************/
/* Import Image functions                                                                         
/******************************************************************************/

QString FusionMainWindow::GetFirstDicomDir(QStringList files)
{
	m_bIsDicomDir = false;
	QString strDicomDirFile = "";
	foreach(QString str, files)
	{
		QFileInfo fpath(str);
		QString fname = fpath.fileName();
		if (fname.compare("DICOMDIR") == 0) //other way to identify the file if it is DICOMDIR aside from FIle name?
		{
			m_bIsDicomDir = true;
			return str;
		}
	}

	return "";
}
void FusionMainWindow::AnalyzeStudySeries(const QStringList& name)
{
	m_bMoreThanOneSeries = false;
	m_bIsMultiFrame = false;
	m_bLoadImage = true;

	const int ntotalRow = name.count();
	const QString default_val = "";
	m_seriesVec.clear();
	m_seriesVec =  QVector < QVector< QString > >(ntotalRow, QVector < QString >(19, "0"));

	QMap<int, int> mapFileCount;
	QMap<int, int> mapRowToRefer;
	ImageIOType::Pointer dicomIO;
	ReaderType::Pointer reader;


	m_mapSeriesID.clear();
	m_mapStudyID.clear();
	mapFileCount.clear();
	m_multiFrameMap.clear();

	///////////////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < ntotalRow; i++)
	{
		dicomIO = ImageIOType::New();
		reader = ReaderType::New();
		reader->SetImageIO(dicomIO);

		bool bEncrypted = false;
		QString sPassword = "";
		if (sPassword != "")
			bEncrypted = PdpDecrypt2(name.at(i), sPassword);

		QString decryptFile = name.at(i);
		if (decryptFile.right(4) == ENCRYPTION_FILE_EXTENSION)
			decryptFile.truncate(decryptFile.lastIndexOf(QChar('.')));

		//QFileInfo qfileInfo(name.at(i));
		QFileInfo qfileInfo(decryptFile);

		QString qfilename = qfileInfo.fileName();

		//reader->SetFileName(name.at(i).toLocal8Bit().data());
		reader->SetFileName(decryptFile.toLocal8Bit().data());

		try
		{
			reader->Update();
		}
		catch (itk::ExceptionObject&)
		{
			// remove even if there is error reading
			if (bEncrypted)
				PdpRemove(decryptFile);
			continue;
		}

		// once read, delete
		if (bEncrypted)
			PdpRemove(decryptFile);
		m_seriesVec[i][0] = name.at(i);
		//vec[i][0] = decryptFile;

		typedef itk::MetaDataDictionary DictionaryType;
		DictionaryType & dictionary = dicomIO->GetMetaDataDictionary();
		typedef itk::MetaDataObject< std::string > MetaDataStringType;
		DictionaryType::ConstIterator itr = dictionary.Begin();
		DictionaryType::ConstIterator end = dictionary.End();

		//		int h = 0;

				// initialize fields of Series dialog box to "" instead of "0"
		m_seriesVec[i][5] = ""; // Series Description
		while (itr != end)
		{
			itk::MetaDataObjectBase::Pointer entry = itr->second;
			MetaDataStringType::Pointer entryvalue = dynamic_cast <MetaDataStringType *>(entry.GetPointer());

			if (entryvalue)
			{
				std::string tagkey = itr->first;
				std::string labelId;
				bool found = itk::GDCMImageIO::GetLabelFromTag(tagkey, labelId);

				std::string tagvalue = entryvalue->GetMetaDataObjectValue();

				if (found)
				{
					if (tagkey == "0020|000d" || labelId == "Study Instance UID")
						m_seriesVec[i][1] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0020|000e" || labelId == "Series Instance UID")
						m_seriesVec[i][2] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0010|0010" || labelId == "Patient's Name")
						m_seriesVec[i][3] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0008|1030" || labelId == "Study Description")
						m_seriesVec[i][4] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0008|103e" || labelId == "Series Description")
						m_seriesVec[i][5] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0008|0020" || labelId == "Study Date")
						m_seriesVec[i][6] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0008|0060" || labelId == "Modality")
						m_seriesVec[i][7] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|1050" || labelId == "Window Center")
						m_seriesVec[i][8] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|1051" || labelId == "Window Width")
						m_seriesVec[i][9] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|0101" || labelId == "Bits Stored")
						m_seriesVec[i][10] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|0106" || labelId == "Smallest Image Pixel Value")
						m_seriesVec[i][11] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|0107" || labelId == "Largest Image Pixel Value")
						m_seriesVec[i][12] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|1053" || labelId == "Rescale Slope")
						m_seriesVec[i][13] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|1052" || labelId == "Rescale Intercept")
						m_seriesVec[i][14] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0028|0103" || labelId == "Pixel Representation")
						m_seriesVec[i][15] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0020|0037" || labelId == "Image Orientation (Patient)")
						m_seriesVec[i][16] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0010|0020" || labelId == "PatientID")
						m_seriesVec[i][17] = QString::fromLocal8Bit(tagvalue.c_str());
					else if (tagkey == "0010|0030" || labelId == "PatientBirthDate")
						m_seriesVec[i][18] = QString::fromLocal8Bit(tagvalue.c_str());

					// to get number of frames
					else if (tagkey == "0028|0008" || labelId == "Number of Frames")
					{
						QString nf = QString::fromLocal8Bit(tagvalue.c_str());
						if (nf.toInt() > 1)
						{
							m_bIsMultiFrame = true;
						}
						else
							m_bIsMultiFrame = false;
					}
					//
				}
				//				++itr; h++;
			}
			++itr;
		}

		int nIndex = -1;
		for (int j = 0; j < m_mapStudyID.count(); j++)
		{
			QString strStudyID = m_mapStudyID.value(j);
			if (strStudyID == m_seriesVec[i][1])
			{
				if (m_mapSeriesID.value(j) == m_seriesVec[i][2])
				{
					nIndex = j;
					break;
				}
			}
		}

		if (nIndex != -1)
		{
			int nTemp = mapFileCount.value(nIndex);
			nTemp += 1;
			mapFileCount.insert(nIndex, nTemp);
		}
		else
		{
			int nSize = m_mapSeriesID.size();
			m_mapStudyID.insert(nSize, m_seriesVec[i][1]);
			QString studyID = m_seriesVec[i][1];
			m_mapSeriesID.insert(nSize, m_seriesVec[i][2]);
			mapFileCount.insert(nSize, 1);
			mapRowToRefer.insert(nSize, i);
			m_multiFrameMap.insert(nSize, m_bIsMultiFrame);

			ImageEntity seriesEntity(nSize);
			seriesEntity.m_metaData.SetWindowCenter(m_seriesVec[i][8]);
			seriesEntity.m_metaData.SetWindowWidth(m_seriesVec[i][9]);
			seriesEntity.m_metaData.m_seriesDesc = m_seriesVec[i][5];
			seriesEntity.m_metaData.m_seriesUid = m_seriesVec[i][2];
			m_mapSeriesImageEntity.insert(nSize, seriesEntity);
		}

		int iProgress = (((float)i) / ntotalRow) * 100;
		SetProgressValue(iProgress);
	}

	///////////////////////////////////////////////////////////////////////////

	m_bLoadImage = true;
	if (m_mapSeriesID.count() > 1)
	{
		if (m_pSeriesModel)
			delete m_pSeriesModel;
		m_pSeriesModel = new QStandardItemModel(m_mapSeriesID.count(), 7, NULL);

		m_pSeriesModel->setHorizontalHeaderItem(0, new QStandardItem(QString("Patient Name")));
		m_pSeriesModel->setHorizontalHeaderItem(1, new QStandardItem(QString("Study Description")));
		m_pSeriesModel->setHorizontalHeaderItem(2, new QStandardItem(QString("Series Description")));
		m_pSeriesModel->setHorizontalHeaderItem(3, new QStandardItem(QString("Study Date")));
		m_pSeriesModel->setHorizontalHeaderItem(4, new QStandardItem(QString("Modality")));
		m_pSeriesModel->setHorizontalHeaderItem(5, new QStandardItem(QString("Files")));
		m_pSeriesModel->setHorizontalHeaderItem(6, new QStandardItem(QString("Selected")));

		for (int row = 0; row < m_mapSeriesID.count(); row++)
		{
			int nRowRefer = mapRowToRefer.value(row);

			QStandardItem* rowItem = NULL;
			for (int col = 0; col < 5; col++)
			{
				QString strContent = QString(m_seriesVec[nRowRefer][col + 3]);
				rowItem = new QStandardItem(strContent);
				if (strContent.length() > 15)
				{					
					rowItem->setData(strContent, Qt::ToolTipRole);
				}				
				m_pSeriesModel->setItem(row, col, rowItem);
			}
			
			rowItem = new QStandardItem(QString::number(mapFileCount.value(row)));
			m_pSeriesModel->setItem(row, 5, rowItem);

			// add check box for MRI series
			QStandardItem* checkBoxItem = new QStandardItem();
			checkBoxItem->setCheckable(true);  			
			checkBoxItem->setCheckState(Qt::Unchecked);			
			m_pSeriesModel->setItem(row, 6, checkBoxItem);
		}

		m_bMoreThanOneSeries = true;
	}
	else
	{
		SetDICOMImage_LevelWindow(m_seriesVec,0,m_seriesVec.count());
		if (!SetDICOMImage_Orientation(m_seriesVec, 0))
		{
			m_bLoadImage = false;
		}
	}

	//m_seriesVec = vec;

	SetProgressValue(100);
	//emit progressChanged(100);

	//return bMoreThanOneSeries;
}

bool FusionMainWindow::AnalyzeImageOrientation()
{
	double max1, max2;
	int maxIndex1, maxIndex2;

	max1 = 0;
	max2 = 0;
	maxIndex1 = 0;
	maxIndex2 = 0;
	for (int i = 0; i < 3; i++)
	{
		if (abs(m_fDirCosines[i]) > abs(max1))
		{
			max1 = m_fDirCosines[i];
			maxIndex1 = i;
		}

		if (abs(m_fDirCosines[i + 3]) > abs(max2))
		{
			max2 = m_fDirCosines[i + 3];
			maxIndex2 = i;
		}
	}

	if (maxIndex1 != 0 || maxIndex2 != 1) // includes other types of rotation, eg Sagittal images
	{
		m_iDicomFlip = FLIP_Y | FLIP_Z; // flip from patient to fusion orientation
		return false; // do not load image
	}
	else if (max1 > 0.0 && max2 > 0.0) // identity, 
		m_iDicomFlip = FLIP_Y | FLIP_Z; // flip from patient to fusion orientation
	else if (max1 < 0.0 && max2 < 0.0) // flip X-Y + flip Y-Z -> flip X-Z
		m_iDicomFlip = FLIP_X | FLIP_Z;
	else if (max1 < 0.0) // flip X-Z + flip Y-Z -> flip X-Y
		m_iDicomFlip = FLIP_X | FLIP_Y;
	else // flip Y-Z + flip Y-Z -> no flip
		m_iDicomFlip = FLIP_NONE;

	return true;

}

void FusionMainWindow::SetDICOMImage_LevelWindow(QVector <QVector< QString >> vec,int index, int count)
{
	int minPixel, maxPixel;
	int totalWidth, totalLevel;

	minPixel = qPow(2, 16) - 1;
	maxPixel = 0;
	totalWidth = 0;
	totalLevel = 0;
	int winCount = 0;

	// find average
	for (int i = 0; i < count; i++)
	
	{
		double windowCenter, windowWidth;
		QString strCenter = vec[index+i][8];
		QString	strWidth = vec[index + i][9];
		int nSmallestPixel = vec[index + i][11].toInt();
		int nLargestPixel = vec[index + i][12].toInt();


		if (strCenter.contains("\\"))
		{
			QStringList strListCenter = strCenter.split("\\");
			windowCenter = strListCenter.at(0).toDouble();
		}
		else
			windowCenter = vec[index + i][8].toDouble();

		if (strWidth.contains("\\"))
		{
			QStringList strListWidth = strWidth.split("\\");
			windowWidth = strListWidth.at(0).toDouble();
		}
		else
			windowWidth = vec[index + i][9].toDouble();

		if (minPixel > nSmallestPixel)
			minPixel = nSmallestPixel;
		if (maxPixel < nLargestPixel)
			maxPixel = nLargestPixel;

		if (abs(windowWidth) > 0.00001) // if there is no windowWidth info
		{
			totalWidth += windowWidth;
			totalLevel += windowCenter;
			winCount++;
		}
	}

	if (winCount > 0)
	{
		m_iDicomWindowWidth = totalWidth / winCount;
		m_iDicomWindowCenter = totalLevel / winCount;
	}
	else
	{
		m_iDicomWindowCenter = 0;
		m_iDicomWindowWidth = 0;
	}

	if (maxPixel == 0)
		maxPixel = m_iDicomWindowCenter * 2;
}

bool FusionMainWindow::SetDICOMImage_Orientation(QVector <QVector< QString >> vec, int nIndex)
{
	m_iDicomFlip = 0;
	bool toContinue = false;
	if (vec.count() > nIndex)
	{
		// need to do more checking here
		QString strCenter = vec[nIndex][16];
		QStringList strListCenter = strCenter.split("\\");
		if (strListCenter.count() == 6)
		{
			for (int i = 0; i < 6; i++)
			{
				m_fDirCosines[i] = strListCenter.at(i).toDouble();
			}
			toContinue = true;
		}
	}

	if (toContinue)
		return AnalyzeImageOrientation();
	else // if image orientation does not exist, assume it is identity. Just flip to fusion orientation. 
		m_iDicomFlip = FLIP_Y | FLIP_Z;

	return true;
}

QStringList FusionMainWindow::GetSeriesFiles(int nIndex)
{
	QString studyID = m_mapStudyID.value(nIndex);
	QString seriesID = m_mapSeriesID.value(nIndex);


	m_bLoadImage = true;
	m_bIsMultiFrame = false;

	// If series Contain MultiFarme: AJ
	bool multiFrame = m_multiFrameMap.value(nIndex);
	if (multiFrame)
		m_bIsMultiFrame = true;

	QStringList filelist;
	QMap<int, int> selectedSeriesIndex;

	selectedSeriesIndex.clear();
	for (int i = 0; i < m_seriesVec.count(); i++)
	{
		if (m_seriesVec[i][1] == studyID && m_seriesVec[i][2] == seriesID)
		{
			filelist.append(m_seriesVec[i][0]);
			selectedSeriesIndex.insert(selectedSeriesIndex.count(), i);
			m_strPatientName = m_seriesVec[i][3];
			m_strPatientID = m_seriesVec[i][17];
			m_strPatientBirthDate = m_seriesVec[i][18];
		}
	}
	//SetDICOMImage_LevelWindow(SelectedSeriesIndex.value(SelectedSeriesIndex.count()/2));
	if (!SetDICOMImage_Orientation(m_seriesVec, selectedSeriesIndex.value(0)))
		m_bLoadImage = false;
	else
		SetDICOMImage_LevelWindow(m_seriesVec, selectedSeriesIndex.value(0), selectedSeriesIndex.count());

	return filelist;
}

QVector<QStringList> FusionMainWindow::GetSeriesFiles(QVector<int> vecIndex)
{
	QVector<QStringList> vecFiles;
	for each (int idx in vecIndex)
	{
		QStringList files = this->GetSeriesFiles(idx);
		vecFiles.append(files);
	}
	return vecFiles;
}

int FusionMainWindow::LoadSeries()
{
	OpenDICOMSeriesDialog dialog(m_pSeriesModel, this);
	dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	dialog.setFixedSize(715, 440);

	if (dialog.exec() == QDialog::Accepted)
	{
		m_selectedSeriesIndx.clear();
		m_selectedSeriesFiles.clear();
		m_selectedSeriesIndx = dialog.GetSelectedSeriesIndexList();
		m_selectedSeriesFiles = GetSeriesFiles(m_selectedSeriesIndx);
		int nIndex = dialog.GetSelectedSeriesIndex();
		m_selectedFiles = GetSeriesFiles(nIndex);
	}

	int ret = 0;
	for each (QStringList selecedFiles in m_selectedSeriesFiles)
	{
		int count = selecedFiles.count();
		if (count > 100)
		{
			ret = -1;
			break;
		}
		else if (count == 0)
		{
			ret = -2;
			break;
		}
	}
	return ret;
}

int FusionMainWindow::Load(QStringList inputFiles)
{
	m_bIsDicomDir = false;
	m_iDicomWindowCenter = 0;
	m_iDicomWindowWidth = 0;
	for (int i=0;i<6;i++)
		m_fDirCosines[i] = 0.0;

	m_originalFiles = inputFiles;
	m_pSeriesModel = NULL;
	m_strPatientName = "";

	m_loadedFiles.clear();
	m_selectedFiles.clear();

	QString strDirDicomFile = GetFirstDicomDir(m_originalFiles);

	if (strDirDicomFile != "")//if Dicom DIR is present
	{
		if (!m_pDicomDirImporter->DicomDirParser(strDirDicomFile))
			return -1; //Invalid Dicom Dir File

		int nSeriesCount = m_pDicomDirImporter->MapDicomDirInfo.count();
		if (nSeriesCount == 0)
			return -2; //No files loaded

		for (int i = 0; i < nSeriesCount; i++)
		{
			int nFileCount = m_pDicomDirImporter->MapDicomDirInfo[i].filespath.count();
			for (int j = 0; j < nFileCount; j++)
			{
				m_loadedFiles.append(m_pDicomDirImporter->MapDicomDirInfo[i].filespath[j]);
			}
		}
	}
	else
	{
		m_loadedFiles = m_originalFiles;
	}

	AnalyzeStudySeries(m_loadedFiles);

	if (m_bMoreThanOneSeries)
	{
		//SetProgressValue(0);
		int ret = LoadSeries();
		if (ret == -1)
			return -3;
		else if (ret == -2)
			return -4;

	}
	else
		m_selectedFiles = m_loadedFiles;

	return 0; //no issue
}

void FusionMainWindow::ResetDicomDirImporter()
{
	if (m_pDicomDirImporter != NULL)
	{
		delete m_pDicomDirImporter;
		m_pDicomDirImporter = NULL;
	}
	m_pDicomDirImporter = new DicomDirImporter();
	m_selectedFiles.clear();
}

void FusionMainWindow::UploadImportedDicom()
{
	ui->rightFrame->setEnabled(false);
	QApplication::processEvents();


	// if images are multiframe
	bool isValid = true;
	if (m_bIsMultiFrame)
	{
		ShowMessageBox(MSGBOX_MULTIFRAME_NOT_SUPPORTED);
		isValid = false;
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_IMPORT_IMAGE, BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST);

	}

	// if dicom images are non transversal, continue to load
	else if (!m_bLoadImage)
	{
		ShowMessageBox(MSGBOX_DICOM_IMAGE_NON_TRANSVERSAL);
		isValid = false;
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_IMPORT_IMAGE, BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST);

	}

	// load files
	else if (!m_pSurgeryController->ImportDICOMImages(m_selectedFiles,m_iDicomFlip,m_iDicomWindowCenter,m_iDicomWindowWidth, m_sDecryptDicomPassword))
	{
		ShowMessageBox(MSGBOX_INVALID_FILES);
		isValid = false; // error message
		m_pSurgeryController->DeleteImageStack();
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_IMPORT_IMAGE, BaseSurgeryController::STATE_IMPORT_IMAGE_FIRST);

	}

	ResetWindowLevel();

	if (isValid) // load successfully //Remove checking to organize deletion of Image Stack
	{
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_IMPORT_IMAGE, BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED);

		// copy tags.xml.enc to case folder if it exists
		QString sTagsFileSource = m_sSelectDir + "\\tags.xml.enc";
		QString sTagsFileDest = m_pSurgeryController->GetCasePath() + "\\tags.xml.enc";
		QFile::remove(sTagsFileDest); //remove previous one first if exist. 
		QFile::copy(sTagsFileSource, sTagsFileDest);

		// decrypt with old password, then encrypt with new password
#ifdef PDP_DATA
		int len = sTagsFileDest.length();
		QString sTagsFileDecrypted = sTagsFileDest;
		sTagsFileDecrypted.truncate(len - 4);
		bool bEncrypted;
		if (m_sDecryptDicomPassword != "")
		{
			bEncrypted = PdpDecrypt2(sTagsFileDecrypted, m_sDecryptDicomPassword);
			if (bEncrypted)
			{
				// encrypt with new password
				PdpEncrypt2(sTagsFileDecrypted, m_pSurgeryController->GetSurgery()->GetEncryptCasePassword());
			}
		}
#endif

		// log
		QString sLog = "Dicom files in '" + m_sSelectDir + "' imported";
		if (m_UserAccount.GetLoginUsername() != "")
			sLog += " by " + m_UserAccount.GetLoginUsername();

		Log(sLog, UtlLogger::SECURITY_INFO);

		m_imageFiles = m_selectedFiles;

		GetVisualEngine()->ResetAllViews();
	}

	// enable or disable the Select Series button //Move here as it no need to check/set this if dicom is invalid, DENVER
	if (!m_bMoreThanOneSeries && !m_bIsDicomDir)
		EnableNormalButton(ui->imageImportSelectSeries, false);
	else
		EnableNormalButton(ui->imageImportSelectSeries);
	//Remove else condition to organize deletion of Image Stack

	SetProgressValue(0);

	ui->rightFrame->setEnabled(true);
}

QStringList FusionMainWindow::SelectFiles(QString &password)
{
	QString filters = tr("DICOM files") + " (*.dcm)";
#ifdef PDP_DATA
	filters += ";;" + tr("Encrypted files (*.enc)");
#endif

	filters += ";;" + tr("All files (*)");

	QString selectedFilter;
	QStringList files = GetFileDialogSelection(this, tr("Import DICOM images"), "C:/", QFileDialog::ExistingFiles, QFileDialog::AcceptOpen, filters, selectedFilter);

	password = "";
	if (selectedFilter == tr("Encrypted files (*.enc)"))
	{
		DecryptPasswordDialog decryptDlg(this);
		decryptDlg.setWindowFlags(decryptDlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
		decryptDlg.adjustSize();
		decryptDlg.setFixedSize(decryptDlg.frameSize());
		if (decryptDlg.exec() == QDialog::Rejected) // cancel
		{
		}//do nothing

		password = decryptDlg.GetDecryptPassword();
		//}
	}

	return files;
}


void FusionMainWindow::on_imageImportSelectSeries_clicked()
{
	Log("on_imageImportSelectSeries_clicked");

	// clear message label
	ClearWarningMessageLabel();

	int ret = LoadSeries();
	if (ret == -1)
	{
		ShowMessageBox(MSGBOX_TOO_MANY_IMAGES);
		SetProgressValue(0);
		return;
	}
	else if (ret == -2)
	{
		//no file is loaded
		SetProgressValue(0);
		return;
	}
	// load series
	DisableMeasurementTool();
	LoadDicomSequecne();
}

QString FindKeyWord(const QString& labeltext)
{
	QString labeltextUpper = labeltext.toUpper();

	if (labeltextUpper.contains("T2")) {
		return "T2";
	}
	else if (labeltextUpper.contains("ADC")) {
		return "ADC";
	}
	else if (labeltextUpper.contains("DWI")) {
		return "DWI";
	}
	else {
		return labeltext.left(10);
	}
}

// should be merged with AxialMousInteractor::AdjustDisplayDirection()
void FusionMainWindow::AdjustDisplayDirection(vtkSmartPointer<vtkImageData> imageData, double angleDegrees, vtkSmartPointer<vtkImageData>& outputImage, ViewType orientation)
{
	vtkSmartPointer<vtkMatrix4x4> rotationMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
	rotationMatrix->Identity();
	double angleRadians = vtkMath::RadiansFromDegrees(angleDegrees);
	if (orientation == ViewType::Coronal) {
		// Coronal rotated y axis
		rotationMatrix->SetElement(0, 0, cos(angleRadians));
		rotationMatrix->SetElement(0, 1, 0); 
		rotationMatrix->SetElement(0, 2, sin(angleRadians)); 
		rotationMatrix->SetElement(1, 0, 0); 
		rotationMatrix->SetElement(1, 1, 1); 
		rotationMatrix->SetElement(1, 2, 0); 
		rotationMatrix->SetElement(2, 0, -sin(angleRadians)); 
		rotationMatrix->SetElement(2, 1, 0);  
		rotationMatrix->SetElement(2, 2, cos(angleRadians));

		if (m_iDicomFlip & ImageEntity::FLIP::FLIP_X)
		{
			rotationMatrix->SetElement(0, 0, -rotationMatrix->GetElement(0, 0)); // 反向翻转
			rotationMatrix->SetElement(1, 0, -rotationMatrix->GetElement(1, 0)); // 反向翻转
			rotationMatrix->SetElement(2, 0, -rotationMatrix->GetElement(2, 0)); // 反向翻转
		}
	}
	else if (orientation == ViewType::Sagittal) {
		// Sagittal rotated x axis
		rotationMatrix->SetElement(0, 0, 1);  
		rotationMatrix->SetElement(0, 1, 0);  
		rotationMatrix->SetElement(0, 2, 0);  
		rotationMatrix->SetElement(1, 0, 0);  
		rotationMatrix->SetElement(1, 1, cos(angleRadians));  
		rotationMatrix->SetElement(1, 2, -sin(angleRadians)); 
		rotationMatrix->SetElement(2, 0, 0);  
		rotationMatrix->SetElement(2, 1, sin(angleRadians));  
		rotationMatrix->SetElement(2, 2, cos(angleRadians)); 

		if (m_iDicomFlip & ImageEntity::FLIP::FLIP_Y)
		{
			rotationMatrix->SetElement(0, 1, -rotationMatrix->GetElement(0, 1)); // 反向翻转
			rotationMatrix->SetElement(1, 1, -rotationMatrix->GetElement(1, 1)); // 反向翻转
			rotationMatrix->SetElement(2, 1, -rotationMatrix->GetElement(2, 1)); // 反向翻转
		}
	}
	vtkSmartPointer<vtkImageReslice> reslice = vtkSmartPointer<vtkImageReslice>::New();
	reslice->SetInputData(imageData);
	reslice->SetResliceAxes(rotationMatrix);
	reslice->SetInterpolationModeToLinear();
	reslice->Update();
	outputImage = reslice->GetOutput();
}

/**
 * @brief Creates a VTK viewer and embeds it into the specified parent widget.
 *
 * @param parent The parent widget in which the VTK viewer will be embedded.
 * @param seriesIdx The row index of the series to be displayed in the VTK viewer. 
 * @param orientation The type of view (e.g., axial, sagittal, or coronal) to be displayed in the VTK viewer. 
 */
void FusionMainWindow::CreateVtkViewer(QWidget* parentWidget, int seriesIdx, ViewType orientation)
{
	QFrame* frame = new QFrame(parentWidget);
	frame->setObjectName(QString("AxialFrame_%1").arg(seriesIdx));
	frame->setContentsMargins(0, 0, 0, 0);
	QVTKOpenGLWidget* vtkWidget = new QVTKOpenGLWidget(frame);
	vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	vtkWidget->GetRenderWindow()->AddRenderer(renderer);
	
	QVBoxLayout* frameLayout = new QVBoxLayout(frame);
	frameLayout->setContentsMargins(0, 0, 0, 0);
	frameLayout->addWidget(vtkWidget);
	
	// 1 Load series image
	QString tempDir = m_pSurgeryController->GetCasePath();
	QString seriesID = m_mapSeriesImageEntity[seriesIdx].m_metaData.m_seriesUid;
	QString strDest = tempDir + "/" + seriesID + "/";
	QDir dir(strDest);
	if (!dir.exists())
	{
		QDir().mkdir(strDest);
	}
	QStringList currentFiles = GetSeriesFiles(seriesIdx);
	m_mapSeriesImageEntity[seriesIdx].LoadData(strDest, currentFiles);

	// 2. create vtk viewer
	vtkSmartPointer<vtkImageViewer2> viewer = vtkSmartPointer<vtkImageViewer2>::New();
	viewer->SetRenderWindow(vtkWidget->GetRenderWindow());
	viewer->SetRenderer(renderer);
	viewer->SetSliceOrientation((int)orientation);
	vtkSmartPointer<vtkImageData> inputImageData = vtkSmartPointer<vtkImageData>::New();
	AdjustDisplayDirection(m_mapSeriesImageEntity[seriesIdx].m_vtkReader->GetOutput(), -90, inputImageData, orientation);
	viewer->SetInputData(inputImageData);
	viewer->SetColorWindow(m_mapSeriesImageEntity[seriesIdx].m_metaData.m_windowWidth);
	viewer->SetColorLevel(m_mapSeriesImageEntity[seriesIdx].m_metaData.m_windowCenter);

	// 3. set viewer slice
	int maxSlice = viewer->GetSliceMax();
	int currentSlice = maxSlice / 2;
	viewer->SetSlice(currentSlice);

	//4. set mouse interactor
	viewer->SetupInteractor(vtkWidget->GetInteractor());
	if (orientation == ViewType::Axial)
	{
		vtkSmartPointer<AxialMousInteractor> mouseInteractor = vtkSmartPointer<AxialMousInteractor>::New();
		vtkWidget->GetInteractor()->SetInteractorStyle(mouseInteractor);
		mouseInteractor->parentFrame = frame;
		mouseInteractor->multiSequenceTransversalWidget = m_ptrtransversalWidget;
		mouseInteractor->m_iFilp = m_iDicomFlip;
		mouseInteractor->m_iSeriesIdx = seriesIdx;
	}
	else {
		vtkSmartPointer<MouseInteractor> mouseInteractor = vtkSmartPointer<MouseInteractor>::New();
		vtkWidget->GetInteractor()->SetInteractorStyle(mouseInteractor);
	}

	//5. render
	viewer->SetRenderer(renderer);
	viewer->Render();	

	// add QLabel and QVTKOpenGLWidget to QVBoxLayout 
	QLabel* label = new QLabel(parentWidget);
	QString seriesDesc = m_mapSeriesImageEntity[seriesIdx].m_metaData.m_seriesDesc;
	QString labelText = QString("  %1/%2").arg(currentSlice).arg(maxSlice);
	switch (orientation)
	{
	case Sagittal:
		labelText = "Sagittal " + labelText;	
		m_mapSeriesImageEntity[seriesIdx].m_sagittalViewer = viewer;
		break;
	case Coronal:
		labelText = "Coronal " + labelText;
		m_mapSeriesImageEntity[seriesIdx].m_coronalViewer = viewer;
		break;
	case Axial:
		labelText = FindKeyWord(seriesDesc) + labelText;
		m_mapSeriesImageEntity[seriesIdx].m_axialViewer = viewer;
		break;
	default:
		break;
	}
	label->setText(labelText);
	label->setStyleSheet("color:rgb(255, 239, 157); padding: 2px;");
	label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	QVBoxLayout* containerLayout = new QVBoxLayout(parentWidget);
	containerLayout->setContentsMargins(0, 0, 0, 0);
	containerLayout->addWidget(label);
	containerLayout->addWidget(frame);
	containerLayout->setStretch(0, 0);
	containerLayout->setStretch(1, 1);
	parentWidget->setLayout(containerLayout);
}

// init multi-sequence widget handler
void FusionMainWindow::MultiSeqWidgetInit()
{
	if (m_pStackedWidget == nullptr)
		m_pStackedWidget = ui->middleFrame->findChild<QStackedWidget*>("graphicPanelStackedWidget");

	if (m_pLayout2DView == nullptr)
		m_pLayout2DView = m_pStackedWidget->findChild<QWidget*>("layout2DView");

	if (m_pTransversal == nullptr)
		m_pTransversal = m_pLayout2DView->findChild<QStackedWidget*>("transversalImageContextStackedWidget");

	if (m_pSagCoronalWidget == nullptr)
	{
		m_pSagCoronalWidget = m_pLayout2DView->findChild<QWidget*>("layoutSagittalCoronal");
		m_pSagitalWidget = m_pSagCoronalWidget->findChild<QWidget*>("sagittalImageContextWidget");
		m_pCoronalWidget = m_pSagCoronalWidget->findChild<QWidget*>("coronalImageContextWidget");
		
	}
}

void FusionMainWindow::CreateSagCorViewer(int seriesIdx)
{
	m_pCoronalWidget->setVisible(false);
	m_pSagitalWidget->setVisible(false);

	QVBoxLayout* sagcorVlayout = new QVBoxLayout(m_pSagCoronalWidget);

	m_pCurCoronalWidget = new QWidget(m_pSagCoronalWidget);
	CreateVtkViewer(m_pCurCoronalWidget, seriesIdx, ViewType::Coronal);
	m_pCurCoronalWidget->setStyleSheet("background-color: #2b2b2b;");	

	m_pCurSagitalWidget = new QWidget(m_pSagCoronalWidget);
	CreateVtkViewer(m_pCurSagitalWidget, seriesIdx, ViewType::Sagittal);
	m_pCurSagitalWidget->setStyleSheet("background-color: #2b2b2b;");

	sagcorVlayout->addWidget(m_pCurSagitalWidget);
	sagcorVlayout->addWidget(m_pCurCoronalWidget);

	if (m_pSagCoronalWidget->layout()) {
		delete m_pSagCoronalWidget->layout();
	}
	m_pSagCoronalWidget->setLayout(sagcorVlayout);
	m_currentSagittalViewer = m_mapSeriesImageEntity[seriesIdx].m_sagittalViewer;
	m_currentCoronalViewer = m_mapSeriesImageEntity[seriesIdx].m_coronalViewer;
}

void FusionMainWindow::UploadMultiSequenceDicom()
{
	ui->rightFrame->setEnabled(false);
	QApplication::processEvents();
	m_pSurgeryController->SetState(BaseSurgeryController::STATE_IMPORT_IMAGE, BaseSurgeryController::STATE_IMPORT_IMAGE_LOADED);
	MultiSeqWidgetInit();
	// add new widget for display multi-sequence's transversal view 
	m_ptrtransversalWidget = new TransversalWidget(m_pLayout2DView);
	m_pTransversal->addWidget(m_ptrtransversalWidget);
	m_pTransversal->setCurrentWidget(m_ptrtransversalWidget);

	QString tempDir = m_pSurgeryController->GetCasePath();
	int seriesCount = m_selectedSeriesFiles.count();
	QGridLayout* gridLayout = new QGridLayout(m_ptrtransversalWidget);
	int* viewerPos = new int[seriesCount];
	for (int i = 0; i < seriesCount; ++i)
	{
		viewerPos[i] = i;		
	}

	for (int i = 0; i < seriesCount; ++i)
	{
		int seriesIdx = m_selectedSeriesIndx[i];
		QString seriesDesc = m_mapSeriesImageEntity[seriesIdx].m_metaData.m_seriesDesc;
		if (QString::compare(FindKeyWord(seriesDesc), "T2", Qt::CaseInsensitive) == 0 && i != 1)
		{
			// adjust the position of T2 sequence  to the index 1			
			int temp = viewerPos[i];
			viewerPos[i] = viewerPos[1];
			viewerPos[1] = temp;
			break;
		}
	}
	
	for (int i = 0; i < seriesCount; ++i) {
		int seriesIdx = m_selectedSeriesIndx[i];
		QWidget* containerWidget = new QWidget(m_ptrtransversalWidget);
		CreateVtkViewer(containerWidget, seriesIdx, ViewType::Axial);
		int row = viewerPos[i] / 2;
		int col = viewerPos[i] % 2;
		gridLayout->addWidget(containerWidget, row, col);
		if (i == 0)
		{
			CreateSagCorViewer(seriesIdx);			
		}
		// set mouse interactor in every axial viewer
		AxialMousInteractor* axisInteractor = AxialMousInteractor::SafeDownCast(m_mapSeriesImageEntity[seriesIdx].m_axialViewer->GetRenderWindow()->GetInteractor()->GetInteractorStyle());
		axisInteractor->imageEntity = m_mapSeriesImageEntity[seriesIdx];
		axisInteractor->m_coronalViewer = m_currentCoronalViewer;
		axisInteractor->m_sagittalViewer = m_currentSagittalViewer;
		if (i == 0)
		{
			m_currentSelectedSeriesId = m_selectedSeriesIndx[i];
			axisInteractor->parentFrame->setStyleSheet("background-color: lightgray; border: 2px solid green;");
		}
	}

	// Fill remaining cells with empty widgets if seriesCount < 4
	for (int i = seriesCount; i < 4; ++i) {
		QWidget* emptyWidget = new QWidget(m_ptrtransversalWidget);
		QVTKOpenGLWidget* vtkWidget = new QVTKOpenGLWidget(emptyWidget);		
		vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);		
		QVBoxLayout* containerLayout = new QVBoxLayout(emptyWidget);
		containerLayout->addWidget(vtkWidget);
		containerLayout->setStretch(0, 0);
		emptyWidget->setLayout(containerLayout);

		int row = i / 2;
		int col = i % 2;
		gridLayout->addWidget(emptyWidget, row, col);
	}
	m_ptrtransversalWidget->setLayout(gridLayout);
	m_ptrtransversalWidget->selectedSeriesID = &m_currentSelectedSeriesId;
	m_ptrtransversalWidget->m_selectedSeriesIndx = m_selectedSeriesIndx;

	if (!m_bMoreThanOneSeries && !m_bIsDicomDir)
		EnableNormalButton(ui->imageImportSelectSeries, false);
	else
		EnableNormalButton(ui->imageImportSelectSeries);
	SetProgressValue(0);
	ui->rightFrame->setEnabled(true);
}

void FusionMainWindow::on_imageImportMRIImages_clicked()
{
	Log("on_imageImportMRIImages_clicked");
	m_sDecryptDicomPassword = "";
	QStringList files = SelectFiles(m_sDecryptDicomPassword);

	if (files.count() == 0)
		return;

	m_sSelectDir = QFileInfo(files.at(0)).absoluteDir().absolutePath();

	DisableMeasurementTool();

	SetProgressValue(0);
	ResetDicomDirImporter();
	m_pSurgeryController->DeleteImageStack();
	int ret = Load(files);
	switch (ret)
	{
	case -1:
		SetProgressValue(0);
		ShowMessageBox(MSGBOX_INVALID_DICOM_FILE);
		return;
	case -2:
	case -4:
		SetProgressValue(0);
		return;
	case -3:
		ShowMessageBox(MSGBOX_TOO_MANY_IMAGES);
		SetProgressValue(0);
		return;

	default: break;
	}

	LoadDicomSequecne();
}

void FusionMainWindow::on_imageImportFlipAntPos_clicked()
{
	Log("on_imageImportFlipAntPos_clicked");
	m_pSurgeryController->FlipImage_AntPos();
}

void FusionMainWindow::on_imageImportFlipApexBase_clicked()
{
	Log("on_imageImportFlipApexBase_clicked");
	m_pSurgeryController->FlipImage_ApexBase();
}

void FusionMainWindow::on_imageImportResetImage_clicked()
{
	Log("on_imageImportResetImage_clicked");
	m_pSurgeryController->ResetToDicomFlip();
}

void FusionMainWindow::on_imageImportApproveImage_clicked()
{
	Log("on_imageImportApproveImage_clicked");
	// if multi-seq MRI, first loaded selected sequence image
	if (m_selectedSeriesIndx.count() > 1)
	{			
		if (m_ptrtransversalWidget != nullptr)
		{
			MultiSeqWidgetInit();
			//del transversalWidget from QStackedWidgetdel
			m_pTransversal->removeWidget(m_ptrtransversalWidget);
			delete m_ptrtransversalWidget;
			m_ptrtransversalWidget = nullptr;

			QWidget* modellingImageContextWidget = m_pTransversal->findChild<QWidget*>("modellingImageContextWidget");
			m_pTransversal->setCurrentWidget(modellingImageContextWidget);

			m_currentCoronalViewer->Delete();
			m_currentCoronalViewer = nullptr;
			delete m_pCurCoronalWidget;
			m_pCurCoronalWidget = nullptr;

			m_currentSagittalViewer->Delete();
			m_currentSagittalViewer = nullptr;
			delete m_pCurSagitalWidget;
			m_pCurSagitalWidget = nullptr;

			m_pCoronalWidget->setVisible(true);
			m_pSagitalWidget->setVisible(true);
		}
		m_selectedFiles = GetSeriesFiles(m_currentSelectedSeriesId);
		UploadImportedDicom();		
	}

	if (m_pSurgeryController->GetNumSlices() < 10)
	{
		ShowMessageBox(MSGBOX_NOT_ENOUGH_IMAGES);
		return;
	}

	m_pSurgeryController->ApproveImage();

	// create new model
	m_pSurgeryController->NewUrologyModel(MODEL_MODE_SEMIAUTO);
	m_pSurgeryController->CreateSubModel(MODEL_PROSTATE,SURFACE_CLOSED);


	m_pSurgeryController->SetState(BaseSurgeryController::STATE_MODEL, BaseSurgeryController::STATE_MODEL_FIRST);
	connect(m_pSurgeryController->GetAutoModel(), SIGNAL(progressChanged(int)), this, SLOT(SetProgressValue(int)));


	DisableMeasurementTool();
}

/******************************************************************************/
/* prostate model functions
/******************************************************************************/

void FusionMainWindow::on_modelLoadRTStruct_clicked()
{
	Log("on_modelLoadRTStruct_clicked");

	// set filters
	QString filters = tr("DICOM files") + " (*.dcm)";
#ifdef PDP_DATA
	filters += ";;" + tr("Encrypted files (*.enc)");
#endif
	filters += ";;" + tr("All files (*)");

	QString selectedFilter;
	QStringList files = GetFileDialogSelection(this, tr("Load RT Struct File"), "C:/", QFileDialog::ExistingFile, QFileDialog::AcceptOpen, filters, selectedFilter);

	if (files.count() == 0)
		return;

	if (m_pSurgeryController->LoadRTStructModel(m_imageFiles,files.at(0), m_sDecryptDicomPassword))
	{
		// set new state
		m_pSurgeryController->UpdateLesionInfo();
		m_pSurgeryController->SetState(BaseSurgeryController::STATE_MODEL, BaseSurgeryController::STATE_MODEL_AUTOMODEL_DONE);
	}
	else
	{
		ShowMessageBox(MSGBOX_INVALID_RTFILE);
	}
	
}

void FusionMainWindow::on_modelDeleteCurve_clicked()
{
	BaseMainWindow::on_modelDeleteCurve_clicked();
	if (m_pSurgeryController->GetSubState() == BaseSurgeryController::STATE_MODEL_FIRST) // reset done successfully
		m_pSurgeryController->CleanLesion();
}

void FusionMainWindow::on_modelResetModel_clicked()
{
	BaseMainWindow::on_modelResetModel_clicked();

	if (m_pSurgeryController->GetSubState() == BaseSurgeryController::STATE_MODEL_FIRST) // reset done successfully
		m_pSurgeryController->CleanLesion();
}


/******************************************************************************/
/* lesion models functions                                                                         
/******************************************************************************/

void FusionMainWindow::SetLesionNavigationButtons()
{
	int iNumValidLesions = m_pSurgeryController->GetNumValidLesionSubModels();
	if (iNumValidLesions >= 2)
	{
		EnableNormalButton(ui->lesionNextLesion);
		EnableNormalButton(ui->lesionPreviousLesion);
	}
	else
	{
		EnableNormalButton(ui->lesionNextLesion, false);
		EnableNormalButton(ui->lesionPreviousLesion, false);
	}
}

void FusionMainWindow::UpdatePiradsControl()
{
	// get current lesion
	inurbsSubModel* pActiveLesion = m_pVisualEngine->GetActiveLesion();
	if (pActiveLesion == NULL)
		ui->lesionPirads->setCurrentIndex(0); // NA
	else
	{
		int piradsScore = pActiveLesion->GetRiskScore();
		if (piradsScore < 0)
			piradsScore = 0;
		else if (piradsScore > 5)
			piradsScore = 5;
		ui->lesionPirads->setCurrentIndex(piradsScore);
	}
}

void FusionMainWindow::UpdateRemarksControl()
{
	// get urology model
	FusionSurgery* pSurgery = m_pSurgeryController->GetSurgery();
	if (!pSurgery)
		return;

	UrologyModel* pUrologyModel = pSurgery->GetUrologyModel();
	if (!pUrologyModel)
		return;

	ui->lesionRemarks->setPlainText(pUrologyModel->GetRemarks());
}

bool FusionMainWindow::ChangeActiveLesionCheck() // return true to continue
{
	// check if pirads score is set
	inurbsSubModel* pActiveLesion = m_pVisualEngine->GetActiveLesion();
	if (pActiveLesion == NULL)
		return true;

	// check if current lesion is completed. 
	//if (!pActiveLesion->IsSubModelComplete())
	//{
	//	ShowMessageBox(MSGBOX_LESION_INCOMPLETE);
	//	return false;
	//}

	if (pActiveLesion->GetRiskScore() < 0)
	{
		if (ShowMessageBox(MSGBOX_LESION_PIRADS_NOT_SET) == QMessageBox::No)
		{
			return false;
		}
		pActiveLesion->SetRiskScore(0);
	}

	return true;
}

void FusionMainWindow::on_lesionCreateLesion_clicked()
{
	Log("on_lesionCreateLesion_clicked");

	// before change, check first
	if (!ChangeActiveLesionCheck())
		return;

	m_pSurgeryController->CreateLeisionSubModel();
	UpdatePiradsControl();
	m_pSurgeryController->SetState(BaseSurgeryController::STATE_LESION, BaseSurgeryController::STATE_LESION_CREATED);
}

void FusionMainWindow::on_lesionDeleteLesion_clicked()
{
	Log("on_lesionDeleteLesion_clicked");

	if (ShowMessageBox(MSGBOX_WARNING_RESET_MODEL) == QMessageBox::Yes)
	{
		m_pSurgeryController->DeleteActiveLesionSubModel();
		SetLesionNavigationButtons();
		UpdatePiradsControl();
		m_pSurgeryController->UpdateLesionInfo();
	}
}

void FusionMainWindow::on_lesionPreviousLesion_clicked()
{
	Log("on_lesionPreviousLesion_clicked");

	// before change, check first
	if (!ChangeActiveLesionCheck())
		return;

	m_pVisualEngine->GotoPrevLesion();
	UpdatePiradsControl();
}

void FusionMainWindow::on_lesionNextLesion_clicked()
{
	Log("on_lesionNextLesion_clicked");

	// before change, check first
	if (!ChangeActiveLesionCheck())
		return;

	m_pVisualEngine->GotoNextLesion();
	UpdatePiradsControl();
}

void FusionMainWindow::on_lesionDeleteContour_clicked()
{
	Log("on_lesionDeleteContour_clicked");

	// if the current view slice does not contain any curve, just return
	if (!m_pVisualEngine->IsTransversalDisplayContainLesionCurve())
	{
		ShowMessageBox(MSGBOX_NO_CURVE_TO_DELETE);
		return; 
	}

	if (ShowMessageBox(MSGBOX_WARNING_DELETE_CURVE) == QMessageBox::Yes)
	{
		m_pVisualEngine->DeleteLesionDisplayCurve();
		SetLesionNavigationButtons(); // might delete lesion
		m_pSurgeryController->BuildLesionSurface(m_pVisualEngine->GetActiveLesion());
	}
}

void FusionMainWindow::on_lesionPirads_currentIndexChanged(int index)
{
	Log("on_lesionPirads_currentIndexChanged");
	m_pSurgeryController->SetActiveLesionRiskScore(index); // 1-1 translation of index and risk score
	m_pSurgeryController->UpdateLesionInfo();
}

void FusionMainWindow::on_lesionApproveLesionModels_clicked()
{
	Log("on_lesionApproveLesionModels_clicked");

	// before change, check first
	if (!ChangeActiveLesionCheck())
		return;

	if (m_pSurgeryController->IsContainIncompleteLesions())
	{
		if (ShowMessageBox(MSGBOX_WARNING_CLEAR_INCOMPLETE_LESION) == QMessageBox::No)
			return;

		m_pSurgeryController->CleanIncompleteLesions();
		m_pSurgeryController->UpdateLesionInfo();
		m_pVisualEngine->UpdateWindows();
	}

	if (m_pSurgeryController->ApproveLesionsModel(ui->lesionRemarks->toPlainText())) // success
	{
		if(ui->lesionShowModelSwitch->value() == 0)
			SetSwitchValue(ui->lesionShowModelSwitch, 1);

		m_pSurgeryController->SetState(BaseSurgeryController::STATE_FINISH, BaseSurgeryController::STATE_FINISH_FIRST);
		DisableMeasurementTool();
	}
	else
		ShowMessageBox(MSGBOX_APPROVE_MODEL_ERROR);
}

void FusionMainWindow::on_finishExportCase_clicked()
{
	Log("on_finishExportCase_clicked");

	QString sPath = GetExportPath();
	if(sPath.isNull() || sPath == "") return;

	if(m_pSurgeryController->ExportCase(sPath))
		ShowMessageBox(MSGBOX_FINISH_EXPORT_CASE_DONE);
	else
		ShowMessageBox(MSGBOX_FINISH_EXPORT_CASE_FAILED);
}

void FusionMainWindow::on_finishExportAnonymousCase_clicked()
{
	Log("on_finishExportAnonymousCase_clicked");
	
	QString sPath = GetExportPath();
	if(sPath.isNull() || sPath == "") return;

	if(m_pSurgeryController->ExportAnonymousCase(sPath))
		ShowMessageBox(MSGBOX_FINISH_EXPORT_ANONYMOUS_CASE_DONE);
	else
		ShowMessageBox(MSGBOX_FINISH_EXPORT_ANONYMOUS_CASE_FAILED);

}

void FusionMainWindow::PopupApplicationSetting()
{
	FusionSettingDialog fsd(this);
	fsd.setWindowFlags(fsd.windowFlags() & ~Qt::WindowContextHelpButtonHint);

	QString sPatientDataFolder = m_pSurgeryController->GetPatientDataFolder();
	fsd.SetPatientDataDrive(sPatientDataFolder.left(2));

	int iSubState = m_pSurgeryController->GetSubState();
	if(m_pSurgeryController->GetState() > BaseSurgeryController::STATE_START || m_pSurgeryController->GetSubStateFlag(BaseSurgeryController::STATE_START_CASE_CREATED))
		fsd.SetCaseCreated();

	fsd.Init();
	fsd.adjustSize();
	fsd.setFixedSize(fsd.frameSize());

	if(fsd.exec() == QDialog::Rejected) return;

	Log("application setting apply", UtlLogger::TEXT_INFO);

	if(fsd.IsValueChanged())
	{
		// save changes to config file
		QString sPatientDataFolder = fsd.GetPatientDataDrive() + "/" + SYSTEM_PATIENTDATA_RELATIVE_PATH;
		m_pSurgeryController->SetPatientDataFolder(sPatientDataFolder);
	}
}

QString FusionMainWindow::GetFusionTempDir()
{
	return QDir::tempPath() + "/Biobot/UroFusion";
}

void FusionMainWindow::LesionShowModelValueChanged(int value)
{
	UpdateSwitchStyle(ui->lesionShowModelSwitch);
	m_pVisualEngine->SetProstateModelVisible(ui->lesionShowModelSwitch->value() > 0);
}
