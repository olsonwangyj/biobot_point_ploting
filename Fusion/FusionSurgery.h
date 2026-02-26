/******************************************************************************
    FusionSurgery.h

    Date      : 1 Nov 2018
 ******************************************************************************/

#ifndef FUSION_SURGERY_H
#define FUSION_SURGERY_H

#define XML_FUSION_SURGERY_FILE_VERSION 1

#include <QStandardItemModel>
#include <itkGDCMImageIO.h>
#include <itkImageSeriesReader.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkIntensityWindowingImageFilter.h>

#include "BaseSurgery.h"
#include "DicomDirImporter.h"
#include "RTStruct.h"

class inurbsSubModel;
class inurbsModel;

class FusionSurgery : public BaseSurgery
{
	Q_OBJECT
public:
		
	typedef short PixelType;
	typedef itk::Image<PixelType,3> ImageType;
	typedef itk::Image<unsigned char,3> OutputImageType;
	typedef itk::GDCMImageIO ImageIOType;
	typedef itk::ImageSeriesReader< ImageType > ReaderType;
	typedef itk::GDCMSeriesFileNames NamesGeneratorType;
	typedef itk::IntensityWindowingImageFilter<ImageType,  OutputImageType>	WindowingFilterType;
	NamesGeneratorType::Pointer nameGenerator;

	enum FLIP {FLIP_NONE = 0, FLIP_X = 1, FLIP_Y = 2, FLIP_Z = 4};

	FusionSurgery();
	~FusionSurgery();
	
	// Open case functions
	bool LoadImageStack(QString sCasePath, QString sPassword=""); // pass in case path as Read function of ImageStack takes case path as argument
	bool LoadModel(QString sCasePath, QString sPassword="");

	// Dicom functions
	
	bool ImportDICOMImages(QStringList &files,int nDicomFlip,int nCenter, int nWidth, QString sPassword="");
	bool ImportDICOMImages(QVector<QStringList>& vecSeriesFileList, int nDicomFlip, int nCenter, int nWidth, QString sPassword = "");
	void ResetToDicomFlip();
	void FlipImageStack(int iFlip);
	void FlipImage_AntPos();
	void FlipImage_ApexBase();

	// model functions
	void InitModelLimitPositions(); // overwrite

	// lesion functions
	void NewLesionsModel(); // new the lesion list
	inurbsModel* GetLesionsModel();
	inurbsSubModel* CreateLesionSubModel();
	int GetLesionSubModelId(inurbsSubModel* pLesion);
	inurbsSubModel* GetNextLesionSubModel(inurbsSubModel* pCurLesion);
	inurbsSubModel* GetPrevLesionSubModel(inurbsSubModel* pCurLesion);
	int GetNumLesionSubModels();
	int GetNumValidLesionSubModels();
	void DeleteLesionSubModel(inurbsSubModel* pLesion);
	bool SaveLesionsModel(QString sRemarks);

	// RTStruct functions
	bool LoadRTStruct(QString sFileName, QString sPassword);
	bool ConvertRTContoursToModel(QStringList files, QString sPassword);

protected:


	
	
	int m_iCurrentFlip;
	//int m_iDicomWindowCenter, m_iDicomWindowWidth;
    //double m_fDirCosines[6];
	RTStruct *m_pRTStruct;
	

signals:
	void progressChanged(int value);
};

#endif
