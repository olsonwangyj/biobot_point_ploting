/******************************************************************************
    FusionSurgeryController.cpp

    Date      : 29 Oct 2018
 ******************************************************************************/

#pragma warning(disable:4996)

#include <QTimer>
#include <QApplication>
#include <QDir>

#include "Application.h"
#include "FusionSurgeryController.h"
#include "inurbsSubModel.h"
#include "inurbsPlanarCurveStack.h"
#include "UtlMetaRecord.h"
#include "CommonClasses.h"
#include "Crypto.h"
#include "PDP.h"

FusionSurgeryController* FusionSurgeryController::m_pInstance = 0;

FusionSurgeryController::FusionSurgeryController()
{
	m_iState = -1;
	m_iSubState = -1;
	m_pSurgery = NULL;
	m_sPatientNationality = "";
	m_sPatientDataFolder = "";
	ClearSubStateFlags();

	ReadAppConfig();

}

FusionSurgeryController::~FusionSurgeryController()
{
	if (m_pSurgery)
		delete m_pSurgery;
}

FusionSurgeryController* FusionSurgeryController::GetInstance()
{
	if (m_pInstance == 0)
		m_pInstance = new FusionSurgeryController();

	return m_pInstance;
}

void FusionSurgeryController::DeleteInstance()
{
	if (m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = 0;
	}
}

void FusionSurgeryController::SetPatientDataFolder(QString sFolder)
{
	if(m_sPatientDataFolder != sFolder)
	{
		m_sPatientDataFolder = sFolder;
		WriteAppConfig();
	}
}

void FusionSurgeryController::SetPatientNationality(QString sNationality)
{
	if(m_sPatientNationality != sNationality)
	{
		m_sPatientNationality = sNationality;
		WriteAppConfig();
	}
}

bool FusionSurgeryController::ReadAppConfig()
{
	// default values
	m_sPatientNationality = "";
	m_sPatientDataFolder = QString("C:/") + SYSTEM_PATIENTDATA_RELATIVE_PATH;

	QString sAppConfigFilePath = SYSTEM_CONFIG_FOLDER + QString("%1Config.xml").arg(APPLICATION_NAME);

	UtlMetaRecordReader reader;
	reader.read(sAppConfigFilePath);

	const UtlMetaRecordItem* pItemRoot = reader.getResult();
	if(!pItemRoot) return false;

	int iFileVersion = 0;
	UtlMetaRecordItem* fileVersion = pItemRoot->getChildItem("file-version");
	if(fileVersion) iFileVersion = fileVersion->getValueAsInt();
	if(iFileVersion > XML_FILE_VERSION_CONFIG_FUSION) return false;

	if(iFileVersion == 0)
	{
	}
	else // from 1
	{
		UtlMetaRecordItem* folder = pItemRoot->getChildItem("patient-data-folder");
		if(folder) m_sPatientDataFolder = folder->getValue();

		UtlMetaRecordItem* nationality = pItemRoot->getChildItem("patient-nationality");
		if(nationality) m_sPatientNationality = nationality->getValue();
	}

	return true;
}

bool FusionSurgeryController::WriteAppConfig()
{
	UtlMetaRecord root("application-config");
	root.createChildItem("file-version", XML_FILE_VERSION_CONFIG_FUSION);
	root.createChildItem("patient-nationality", m_sPatientNationality);
	root.createChildItem("patient-data-folder", m_sPatientDataFolder);

	QString sAppConfigFilePath = SYSTEM_CONFIG_FOLDER + QString("%1Config.xml").arg(APPLICATION_NAME);

	UtlMetaRecordWriter writer;
	return writer.write(sAppConfigFilePath, &root);
}

void FusionSurgeryController::UpdateState(int state, int subState)
{
	switch (state)
	{
		case STATE_MODEL:
		{
			switch (subState)
			{
				case STATE_MODEL_FIRST:
					// reset all the flags
					if (m_pSurgery->GetUrologyModel())
						m_pSurgery->GetUrologyModel()->ResetFlags();
					ClearSubStateFlags(state);
				break;

				case STATE_MODEL_FIRSTCURVE_CREATED:
					// set apex and base position
					InitModelLimitPositions();
				break;				

				default:
				break;
			}
		}
		break;

		default:
		break;
	}

	SetSubStateFlag(subState);
}

int FusionSurgeryController::GoBackToState(int iState)
{
	// check current state and input state
	if (iState >= m_iState) // do not do anything if this is current state or later stage
		return GetSubState();

	// clean up later stages	

	if (iState <= STATE_LESION)
	{
		ClearSubStateFlags(STATE_FINISH);
	}

	if (iState <= STATE_MODEL)
	{
		//CleanLesion();
		CleanIncompleteLesionCurves();
		ClearSubStateFlags(STATE_LESION);

		// the lesions are changed if they are incomplete
		if(iState == STATE_MODEL)
			m_pSurgery->SaveModel();


	}

	if (iState <= STATE_IMPORT_IMAGE)
	{
		CleanLesion();
		CleanModel(iState > STATE_START);
		ClearSubStateFlags(STATE_MODEL);
	}

	if (iState <= STATE_START)
	{
		// close case, no need to delete the surgery
//		if (m_pSurgery)
//		{
//			delete m_pSurgery;
//			m_pSurgery = NULL;
//			m_pBaseSurgery = NULL;
//		}

		ClearSubStateFlags(); // clear all sub states
		SetSubStateFlag(STATE_START_FIRST);
	}

	// get last substate
	int subState;
	switch (iState)
	{
		// get last substate
		case STATE_START:
			subState = STATE_START_FIRST;
		break;

		case STATE_IMPORT_IMAGE:
			subState = STATE_IMPORT_IMAGE_LOADED;
		break;

		case STATE_MODEL:
		{
			int currentModel = m_pSurgery->GetCurrentModel();
			if (currentModel == MODEL_PROSTATE)
				subState = STATE_MODEL_AUTOMODEL_DONE; // only one possible case
			else // submodels
				subState = STATE_MODEL_SUBMODEL_MODEL_CREATED;			
		}
		break;

		case STATE_LESION:
		{
			UrologyModel *pUrologyModel = m_pSurgery->GetUrologyModel();
			int iNumLesion = pUrologyModel->GetNumLesionSubModels();
			if(iNumLesion > 0)
			{
				QTimer::singleShot(200, this, SLOT(SelectFirstLesion()));
				subState = STATE_LESION_MODEL_CREATED;
			}
			else
				subState = STATE_LESION_FIRST;
		}
		break;

		default:
		break;
	}

	UpdateLesionInfo();
	SetState(iState, subState);
	GetVisualEngine()->ResetAllViews();

	if (m_iState == STATE_MODEL)
		GetVisualEngine()->UpdateSliceZ();

	return subState;
}

//bool FusionSurgeryController::CreateSurgery(QString sPatientDataFolder, QString sCaseId)
//{
//	if (m_pSurgery) 
//		delete m_pSurgery;
//
//	m_pSurgery = new FusionSurgery;
//	m_pSurgery->SetAppVersion(m_sAppVersion + "." + QString::number(m_iAppSvn));
//
//	return m_pSurgery->Create(sPatientDataFolder, sCaseId);	
//}

// assumption is that the file is not encrypted with new password encryption.
// it is either non encrypted at all or encrypted with the master password using crypto++ algo. 
bool FusionSurgeryController::EncryptCaseFile(QString sFilePath, QString sPassword)
{
	if (!QFile::exists(sFilePath)) 
		return false;

	//bool bEncrypted = PdpDecrypt(sFilePath);
	bool bSuccess = PdpEncrypt2(sFilePath, sPassword); // remove original
	if (!bSuccess)
		return false;
	//PdpRemove(sFilePath); // remove non encrypted file whether 

	return true;
}

// assumption is that the case is not encrypted with new password encryption.
// it is either non encrypted at all or encrypted with the master password using crypto++ algo. 
bool FusionSurgeryController::EncryptCase(QString sCaseDirPath, QString sDecryptPassword, QString sEncryptPassword)
{

	return BaseSurgery::EncryptCase(sCaseDirPath,sDecryptPassword, sEncryptPassword);

	/*// encrypt and decrypt surgery
	QString sFilePathSurgery = sCaseDirPath + "/surgery.xml";
	if (!EncryptCaseFile(sFilePathSurgery,sPassword))
		return false; // false because the folder does not contain valid case file

	QString sFilePathLog = sCaseDirPath + "/log.txt";
	if (!EncryptCaseFile(sFilePathLog,sPassword))
		return false; // false because the folder does not contain valid case file

	// encrypt and decrypt image
	QString sImageFolder = GetFolderPath(sCaseDirPath, "image");
	if(sImageFolder == "") return true;

	QString sFileImageXml = sImageFolder + "/" + XML_FILE_NAME_IMAGE;
	QString sFileImageDat = sImageFolder + "/" + IMAGE_FILE_NAME;
	QString sFileImageDat2 = sImageFolder + "/" + IMAGE_16BIT_FILE_NAME;
	
	if (!EncryptCaseFile(sFileImageXml, sPassword))
		return true;
	if (!EncryptCaseFile(sFileImageDat, sPassword))
		return true;
	if (!EncryptCaseFile(sFileImageDat2, sPassword))
		return true;

	// encrypt and decrypt model files
	QString sFileModelPath = GetFolderPath(sCaseDirPath, "model") + "/" + XML_FILE_NAME_MODEL;
	if (!EncryptCaseFile(sFileModelPath, sPassword))
		return true;

	// encrypt screenshots
	// omit this first as it is making trend micro recognizing it as virus
	//QStringList filter;
	//filter = QStringList() << "*.png" << "*.png.enc";

	//QString sFoldercreenshotPath = sCaseDirPath + "/screenshot";
	//QDir dir(sFoldercreenshotPath);
	//QStringList listFiles = dir.entryList(filter, QDir::Files | QDir::NoSymLinks);
	//for(int k=0; k<listFiles.count(); k++)
	//{
	//	QString fname = listFiles.at(k);
	//	QString sFileScreenShot = sFoldercreenshotPath + "/" + fname;
	//	EncryptCaseFile(sFileScreenShot, sPassword);
	//	int stop=1;
	//}

	return true;*/
}

bool FusionSurgeryController::OpenCase(QString sCaseDirPath, QString sPassword)
{
	// close case first
	if (m_pSurgery)
		delete m_pSurgery;

	m_pSurgery = new FusionSurgery;

	// load surgery.xml
	QString sFilePathSurgery = sCaseDirPath + "/surgery.xml";
	m_pSurgery->SetEncryptCasePassword(sPassword);


//#ifdef UROCRYPTO2
//	bool bEncryptedSurgery = PdpDecrypt2(sFilePathSurgery, sPassword);
//#else
//	bool bEncryptedSurgery = PdpDecrypt(sFilePathSurgery);
//#endif
	bool bEncryptedSurgery = false;
	if (sPassword != "")
	{
		bEncryptedSurgery = PdpDecrypt2(sFilePathSurgery, sPassword);
		if (!bEncryptedSurgery)
			return false;
	}

	if (!m_pSurgery->Read(sFilePathSurgery))
	{
		if(bEncryptedSurgery)
			PdpRemove(sFilePathSurgery);

		return false;
	}

	// save case path here, whatever changes in this opened case will be saved
	// in the same dir
	m_pSurgery->SetCasePath(sCaseDirPath);

	VISUALENGINE *pVisualEngine = GetVisualEngine();

	if (!m_pSurgery->LoadImageStack(sCaseDirPath, sPassword))
	{
		if(bEncryptedSurgery)
			PdpRemove(sFilePathSurgery);

		SetState(STATE_IMPORT_IMAGE, STATE_IMPORT_IMAGE_FIRST);
		return true; // only able to load surgery.xml
	}
	else
	{
		// successful
		pVisualEngine->UpdateImageStack(m_pSurgery->GetImageStack());
		NewUrologyModel(MODEL_MODE_SEMIAUTO);
	}

	// load model
	if (!m_pSurgery->LoadModel(sCaseDirPath, sPassword))
	{
		// failed to load model, need to create the prostate submodel
		NewUrologyModel(MODEL_MODE_SEMIAUTO);
		CreateSubModel(MODEL_PROSTATE, SURFACE_CLOSED);
		SetState(STATE_MODEL, STATE_MODEL_FIRST);
	}
	else
	{
		// mode loaded successfully
		SetSubStateFlag(STATE_MODEL_AUTOMODEL_DONE);

		UrologyModel *model = m_pSurgery->GetUrologyModel();
		SetState(STATE_LESION, STATE_LESION_FIRST);

		// check if lesions have been loaded
		int iNumSubModels = model->GetSubModelCount();
		if (iNumSubModels > 0)
		{
			QList<int> ids = model->GetSubModelIds();
			QMap<int, inurbsSubModel*>* pUrologySubModels = model->GetSubModels();
			inurbsModel* pLesionsModel = m_pSurgery->GetLesionsModel();
			inurbsSubModel* pActiveSubModel = NULL;
			for (int i = 0; i < ids.size(); ++i)
			{
				int id = ids.at(i);
				if (id < 4) // prostate model or critical structures
				{
					inurbsSubModel *subModel = model->GetSubModel(id);
					pVisualEngine->CreateSubModelDisplayObject(id, subModel);
					pVisualEngine->CreateCurveDisplayObjects(id, subModel);
					pVisualEngine->UpdateSurfaceDisplayObject(id, subModel->GetSurface());
				}
				else
				{
					inurbsSubModel* lesionSubModel = model->GetSubModel(id);
					if (!lesionSubModel)
						continue;

					// remove link from Urology model
					pUrologySubModels->remove(id);

					// mode loaded successfully
					SetSubStateFlag(STATE_LESION_CREATED);

					// add submodel to lesions
					pLesionsModel->AddSubModel(id-4,lesionSubModel);
					pVisualEngine->CreateLesionSubModelDisplayObject(lesionSubModel);
					pVisualEngine->CreateLesionCurveDisplayObjects(lesionSubModel);
					pVisualEngine->UpdateLesionSurfaceDisplayObject(lesionSubModel,lesionSubModel->GetSurface());
					UpdateLesionInfo();
	
					// set first submodel as the active model
					if (!pActiveSubModel)
						pActiveSubModel = lesionSubModel;
				}
			}

			if (pActiveSubModel)
			{
				pVisualEngine->SetActiveLesion(pActiveSubModel);
				pVisualEngine->ShowActiveLesionCentre();

				SetState(STATE_LESION, STATE_LESION_MODEL_CREATED);
				GetMainWindow()->UpdatePiradsControl();
				GetMainWindow()->UpdateRemarksControl();
			}
		}
	}

	if(bEncryptedSurgery)
		PdpRemove(sFilePathSurgery);

	return true;
}

void FusionSurgeryController::CloseCase()
{
	if(!m_pSurgery)
		return;

	GetLogger()->SetCaseLogger("", m_pSurgery->GetEncryptCasePassword());

	delete m_pSurgery;
	m_pSurgery = NULL;

	ClearSubStateFlags(); // clear all sub states
	SetSubStateFlag(STATE_START_FIRST);
}


bool FusionSurgeryController::ImportDICOMImages(QStringList& files,int nDicomFlip, int nCenter, int nWidth, QString sPassword)
{
	if (!m_pSurgery)
		return false;
	if (!m_pSurgery->ImportDICOMImages(files,nDicomFlip, nCenter, nWidth, sPassword))
		return false;

	GetVisualEngine()->UpdateImageStack(m_pSurgery->GetImageStack());
	return true;
}

void FusionSurgeryController::FlipImage_AntPos()
{
	if (!m_pSurgery)
		return;

	m_pSurgery->FlipImage_AntPos();
	GetVisualEngine()->UpdateImageImport();
}

void FusionSurgeryController::FlipImage_ApexBase()
{
	if (!m_pSurgery)
		return;
	
	m_pSurgery->FlipImage_ApexBase();
	GetVisualEngine()->UpdateImageImport();
}

void FusionSurgeryController::ResetToDicomFlip()
{
	if (!m_pSurgery)
		return;

	m_pSurgery->ResetToDicomFlip();
	GetVisualEngine()->UpdateImageImport();
}

bool FusionSurgeryController::ApproveImage()
{
	if(m_pSurgery)
	{
		if(m_pSurgery->SaveImageStack(false, m_pSurgery->GetEncryptCasePassword()))
		{
			return SaveImageStack8Bit(m_pSurgery->GetEncryptCasePassword());
		}
	}

	return false;
}

bool FusionSurgeryController::SaveImageStack8Bit(QString sPassword)
{
	ImageStack *pImageStack = m_pSurgery->GetImageStack();
	if(!pImageStack || pImageStack->GetPixelSize() != 2) return false;

	QString sCasePath = m_pSurgery->GetCasePath();

	pImageStack->WriteImage(sCasePath, true, m_pSurgery->GetEncryptCasePassword());

	typedef itk::Image<short,3> ImageType;
	typedef itk::Image<unsigned char,3> OutputImageType;
	typedef itk::IntensityWindowingImageFilter<ImageType,  OutputImageType>	WindowingFilterType;

	int iPixelSize = pImageStack->GetPixelSize();
	int *dim = pImageStack->GetDimension();
	int iImageSize = dim[0]*dim[1]*dim[2];
	unsigned char *pPixels = pImageStack->GetPixelsPtr();

	int iWindowCenter, iWindowWidth;
	pImageStack->GetWindowCenterWidthUser(iWindowCenter, iWindowWidth);
	if(iWindowCenter == 0 && iWindowWidth == 0)
		pImageStack->GetWindowCenterWidthDicom(iWindowCenter, iWindowWidth);

	ImageType::SizeType size;
	size[0] = dim[0];
	size[1] = dim[1];
	size[2] = dim[2];

	ImageType::RegionType region;
	region.SetSize(size);

	ImageType::Pointer image = ImageType::New();
	image->SetRegions(region);
	image->Allocate();
	memcpy(image->GetBufferPointer(), pPixels, iImageSize*iPixelSize);

	WindowingFilterType::Pointer filter = WindowingFilterType::New();
	filter->SetInput(image);

	filter->SetWindowLevel(iWindowWidth, iWindowCenter);
	filter->SetOutputMinimum(0);
	filter->SetOutputMaximum(255);
	filter->Update();

	QString sImageFolder = sCasePath + "/image";
	QString sFileDat = sImageFolder + "/" + IMAGE_FILE_NAME;
	FILE *fp = fopen(sFileDat.toLatin1().data(), "wb");
	if(!fp) return false;

	bool bWriteOk = false;
	if(fwrite(filter->GetOutput()->GetBufferPointer(), 1, iImageSize, fp) == iImageSize)
		bWriteOk = true;

	fclose(fp);

#ifdef PDP_DATA
	if (!PdpEncrypt2(sFileDat, sPassword))
		return false;
#endif

	return bWriteOk;
}

void FusionSurgeryController::DeleteImageStack()
{
	if (!m_pSurgery)
		return;
	m_pSurgery->DeleteImageStack();
	GetVisualEngine()->UpdateImageStack(NULL);
}

void FusionSurgeryController::NewLesionsModel()
{
	if (!m_pSurgery)
		return;

	m_pSurgery->NewLesionsModel();
}

void FusionSurgeryController::CreateLeisionSubModel()
{
	if (!m_pSurgery)
		return;

	inurbsSubModel* pSubModel = m_pSurgery->CreateLesionSubModel();

	GetVisualEngine()->CreateLesionSubModelDisplayObject(pSubModel);
}

void FusionSurgeryController::DeleteActiveLesionSubModel()
{
	if (!m_pSurgery)
		return;

	// get active lesion
	inurbsSubModel* pLesion = GetVisualEngine()->GetActiveLesion();

	// delete submodel display object
	GetVisualEngine()->DeleteLesionSubModelDisplayObject(pLesion);

	// delete lesion submodel 
	m_pSurgery->DeleteLesionSubModel(pLesion);

	// update labels texts after the deletion
	GetVisualEngine()->UpdateAllLabelTexts();

	// set state	
	inurbsSubModel* pCurLesion = GetVisualEngine()->GetActiveLesion(); // changed as previous one deleted
	if (pCurLesion == NULL) // no more lesion
		SetState(STATE_LESION, STATE_LESION_FIRST);
	else
		UpdateLesionModelState(pCurLesion);
}

int FusionSurgeryController::GetNumLesionSubModels()
{
	if (!m_pSurgery)
		return 0;
	return m_pSurgery->GetNumLesionSubModels();
}

int FusionSurgeryController::GetNumValidLesionSubModels()
{
	if (!m_pSurgery)
		return 0;
	return m_pSurgery->GetNumValidLesionSubModels();
}

int FusionSurgeryController::GetLesionSubModelId(inurbsSubModel* pLesion)
{
	if (!m_pSurgery)
		return 0;
	return m_pSurgery->GetLesionSubModelId(pLesion);
}

inurbsSubModel* FusionSurgeryController::GetNextLesionSubModel(inurbsSubModel* pCurLesion)
{
	if (!m_pSurgery)
		return NULL;
	return m_pSurgery->GetNextLesionSubModel(pCurLesion);
}

inurbsSubModel* FusionSurgeryController::GetPrevLesionSubModel(inurbsSubModel* pCurLesion)
{
	if (!m_pSurgery)
		return NULL;
	return m_pSurgery->GetPrevLesionSubModel(pCurLesion);
}

void FusionSurgeryController::BuildLesionSurface(inurbsSubModel* pLesion)
{
	int iState = GetState();
	int iSubState = GetSubState();

	//if (iState != STATE_LESION)
	//	return;

	if (!pLesion)
		return;

	// get num of closed curves
	int numClosedCurves = pLesion->GetNumOfClosedCurves();

	double position = GetVisualEngine()->GetCurrentPlanePosition();
	inurbsPlanarCurve* currentCurve = pLesion->GetCurve(position);

	// do not build the model if the the current displayed slice has less then 3 points
	// in the contour. It will delete away the loose points. 
	if ((!currentCurve && numClosedCurves >= 2) || // no curve in current plane and there are more than 1 closed curves
		(currentCurve && currentCurve->GetNumOfPoints() >= 3  && numClosedCurves >=2)) // curve in current plane and have more than 3 points
	{
		pLesion->BuildSurface();

		GetVisualEngine()->UpdateLesionSurfaceDisplayObject(pLesion,pLesion->GetSurface());
		UpdateLesionInfo();
	}
	else if (numClosedCurves == 1) // just one curve, show label
	{
		GetVisualEngine()->UpdateLesionCurveLabel(pLesion);
	}
}

void FusionSurgeryController::DeleteLesionCurve(inurbsSubModel* pLesion, double pos)
{

	pLesion->RemoveCurve(pos);	
	int numCurves = pLesion->GetNumOfCurves();

	if (numCurves == 1)
	{
		// remove surface if there are less than 2 curves
		pLesion->RemoveSurface();
		GetVisualEngine()->UpdateLesionSurfaceDisplayObject(pLesion,pLesion->GetSurface());
		UpdateLesionInfo();
		SetState(STATE_LESION,STATE_LESION_FIRSTCURVE_CREATED);		
	}
	// check if there is no more curves in model
	else if (numCurves == 0)
	{
		// remove lesion
		DeleteActiveLesionSubModel();
	}
	else // still maintain as a model
	{
		// should build surface again
		pLesion->BuildSurface();
		GetVisualEngine()->UpdateLesionSurfaceDisplayObject(pLesion,pLesion->GetSurface());
		UpdateLesionInfo();
	}
}

int FusionSurgeryController::AddCurveToLesionModel(inurbsSubModel* pActiveLesion, double x, double y, double z, inurbsPlanarCurve** curCurve)
{
	if (!pActiveLesion) // does not exist, return
		return -1;

	if (!pActiveLesion->GetSurface())
		return -1;

	inurbsPlanarCurve* curve = pActiveLesion->CreateIsoCurve(x,y,z);

	if (curve)
	{
		GetVisualEngine()->CreateLesionCurveDisplayObject(pActiveLesion, curve);
		GetVisualEngine()->UpdateLesionCurveDisplayObject(pActiveLesion,curve);
		*curCurve = curve;
		return 0;
	}
	else
	{
		*curCurve = 0;
		return -1;
	}
}

// return the index and also the curve that contains the point
int FusionSurgeryController::GetPickedLesionPointIndex(inurbsSubModel* pActiveLesion, double x, double y, double z, inurbsPlanarCurve** curve)
{
	if (pActiveLesion == NULL)
		return -1;

	// get curve stack
	inurbsPlanarCurveStack *curveStack = pActiveLesion->GetCurveStack();

	// get curve
	*curve = curveStack->GetCurve(z);

	if (*curve)
		return (*curve)->GetPickedPointIndex(x,y);

	return -1;
}

int FusionSurgeryController::AddLesionPoint(inurbsSubModel* pActiveLesion, double x, double y, double z, inurbsPlanarCurve** curCurve)
{
	if (pActiveLesion == NULL)
		return -1;

	// get curve stack
	inurbsPlanarCurveStack *curveStack = pActiveLesion->GetCurveStack();

	// get curve
	inurbsPlanarCurve *curve = curveStack->GetCurve(z);
	if (!curve) // curve does not exist
	{
		curve = curveStack->CreateCurve(z);
		GetVisualEngine()->CreateLesionCurveDisplayObject(pActiveLesion,curve);
	}

	int pIndex = curve->AddPoint(x,y);
	if (pIndex != -1) // point added successfully
	{

		GetVisualEngine()->UpdateLesionCurveDisplayObject(pActiveLesion,curve);

		UpdateLesionModelState(pActiveLesion);
	}

	*curCurve = curve;
	return pIndex;
}

void FusionSurgeryController::UpdateLesionModelState(inurbsSubModel* pActiveLesion)
{
	if (m_iState != STATE_LESION || pActiveLesion == NULL)
		return;

	int numClosedCurves = pActiveLesion->GetNumOfClosedCurves();

	int numCurves = pActiveLesion->GetNumOfCurves();
	int numPoints = 0;
	if (numCurves == 1)
		numPoints = pActiveLesion->GetCurve(0)->GetNumOfPoints();

	switch (m_iSubState)
	{

		case STATE_LESION_CREATED:
			if (numCurves == 1 && numPoints == 1)
				SetState(STATE_LESION,STATE_LESION_FIRSTPOINT_CREATED);
			else if (numClosedCurves >= 2)
				SetState(STATE_LESION,STATE_LESION_MODEL_CREATED);
		break;

		case STATE_LESION_FIRSTPOINT_CREATED:
			if (numClosedCurves == 1 && numPoints == 3)
				SetState(STATE_LESION,STATE_LESION_FIRSTCURVE_CREATED);
			else if (numClosedCurves >= 2)
				SetState(STATE_LESION,STATE_LESION_MODEL_CREATED);
		break;

		case STATE_LESION_FIRSTCURVE_CREATED:
			if (numClosedCurves == 0)
				SetState(STATE_LESION,STATE_LESION_FIRST);
			else if (numClosedCurves >= 2)
				SetState(STATE_LESION,STATE_LESION_MODEL_CREATED);
		break;

		case STATE_LESION_MODEL_CREATED:
			if (numClosedCurves <= 1)
				SetState(STATE_LESION,STATE_LESION_FIRSTCURVE_CREATED);						
		break;

		default:
		break;
	}
}

void FusionSurgeryController::SetActiveLesionRiskScore(int iScore)
{
	inurbsSubModel* pActiveLesion = GetVisualEngine()->GetActiveLesion();
	if (!pActiveLesion)
		return;
	pActiveLesion->SetRiskScore(iScore);
}

bool FusionSurgeryController::ApproveLesionsModel(QString sRemarks)
{
	if (!m_pSurgery)
		return false;

	m_pSurgery->SaveImageStack(true, m_pSurgery->GetEncryptCasePassword());	// in case windowing values changes

	SaveImageStack8Bit(m_pSurgery->GetEncryptCasePassword());
	return m_pSurgery->SaveLesionsModel(sRemarks);
}

void FusionSurgeryController::UpdateLesionInfo()
{
	QString sLesionInfo = "";
	if(m_pSurgery)
	{
		UrologyModel *pUrologyModel = m_pSurgery->GetUrologyModel();
		//if(pUrologyModel && m_iState >= STATE_LESION)
		if (pUrologyModel)
		{
			int iNumLesion = pUrologyModel->GetNumLesionSubModels();
			for(int k=0; k<iNumLesion; k++)
			{
				inurbsSubModel *pLesionModel = pUrologyModel->GetLesionModel(k);
				if(pLesionModel)
				{
					double fLesionVolume = GetVisualEngine()->GetLesionVolume(pLesionModel);
					QString sLesionVolume = QString::number(fLesionVolume/1000, 'f', 2);

					int iRiskScore = pLesionModel->GetRiskScore();
					QString sRiskScore = (iRiskScore <= 0) ? "NA" : QString::number(iRiskScore);

					if(k > 0) sLesionInfo += "\n";
					sLesionInfo += QString("Lesion %1 Volume (ml): %2, PI-RADS: %3").arg(k+1).arg(sLesionVolume).arg(sRiskScore);
				}
			}
		}
	}

	GetVisualEngine()->UpdateLesionInfo(sLesionInfo);
}

void FusionSurgeryController::SetFirstLesionAsActive()
{
	inurbsSubModel* pLesionFirstModel;
	if (m_pSurgery->GetNumLesionSubModels() > 0) // at least one sub model
	{
		pLesionFirstModel = m_pSurgery->GetUrologyModel()->GetLesionModel(0);
	}
	else
	{
		pLesionFirstModel = NULL;
	}

	GetVisualEngine()->SetActiveLesion(pLesionFirstModel);
}

void FusionSurgeryController::CleanModel(bool bRemoveModelFile)
{
	BaseSurgeryController::CleanModel();

	if(bRemoveModelFile)
	{
		// delete model.xml
		QString sModelFolder = GetFolderPath(m_pSurgery->GetCasePath(), "model");
		QString sModelPath = sModelFolder + "/" + XML_FILE_NAME_MODEL;
		QFile::remove(sModelPath);
	}
}

void FusionSurgeryController::CleanLesion()
{

	//while(m_pSurgery->GetNumLesionSubModels() > 0)
	//	DeleteActiveLesionSubModel();

	if (!m_pSurgery)
		return;

	// get urology model
	UrologyModel* pUrologyModel = m_pSurgery->GetUrologyModel();
	if (!pUrologyModel)
		return;

	pUrologyModel->DeleteAllLesionSubModels();
	GetVisualEngine()->CleanLesion();
}

void FusionSurgeryController::SelectFirstLesion()
{
	inurbsSubModel *pFirstLesion = m_pSurgery->GetUrologyModel()->GetLesionModel(0);
	if(pFirstLesion)
	{
		GetVisualEngine()->SetActiveLesion(pFirstLesion);
		GetVisualEngine()->ShowActiveLesionCentre();

		GetMainWindow()->UpdatePiradsControl();
	}
}

bool FusionSurgeryController::IsContainIncompleteLesionCurves()
{
	int iNumLesionSubmodels = m_pSurgery->GetNumLesionSubModels();

	for (int i = 0;i < iNumLesionSubmodels;i++)
	{
		inurbsSubModel* pLesionSubModel = m_pSurgery->GetUrologyModel()->GetLesionModel(i);
		if (!pLesionSubModel)
			continue;

		if (!pLesionSubModel->IsAllCurvesClosed())
			return true;
	}

	return false;
}

bool FusionSurgeryController::IsContainIncompleteLesions()
{
	int iNumLesionSubmodels = m_pSurgery->GetNumLesionSubModels();

	for (int i = 0;i < iNumLesionSubmodels;i++)
	{
		inurbsSubModel* pLesionSubModel = m_pSurgery->GetUrologyModel()->GetLesionModel(i);
		if (!pLesionSubModel)
			continue;

		if (!pLesionSubModel->IsAllCurvesClosed())
			return true;
		
		if (pLesionSubModel->GetNumOfClosedCurves() < 2)
			return true;
	}

	return false;
}
void FusionSurgeryController::CleanIncompleteLesionCurves()
{
	int iNumLesionSubmodels = m_pSurgery->GetNumLesionSubModels();
	QList<inurbsSubModel*> deleteList;

	for (int i = 0;i < iNumLesionSubmodels;i++)
	{
		inurbsSubModel* pLesionSubModel = m_pSurgery->GetUrologyModel()->GetLesionModel(i);

		if (!pLesionSubModel)
			continue;

		pLesionSubModel->RemoveAllInvalidCurves();

		if (pLesionSubModel->GetNumOfClosedCurves() < 1) // delete lesion if there is no curve left
			deleteList.append(pLesionSubModel);
	}

	for (int i = 0; i < deleteList.size();i++)
	{
		inurbsSubModel* pLesionSubModel = deleteList.at(i);
		m_pSurgery->DeleteLesionSubModel(pLesionSubModel);
	}

}

void FusionSurgeryController::CleanIncompleteLesions()
{
	int iNumLesionSubmodels = m_pSurgery->GetNumLesionSubModels();
	QList<inurbsSubModel*> deleteList;

	for (int i = 0;i < iNumLesionSubmodels;i++)
	{
		inurbsSubModel* pLesionSubModel = m_pSurgery->GetUrologyModel()->GetLesionModel(i);

		if (!pLesionSubModel)
			continue;
		
		pLesionSubModel->RemoveAllInvalidCurves();
		if (pLesionSubModel->GetNumOfClosedCurves() < 2)
			deleteList.append(pLesionSubModel);
	}

	for (int i = 0; i < deleteList.size();i++)
	{
		inurbsSubModel* pLesionSubModel = deleteList.at(i);
		GetVisualEngine()->DeleteLesionSubModelDisplayObject(pLesionSubModel);
		m_pSurgery->DeleteLesionSubModel(pLesionSubModel);
	}

	// update labels texts after the deletion
	GetVisualEngine()->UpdateAllLabelTexts();


}

void FusionSurgeryController::ExitApp()
{
	Sleep(1000);
	QApplication::exit(0);
}

/******************************************************************************/
/* RTStruct functions
/******************************************************************************/

bool FusionSurgeryController::LoadRTStructModel(QStringList files, QString sRTStructFilename, QString sPassword)
{

	if (!m_pSurgery)
		return false;

	// load RT Struct
	if (!m_pSurgery->LoadRTStruct(sRTStructFilename, sPassword))
		return false;

	// convert RT contours to model
	if (!m_pSurgery->ConvertRTContoursToModel(files, sPassword))
		return false;

	GetVisualEngine()->UpdateImageStack(m_pSurgery->GetImageStack());
	
	UrologyModel *pUrologyModel = m_pSurgery->GetUrologyModel();

	// create curves and model display objects
	inurbsSubModel* pProstateSubModel = pUrologyModel->GetSubModel(MODEL_PROSTATE);
	inurbsPlanarCurveStack *pCurveStack = pProstateSubModel->GetCurveStack();
	if (!pCurveStack)
		return false;

	int iNumCurves = pCurveStack->GetNumOfCurves();
	for (int i = 0;i < iNumCurves;i++)
	{
		inurbsPlanarCurve *pCurve;

		pCurve = pCurveStack->GetCurve(i);
		if (pCurve)
		{
			GetVisualEngine()->CreateCurveDisplayObject(MODEL_PROSTATE, pCurve);
			GetVisualEngine()->UpdateCurveDisplayObject(MODEL_PROSTATE, pCurve);
		}
	}

	GetVisualEngine()->UpdateSurfaceDisplayObject(MODEL_PROSTATE, pProstateSubModel->GetSurface());

	// create lesion curves and model display objects
	inurbsModel* pLesionsModel = m_pSurgery->GetLesionsModel();
	int iNumLesions = pLesionsModel->GetSubModelCount();

	for (int i = 0;i < iNumLesions;i++)
	{
		inurbsSubModel* pLesionSubModel = pLesionsModel->GetSubModel(i);
		if (!pLesionSubModel)
		{
			continue;

		}
		int id = pLesionSubModel->GetID();
		GetVisualEngine()->CreateLesionSubModelDisplayObject(pLesionSubModel);
		GetVisualEngine()->CreateLesionCurveDisplayObjects(pLesionSubModel);
		GetVisualEngine()->UpdateLesionSurfaceDisplayObject(pLesionSubModel, pLesionSubModel->GetSurface());
		BuildLesionSurface(pLesionSubModel);
		UpdateLesionInfo();

	}



	return true;
}
