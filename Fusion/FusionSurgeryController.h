/******************************************************************************
    FusionSurgeryController.h

    Date      : 29 Oct 2018
 ******************************************************************************/

#ifndef FUSION_SURGERY_CONTROLLER_H
#define FUSION_SURGERY_CONTROLLER_H
 
#include "BaseSurgeryController.h"

class FusionSurgery;
class inurbsSubModel;
class DicomDirImporter;

class FusionSurgeryController : public BaseSurgeryController
{
	Q_OBJECT

public:
	FusionSurgeryController();
	~FusionSurgeryController();

	static FusionSurgeryController* GetInstance();
	static void DeleteInstance();

	// patient data folder setting
	QString GetPatientDataFolder() { return m_sPatientDataFolder; }
	void SetPatientDataFolder(QString sFolder);

	// patient nationality setting
	QString GetPatientNationality() { return m_sPatientNationality; }
	void SetPatientNationality(QString sNationality);


	int GoBackToState(int iState=-1);

	// Case functions
	bool EncryptCase(QString sCaseDirPath, QString sDecryptPassword, QString sEncryptPassword);
	bool EncryptCaseFile(QString sFilePath, QString sPassword="");

	// Surgery functions
//	bool CreateSurgery(QString sPatientDataFolder, QString sCaseId);
	bool OpenCase(QString sCaseDirPath, QString sPassword="");
	void CloseCase();

	// Import Image stage functions
	
	bool ImportDICOMImages(QStringList& files,int nDicomFlip, int nCenter, int nWidth, QString sPassword="");
	void FlipImage_AntPos();
	void FlipImage_ApexBase();
	void ResetToDicomFlip();
	bool ApproveImage();
	void DeleteImageStack();
	bool SaveImageStack8Bit(QString sPassword="");

	// lesion model functions
	void NewLesionsModel();
	void CreateLeisionSubModel();
	void DeleteActiveLesionSubModel();
	int GetNumLesionSubModels();
	int GetNumValidLesionSubModels();
	int GetLesionSubModelId(inurbsSubModel* pLesion);
	inurbsSubModel* GetNextLesionSubModel(inurbsSubModel* pCurLesion);
	inurbsSubModel* GetPrevLesionSubModel(inurbsSubModel* pPrevLesion);
	void BuildLesionSurface(inurbsSubModel* pLesion);
	void DeleteLesionCurve(inurbsSubModel* pLesion, double pos);
	int AddCurveToLesionModel(inurbsSubModel* pActiveLesion, double x, double y, double z, inurbsPlanarCurve** curCurve);
	int GetPickedLesionPointIndex(inurbsSubModel* pActiveLesion, double x, double y, double z, inurbsPlanarCurve** curve);
	int AddLesionPoint(inurbsSubModel* pActiveLesion, double x, double y, double z, inurbsPlanarCurve** curCurve);
	void UpdateLesionModelState(inurbsSubModel* pActiveLesion);
	void SetActiveLesionRiskScore(int iScore);
	bool ApproveLesionsModel(QString sRemarks);
	void UpdateLesionInfo();
	void SetFirstLesionAsActive();

	// RTStruct functions
	bool LoadRTStructModel(QStringList files, QString sRTStructFilename, QString sPassword="");

	// clean functions
	void CleanModel(bool bRemoveModelFile);
	void CleanLesion();
	void CleanIncompleteLesionCurves();
	void CleanIncompleteLesions(); // remove incomplete curves and incomplete models
	bool IsContainIncompleteLesions();
	bool IsContainIncompleteLesionCurves();

	void ExitApp();

protected:
	void UpdateState(int state, int subState);

	bool ReadAppConfig();
	bool WriteAppConfig();

protected:
	static FusionSurgeryController* m_pInstance;

	// surgery variable
	QString m_sPatientNationality;
	QString m_sPatientDataFolder;

protected slots:
	void SelectFirstLesion();
};

#endif