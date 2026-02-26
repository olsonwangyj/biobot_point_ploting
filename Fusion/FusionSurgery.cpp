/******************************************************************************
    FusionSurgery.cpp

    Date      : 1 Nov 2018
 ******************************************************************************/

#pragma warning(disable:4996)

#include <QFileinfo>
#include <QDir>
#include <QMath.h>
#include <gdcmAttribute.h>

#include "FusionSurgery.h"
#include "RTROI.h"
#include "inurbsSubModel.h"
#include "inurbsPlanarCurve.h"
#include "inurbsPlanarCurveStack.h"
#include "CommonClasses.h"
#include "Crypto.h"
#include "Constants.h"



/******************************************************************************/
/* Constructos and Destructors                                                                           
/******************************************************************************/

FusionSurgery::FusionSurgery()
{


	
	m_iCurrentFlip = FLIP_NONE;
	//m_iDicomWindowCenter = 0;
	//m_iDicomWindowWidth = 0;
	m_pRTStruct = NULL;

	//for (int i=0;i<6;i++)
	//	m_fDirCosines[i] = 0.0;
}

FusionSurgery::~FusionSurgery()
{


	if (m_pRTStruct)
		delete m_pRTStruct;
}

/******************************************************************************/
/* Open case functions                                                                           
/******************************************************************************/

bool FusionSurgery::LoadImageStack(QString sCasePath, QString sPassword)
{
	if(BaseSurgery::LoadImageStack(sCasePath, sPassword))
	{
		m_pImageStack->SetModality(ImageStack::MODALITY_MRI);
		return true;
	}

	return false;
}

bool FusionSurgery::LoadModel(QString sCasePath, QString sPassword)
{
	if (!m_pUrologyModel)
		return false;

	if (!m_pUrologyModel->ReadModel(sCasePath, sPassword, XML_FILE_NAME_MODEL))
	{
		delete m_pUrologyModel;
		m_pUrologyModel = NULL;
		return false;
	}
	m_pUrologyModel->BuildAllSurfaces();

	return true;
}

/******************************************************************************/
/* Dicom functions                                                                           
/******************************************************************************/


bool FusionSurgery::ImportDICOMImages(QStringList& files, int nDicomFlip,int nDicomWindowCenter, int nDicomWindowWidth, QString sPassword)
{
	ReaderType::Pointer reader;
	ImageIOType::Pointer dicomIO;
	ImageType::Pointer image;

	emit progressChanged(0);

	m_iCurrentFlip = FLIP_NONE;

	// create temp dir in the case path 
	QString strDest = m_sCasePath + "/temp_files/";
	RemoveDir(strDest);
	QDir().mkdir(strDest);

	// copy dicom files over to temp dir
	int iCount = files.count();
	for (int i = 0; i < iCount; i++)
	{
		QFileInfo qfileInfo(files.at(i));
		QString qfilename = qfileInfo.fileName();

		bool bEncrypted = false;
		if (sPassword != "")
			bEncrypted = PdpDecrypt2(files.at(i), sPassword);

		QString srcFile = files.at(i);
		if (qfilename.right(4) == ENCRYPTION_FILE_EXTENSION)
		{
			qfilename.truncate(qfilename.lastIndexOf(QChar('.')));
			srcFile.truncate(srcFile.lastIndexOf(QChar('.')));
		}

		QFile::copy(srcFile, strDest + qfilename + ".DCM");

		if (bEncrypted)
			PdpRemove(srcFile);

		int iProgress = ((float)i/iCount) * 20;
		emit progressChanged(iProgress);
	}

	// load dicom image files
	reader = ReaderType::New();
	dicomIO = ImageIOType::New();
	reader->SetImageIO( dicomIO );

	nameGenerator = NamesGeneratorType::New();
	nameGenerator->SetUseSeriesDetails( true );
	nameGenerator->SetLoadSequences( true );
	nameGenerator->SetLoadPrivateTags( true );
	nameGenerator->SetInputDirectory(strDest.toLocal8Bit().data());
	const ReaderType::FileNamesContainer & filenames = nameGenerator->GetInputFileNames();
	reader->SetFileNames(filenames);
	
	try
	{
		reader->Update();
	}
	catch (itk::ExceptionObject&)
	{
		// remove directory if error
		RemoveDir(strDest);
		return false;
	}

	// remove dir immediately after reading
	RemoveDir(strDest);

	emit progressChanged(50);

	image = ImageType::New();
	image = reader->GetOutput();

	const ImageType::SpacingType& spacing0 = image->GetSpacing();
	const ImageType::SizeType& size0 = image->GetBufferedRegion().GetSize();
	ImageType::PointType origin0 = image->GetOrigin();

	double spacing[3],origin[3];
	int size[3];
	for (int i=0;i<3;i++)
	{
		spacing[i] = spacing0[i];
		origin[i] = origin0[i];
		size[i] = size0[i];
	}

	if(1)
	{
		// re-position the origin of volume, from old Uro-Fusion (svn 190)
		origin[0] = -(spacing[0] * size[0] / 2);
		origin[1] = -(spacing[1] * size[1] / 2) + 30;
//		origin[2] = ConeHeightCenter_Z + 80;
		origin[2] = -(spacing[2] * size[2] / 2) -100;	// calibration jig surface position
	}

	// create image stack
	if (CreateImageStack(size,origin,spacing,2)) 
	{
		emit progressChanged(60);

		// apply window center and window center to images
		if (nDicomWindowWidth == 0 && nDicomWindowCenter == 0) // both are 0, which means no information of them are available in dicom
		{
			// find min & max pixel values
			typedef itk::MinimumMaximumImageCalculator<ImageType> CalType;
			CalType::Pointer cal = CalType::New();
			cal->SetImage(image);
			cal->Compute();
			int minVal = cal->GetMinimum();
			int maxVal = cal->GetMaximum();

			nDicomWindowCenter = minVal + (maxVal-minVal)/2;
			nDicomWindowWidth = maxVal-minVal;

			double minCenter = minVal;
			double maxCenter = maxVal;
			double minWidth = 1;
			double maxWidth = maxVal*2;

			cal = NULL;
		}
		
		emit progressChanged (70);

		//WindowingFilterType::Pointer filter = WindowingFilterType::New();
		//filter->SetInput(image);

		//filter->SetWindowLevel(m_fDicomWindowWidth, m_fDicomWindowCenter);
		//filter->SetOutputMinimum(0);
		//filter->SetOutputMaximum(255);

		//try
		//{
		//	filter->Update();
		//}
		//catch (itk::ExceptionObject &ex)
		//{
		//	std::cout << ex << std::endl;
		//	return false;
		//}

		//emit progressChanged(80);

		//// copy image over
		//memcpy(m_pImageStack->GetPixelsPtr(),filter->GetOutput()->GetBufferPointer(), m_pImageStack->GetSize());

		// copy image over
		memcpy(m_pImageStack->GetPixelsPtr(), image->GetBufferPointer(), m_pImageStack->GetSize());

		emit progressChanged(90);

		// flip image with the orientation 
		FlipImageStack(nDicomFlip);

		m_pImageStack->SetModality(ImageStack::MODALITY_MRI);
		m_pImageStack->SetWindowCenterWidthDicom(nDicomWindowCenter, nDicomWindowWidth);
	}
	
	emit progressChanged(100);

	return true;
}

bool FusionSurgery::ImportDICOMImages(QVector<QStringList>& vecSeriesFileList, int nDicomFlip, int nCenter, int nWidth, QString sPassword /*= ""*/)
{
	if (vecSeriesFileList.count() > 1)
	{

	}
	else
	{
		return this->ImportDICOMImages(vecSeriesFileList[0], nDicomFlip, nCenter, nWidth, sPassword);
	}
}

void FusionSurgery::FlipImageStack(int iFlip)
{
	if (!m_pImageStack)
		return;

	unsigned char* pImage = m_pImageStack->GetPixelsPtr();
	int* size = m_pImageStack->GetDimension();
	int iPixelSize = m_pImageStack->GetPixelSize();

	int line_len = size[0];
	int page_len = size[0] * size[1];

	if (iFlip & FLIP_Z)
	{
		// just allocate one page instead of the whole image stack and swap. 
		unsigned char* tmpPage = new unsigned char[page_len*iPixelSize];
		int swapTimes = size[2]/2;
		unsigned char* src = pImage;
		unsigned char* dest = pImage + page_len*iPixelSize * (size[2]-1);
		for (int i=0;i<swapTimes;i++)
		{
			memcpy(tmpPage,src,page_len*iPixelSize);
			memcpy(src,dest,page_len*iPixelSize);
			memcpy(dest,tmpPage,page_len*iPixelSize);
			src += page_len*iPixelSize;
			dest -= page_len*iPixelSize;
		}
		delete [] tmpPage;
	}

	if (iFlip & FLIP_Y)
	{
		// just allocated one line instead of whole image stack and swap. 
		unsigned char* tmpLine = new unsigned char[line_len*iPixelSize];
		int swapTimes = size[1]/2;

		for (int i=0;i<size[2];i++)
		{
			unsigned char* src = pImage + i*page_len*iPixelSize;
			unsigned char* dest = src + line_len*iPixelSize*(size[1]-1);
			for (int j=0;j<swapTimes;j++)
			{
				memcpy(tmpLine,src,line_len*iPixelSize);
				memcpy(src,dest,line_len*iPixelSize);
				memcpy(dest,tmpLine,line_len*iPixelSize);
				src += line_len*iPixelSize;
				dest -= line_len*iPixelSize;
			}
		}
		delete [] tmpLine;
	}
	
	if (iFlip & FLIP_X)
	{
		unsigned char *tmpPixel = new unsigned char [iPixelSize];
		for (int k=0; k<size[2]; k++)
		{
			for (int j=0; j<size[1]; j++)
			{
				unsigned char* src = pImage + k*page_len*iPixelSize + j*size[0]*iPixelSize;
				unsigned char* dest = src + iPixelSize*(size[0]-1);
				for (int i=0; i<size[0]/2; i++)
				{
					memcpy(tmpPixel, src, iPixelSize);
					memcpy(src, dest, iPixelSize);
					memcpy(dest, tmpPixel, iPixelSize);

					src += iPixelSize;
					dest -= iPixelSize;
				}
			}
		}
		delete [] tmpPixel;
	}
}

void FusionSurgery::FlipImage_AntPos()
{
	int iFlip = FLIP_X | FLIP_Y;
	FlipImageStack(iFlip);
	m_iCurrentFlip ^= iFlip;
}

void FusionSurgery::FlipImage_ApexBase()
{
	int iFlip = FLIP_X | FLIP_Z;
	FlipImageStack(iFlip);
	m_iCurrentFlip ^= iFlip;
}

void FusionSurgery::ResetToDicomFlip()
{
	FlipImageStack(m_iCurrentFlip);
	m_iCurrentFlip = FLIP_NONE;
}

/******************************************************************************/
/* Model functions                                                                           
/******************************************************************************/

void FusionSurgery::InitModelLimitPositions()
{
	if (!m_pUrologyModel)
		return;

	m_pUrologyModel->InitLimitPositions();
}

/******************************************************************************/
/* Lesion functions                                                                           
/******************************************************************************/

void FusionSurgery::NewLesionsModel()
{
	if (!m_pUrologyModel)
		return;

	m_pUrologyModel->NewLesionsModel();
}

inurbsModel* FusionSurgery::GetLesionsModel()
{
	if (!m_pUrologyModel)
		return NULL;

	return m_pUrologyModel->GetLesionsModel();
}

inurbsSubModel* FusionSurgery::CreateLesionSubModel()
{
	if (!m_pUrologyModel)
		return NULL;

	inurbsSubModel* pLesionSubModel = m_pUrologyModel->CreateLesionSubModel();

	return pLesionSubModel;
}

void FusionSurgery::DeleteLesionSubModel(inurbsSubModel* pLesion)
{
	if (!m_pUrologyModel)
		return;

	m_pUrologyModel->DeleteLesionSubModel(pLesion);
}

int FusionSurgery::GetLesionSubModelId(inurbsSubModel* pLesion)
{
	if (!m_pUrologyModel)
		return 0;

	return m_pUrologyModel->GetLesionSubModelId(pLesion);
}

inurbsSubModel* FusionSurgery::GetNextLesionSubModel(inurbsSubModel* pCurLesion)
{
	if (!m_pUrologyModel)
		return NULL;
	return m_pUrologyModel->GetNextLesionSubModel(pCurLesion);
}

inurbsSubModel* FusionSurgery::GetPrevLesionSubModel(inurbsSubModel* pCurLesion)
{
	if (!m_pUrologyModel)
		return NULL;
	return m_pUrologyModel->GetPrevLesionSubModel(pCurLesion);
}

int FusionSurgery::GetNumLesionSubModels()
{
	if (!m_pUrologyModel)
		return 0;

	return m_pUrologyModel->GetNumLesionSubModels();
}

int FusionSurgery::GetNumValidLesionSubModels()
{
	if (!m_pUrologyModel)
		return 0;
	return m_pUrologyModel->GetNumValidLesionSubModels();
}

bool FusionSurgery::SaveLesionsModel(QString sRemarks)
{
	if (!m_pUrologyModel)
		return false;

	return m_pUrologyModel->SaveLesionsModel(m_sCasePath,sRemarks, m_sEncryptCasePassword);
}

/******************************************************************************/
/* RTStruct functions
/******************************************************************************/

bool FusionSurgery::LoadRTStruct(QString sFileName, QString sPassword)
{
	bool bEncryptedRTFile = false;

	if (sPassword != "")
	{
		bEncryptedRTFile = PdpDecrypt2(sFileName, sPassword);
		if (!bEncryptedRTFile)
			return false;
	}


	QString srcFile = sFileName;
	if (srcFile.right(4) == ENCRYPTION_FILE_EXTENSION)
	{
		srcFile.truncate(srcFile.lastIndexOf(QChar('.')));
	}

	// read file
	gdcm::Reader RTreader;
	//RTreader.SetFileName(sFileName.toLatin1().data());
	RTreader.SetFileName(srcFile.toLatin1().data());


	if (!RTreader.Read())
	{
		if (bEncryptedRTFile)
			PdpRemove(srcFile);
		return false;
	}

	//  Tag(3006, 0020) ?Identify the start of structure set ROI sequence.
	//	Tag(3006, 0039) ?To identify start of ROI contour sequence
	//	Identify the number of structures found.
	//	For each structure
	//		Tag(3006, 0026) Identify user - defined name of ROI.
	//		Tag(3006, 0040) Extract the contour sequence.
	//		For each item in the current structure :
	//			Tag(3006, 0050) Extract contour data.

	//  const gdcm::FileMetaInformation &h = RTreader.GetFile().GetHeader();
	const gdcm::DataSet& ds = RTreader.GetFile().GetDataSet();

	gdcm::Tag tssroisq(0x3006, 0x0020); // Structure Set ROI sequence
	if (!ds.FindDataElement(tssroisq))
	{
		if (bEncryptedRTFile)
			PdpRemove(srcFile);
		return false;
	}

	gdcm::Tag troicsq(0x3006, 0x0039); // ROI contour sequence attribute
	if (!ds.FindDataElement(troicsq))
	{
		if (bEncryptedRTFile)
			PdpRemove(srcFile);
		return false;
	}

	const gdcm::DataElement &roicsq = ds.GetDataElement(troicsq);

	gdcm::SmartPointer<gdcm::SequenceOfItems> sqi = roicsq.GetValueAsSQ();
	if (!sqi || !sqi->GetNumberOfItems())
	{
		if (bEncryptedRTFile)
			PdpRemove(srcFile);
		return false;
	}

	const gdcm::DataElement &ssroisq = ds.GetDataElement(tssroisq);
	gdcm::SmartPointer<gdcm::SequenceOfItems> ssqi = ssroisq.GetValueAsSQ();
	if (!ssqi || !ssqi->GetNumberOfItems())
	{
		if (bEncryptedRTFile)
			PdpRemove(srcFile);
		return false;
	}

	if (m_pRTStruct)
		delete m_pRTStruct;
	m_pRTStruct = new RTStruct;

	
	for (unsigned int pd = 0; pd < sqi->GetNumberOfItems(); ++pd) // number of ROIs
	{

		const gdcm::Item & item = sqi->GetItem(pd + 1); // Item start at #1
		//std::cout << item << std::endl;

		// get ROI name
		const gdcm::Item & sitem = ssqi->GetItem(pd + 1); // Item start at #1
		const gdcm::DataSet& snestedds = sitem.GetNestedDataSet();
		gdcm::Tag stcsq(0x3006, 0x0026); // ROI name
		if (!snestedds.FindDataElement(stcsq))
		{ 
			continue;
		}
		
		RTROI* pROI = new RTROI();
		const gdcm::DataElement &sde = snestedds.GetDataElement(stcsq);
		std::string s(sde.GetByteValue()->GetPointer(), sde.GetByteValue()->GetLength());
		pROI->SetName(s.c_str());


		// get contour sequence
		const gdcm::DataSet& nestedds = item.GetNestedDataSet();
		gdcm::Tag tcsq(0x3006, 0x0040); // Sequence of Contours defining ROI. 
		if (!nestedds.FindDataElement(tcsq)) // no contours
		{
			delete pROI;
			continue;
		}
		const gdcm::DataElement& csq = nestedds.GetDataElement(tcsq);

		gdcm::SmartPointer<gdcm::SequenceOfItems> sqi2 = csq.GetValueAsSQ();
		if ((!sqi2) || !sqi2->GetNumberOfItems())
		{
			delete pROI;
			continue;
		}
		size_t nitems = sqi2->GetNumberOfItems(); 
									

		for (unsigned int ii = 0; ii < nitems; ++ii) // number of contours
		{
			const gdcm::Item & item2 = sqi2->GetItem(ii + 1); // Item start at #1
			const gdcm::DataSet& nestedds2 = item2.GetNestedDataSet();

			// (3006,0050) DS [43.57636\65.52504\-10.0\46.043102\62.564945\-10.0\49.126537\60.714... # 398,48 ContourData
			gdcm::Tag tcontourdata(0x3006, 0x0050);
			const gdcm::DataElement & contourdata = nestedds2.GetDataElement(tcontourdata);

			// type of contour
			gdcm::Attribute<0x3006, 0x0042> contgeotype;
			contgeotype.SetFromDataSet(nestedds2);
			const char* vv = contgeotype.GetValue();
			assert(contgeotype.GetValue() == "CLOSED_PLANAR " || contgeotype.GetValue() == "POINT " || contgeotype.GetValue() == "OPEN_NONPLANAR ");

			gdcm::Attribute<0x3006, 0x0046> numcontpoints;
			numcontpoints.SetFromDataSet(nestedds2);

			// point type
			if (contgeotype.GetValue() == "POINT ")
			{
				assert(numcontpoints.GetValue() == 1);
			}

			gdcm::Attribute<0x3006, 0x0050> at;
			at.SetFromDataElement(contourdata);

			if (contgeotype.GetValue() == "CLOSED_PLANAR " || contgeotype.GetValue() == "OPEN_NONPLANAR ")
			{
				RTContour* pContour = new RTContour;

				if (nestedds2.FindDataElement(gdcm::Tag(0x3006, 0x0016)))
				{
					const gdcm::DataElement &contourimagesequence = nestedds2.GetDataElement(gdcm::Tag(0x3006, 0x0016)); // contour image sequence
					gdcm::SmartPointer<gdcm::SequenceOfItems> contourimagesequence_sqi = contourimagesequence.GetValueAsSQ();
					assert(contourimagesequence_sqi && contourimagesequence_sqi->GetNumberOfItems() == 1);
					const gdcm::Item & theitem = contourimagesequence_sqi->GetItem(1);
					const gdcm::DataSet& thenestedds = theitem.GetNestedDataSet();

					gdcm::Attribute<0x0008, 0x1150> classat;
					classat.SetFromDataSet(thenestedds);
					gdcm::Attribute<0x0008, 0x1155> instat;
					instat.SetFromDataSet(thenestedds);

					pContour->m_strRefSOPInstanceUID = instat.GetValue();

					//printf("ref sop instance uid = %s\n", pContour->m_strRefSOPInstanceUID.toLatin1().data());
					
				}


				const double* pts = at.GetValues();
				unsigned int npts = at.GetNumberOfValues() / 3;

				//pROI->m_numPoints.append(npts);
				
				double* pPts = new double[npts * 3];
				for (unsigned int j = 0; j < npts * 3; j += 3)
				{
					pPts[j] = pts[j + 0];
					pPts[j + 1] = pts[j + 1];
					pPts[j + 2] = pts[j + 2];
				}

				// add contour
				pContour->m_iNumPoints = npts;
				pContour->m_pPoints = pPts;
				pROI->AddContour(pContour);

				//pROI->m_contours.append(pPts);
			}
		}
		m_pRTStruct->AddROI(pROI);

	}

	if (bEncryptedRTFile)
		PdpRemove(srcFile);

	return true;
}

bool FusionSurgery::ConvertRTContoursToModel(QStringList files, QString sPassword)
{
	if (!m_pRTStruct || !m_pRTStruct->GetNumROIs())
		return false;

	// create temp dir in the case path 
	QString strDest = m_sCasePath + "/temp_files/";
	RemoveDir(strDest);
	QDir().mkdir(strDest);

	// copy dicom files over to temp dir
	int iCount = files.count();
	for (int i = 0; i < iCount; i++)
	{
		QFileInfo qfileInfo(files.at(i));
		QString qfilename = qfileInfo.fileName();

		bool bEncrypted = false;
		if (sPassword != "")
			bEncrypted = PdpDecrypt2(files.at(i), sPassword);

		QString srcFile = files.at(i);
		if (qfilename.right(4) == ENCRYPTION_FILE_EXTENSION)
		{
			qfilename.truncate(qfilename.lastIndexOf(QChar('.')));
			srcFile.truncate(srcFile.lastIndexOf(QChar('.')));
		}

		QFile::copy(srcFile, strDest + qfilename + ".DCM");

		if (bEncrypted)
			PdpRemove(srcFile);

		//int iProgress = ((float)i / iCount) * 20;
		//emit progressChanged(iProgress);
	}


	// get sorted files according to positions of images
	typedef itk::GDCMSeriesFileNames NamesGeneratorType;
	NamesGeneratorType::Pointer nameGenerator = NamesGeneratorType::New();
	nameGenerator->SetUseSeriesDetails(true);
	nameGenerator->SetLoadSequences(true);
	nameGenerator->SetLoadPrivateTags(true);
	nameGenerator->SetInputDirectory(strDest.toLatin1().data());
	const FusionSurgery::ReaderType::FileNamesContainer & sortedFileNames = nameGenerator->GetInputFileNames();

	int iNumImages = sortedFileNames.size();

	// read origins of each image in the proper order.
	//double *pOrigins = new double[iNumImages * 3];

	// create a map that will index into the image with the sop uid
	int iIndex = 0;
	QMap<QString, int> sopInstanceUIDIndexMap;
	for (int i = 0; i < iNumImages; i++)
	{
		gdcm::Reader dcmReader;
		dcmReader.SetFileName(sortedFileNames[i].c_str());

		if (!dcmReader.Read())
		{
			return false;
		}

		const gdcm::DataSet& ds = dcmReader.GetFile().GetDataSet();

		gdcm::Tag tsopUid(0x0008, 0x0018); // SOP Instance UID
		const gdcm::DataElement & sopUid = ds.GetDataElement(tsopUid);
		if (!ds.FindDataElement(tsopUid)) // cannot find sop instance uid
			continue;

		gdcm::Attribute<0x0008, 0x0018> at;
		at.SetFromDataElement(sopUid);

		QString s = at.GetValue();
		sopInstanceUIDIndexMap.insert(s, i);

		printf("filename %s\n", sortedFileNames[i].c_str());

		//gdcm::Tag tpatientImagePos(0x0020, 0x0032);
		//const gdcm::DataElement & patientImagePos = ds.GetDataElement(tpatientImagePos);
		//if (!ds.FindDataElement(tpatientImagePos)) // cannot find patient image pos
		//	continue;

		//gdcm::Attribute<0x0020, 0x0032> at;
		//at.SetFromDataElement(patientImagePos);

		//const double* pts = at.GetValues();
		//unsigned int npts = at.GetNumberOfValues();
		//if (npts != 3)
		//	continue;

		//gdcm::Attribute<0x0020, 0x1041> at1;
		//at1.SetFromDataElement(sliceLocation);

		//pOrigins[i*3] = pts[0];
		//pOrigins[i*3+1] = pts[1];
		//pOrigins[i*3+2] = at1.GetValue();


		//printf("image %d, origin x = %4.2f, y = %4.2f\n", i, pOrigins[iIndex], pOrigins[iIndex + 1]);

		iIndex += 3;
	}

	// testing, print out the map
	QMapIterator<QString, int> its(sopInstanceUIDIndexMap);
	while (its.hasNext())
	{
		its.next();
		QString ss = its.key();
		int index = its.value();
		printf("%s, %d\n", ss.toLatin1().data(), index);
	}
	
	double* spacing = m_pImageStack->GetSpacing();
	int iImageWidth = m_pImageStack->GetWidth();
	int iImageHeight = m_pImageStack->GetHeight();
	int iImageSlices = m_pImageStack->GetNumSlices();
	double*worldOrigin = m_pImageStack->GetOrigin();

	// get number of roi
	int iNumROIs = m_pRTStruct->GetNumROIs();

	inurbsSubModel* pSubModel;
	inurbsPlanarCurveStack *pCurveStack;

	typedef itk::Point< double, 3 > PointType;
	double fDistanceLimit, fDistanceLimit1, fDistanceLimit2;

	int *pMaxNumPoints = new int[iNumImages];

	for (int i = 0;i < iNumROIs;i++)
	{
		printf("Roi %d\n",i);
		// here it is assumed that m_pUrologyModel and the submodel is already created
		// TODO PLE: revisit here when we work out the workflow
		// assuming that the first ROI is the prostate
		if (i == 0)
		{
			pSubModel = m_pUrologyModel->GetSubModel(MODEL_PROSTATE);
		}
		else
		{
			pSubModel = CreateLesionSubModel();
		}

		pCurveStack = pSubModel->GetCurveStack();
		RTROI* pROI = m_pRTStruct->GetROI(i);
		int iNumCurves = pROI->GetNumContours();

		if (i != 0)
			printf("ROI %d, num of curves = %d\n ", i, iNumCurves);

		for (int n = 0;n < iNumImages;n++)
		{
			pMaxNumPoints[n] = 0;
		}

		for (int j = 0; j < iNumCurves; j++)
		{
			RTContour *pContour = pROI->GetContour(j);
			int iNumPoints = pContour->m_iNumPoints;
			double *pPts = pContour->m_pPoints;

			printf("curve %d\n", j);

			if (i == 0) // prostate contour
			{
				fDistanceLimit1 = 8.0;
			}
			else
			{ 
				fDistanceLimit1 = iNumPoints / 15.0;
			}
			//fDistanceLimit2 = 2.0;
			fDistanceLimit2 = 4.0;

			int iSliceOriginIndex = sopInstanceUIDIndexMap.value(pContour->m_strRefSOPInstanceUID);

			// load the contour referenced image for later transformation
			using ReaderType1 = itk::ImageFileReader<ImageType>;
			ReaderType1::Pointer reader;
			reader = ReaderType1::New();
			reader->SetFileName(sortedFileNames[iSliceOriginIndex].c_str());
			FusionSurgery::ImageIOType::Pointer dicomIO = FusionSurgery::ImageIOType::New();
			reader->SetImageIO(dicomIO);
			reader->Update();
			FusionSurgery::ImageType::Pointer image = reader->GetOutput();

			//sliceOrigin[0] = pOrigins[iSliceOriginIndex * 3];
			//sliceOrigin[1] = pOrigins[iSliceOriginIndex * 3 + 1];
			//sliceOrigin[2] = pOrigins[iSliceOriginIndex * 3 + 2];


			// calculate the world coordinates z of the curve
			double worldZ;
			worldZ = (iImageSlices - iSliceOriginIndex - 1) * spacing[2] + worldOrigin[2]; // TODO PLE: check:previous calculation need a -1 to get the same slice
			
			// get curve
			inurbsPlanarCurve* pCurve = pCurveStack->GetCurve(worldZ);
			if (!pCurve) // curve does not exist
			{
				//curve = curveStack->CreateCurve(pt[2]);
				pCurve = pCurveStack->CreateCurve(worldZ);
			}
			else
			{
				// if there is already a curve for this roi in the slice and current curve is bigger, replace curve. If not, just continue.
				if (pMaxNumPoints[iSliceOriginIndex] < iNumPoints)
				{
					pCurveStack->RemoveCurve(pCurve); // remove smaller curve
					pCurve = pCurveStack->CreateCurve(worldZ); // recreate new curve
				}
				else // use bigger contour, ignore the smaller one
					continue;
			}


			double fPrevAddedPoint[2], fFirstAddedPoint[2];

			printf("Num points %d\n", iNumPoints);
			QList<inurbsPoint *> *tmpPoints = new QList<inurbsPoint *>;
			
			for (int k = 0; k < iNumPoints; k++)
			{
				ImageType::IndexType pixelIndex;
				PointType point;

				point[0] = pPts[k * 3];
				point[1] = pPts[k * 3 + 1];
				point[2] = pPts[k * 3 + 2];

				if (!(image->TransformPhysicalPointToIndex(point, pixelIndex)))
					continue;

				double tp[2];

				tp[0] = pixelIndex[0] * spacing[0] + worldOrigin[0];
				tp[1] = (iImageHeight - pixelIndex[1] - 1) * spacing[1] + worldOrigin[1];

				pPts[k * 3] = tp[0];
				pPts[k * 3 + 1] = tp[1];

			}

			for (int k=0; k < iNumPoints; k++)
			{
				//int pIndex[3];
				inurbsPoint* addPt;


				// get next point
				bool bCornerPoint = false;
				double prevPt[2];

				if (k == 0)
				{
					prevPt[0] = pPts[0];
					prevPt[1] = pPts[1];
				}

				// here we try to detect corner points where there is a bend whether in x or y direction. 
				// if so, we try to add the point with smaller minimum distance. 
				if (k != 0 && k != iNumPoints - 1) // not the first and last point
				{

					double dx1 = pPts[(k + 1) * 3] - pPts[k * 3]; // next - cur
					double dx2 = pPts[k * 3] - prevPt[0];          // cur - prev


					double dy1 = pPts[(k + 1) * 3 + 1] - pPts[k * 3 + 1];
					double dy2 = pPts[k * 3 + 1] - prevPt[1];

					if (!(fabs(dx1) < 0.00001) && !(fabs(dy1) < 0.00001)) // use prev pixel if the next pixel does not have the same x or y, do not 
					{
						prevPt[0] = pPts[k*3];
						prevPt[1] = pPts[k * 3 + 1];
					}


					if (dx1*dx2 < 0.0 ||   dy1*dy2 < 0.0)
						bCornerPoint = true;
				}

				/*// transform from contour points coordinates to image coordinates
				if (!(image->TransformPhysicalPointToIndex(point, pixelIndex)))
					continue;

				double tp[2];

				tp[0] = pixelIndex[0] * spacing[0] + worldOrigin[0];
				tp[1] = (iImageHeight - pixelIndex[1] - 1) * spacing[1] + worldOrigin[1];*/

				double tp[2];
				tp[0] = pPts[k * 3];
				tp[1] = pPts[k * 3 + 1];



				double fDistance,fDistanceWithFirstPoint;
				if (k > 0)
				{
					//fDistance = min((fabs(tp[0] - fPrevAddedPoint[0]) + fabs(tp[1] - fPrevAddedPoint[1])),
					//				 fabs(tp[0] - fFirstAddedPoint[0] + fabs(tp[1] - fFirstAddedPoint[1])));
					fDistanceWithFirstPoint = fabs(tp[0] - fFirstAddedPoint[0]) + fabs(tp[1] - fFirstAddedPoint[1]);
					fDistance = fabs(tp[0] - fPrevAddedPoint[0]) + fabs(tp[1] - fPrevAddedPoint[1]);
					if (fDistance > fDistanceWithFirstPoint)
						fDistance = fDistanceWithFirstPoint;
					if (bCornerPoint)
						fDistanceLimit = fDistanceLimit2;
					else
						fDistanceLimit = fDistanceLimit1;
					//if (fDistance > fDistanceLimit || bCornerPoint)
					if (fDistance > fDistanceLimit)
					{
						addPt = new inurbsPoint(tp[0], tp[1], worldZ);
						printf("add point %4.2f, %4.2f, %4.2f\n", tp[0], tp[1], worldZ);
						tmpPoints->append(addPt);

						fPrevAddedPoint[0] = tp[0];
						fPrevAddedPoint[1] = tp[1];
					}

				}
				else // first point
				{
					printf("add point %4.2f, %4.2f, %4.2f\n", tp[0], tp[1], worldZ);
					addPt = new inurbsPoint(tp[0], tp[1], worldZ);
					tmpPoints->append(addPt);
					
					fPrevAddedPoint[0] = tp[0];
					fPrevAddedPoint[1] = tp[1];

					fFirstAddedPoint[0] = tp[0];
					fFirstAddedPoint[1] = tp[1];
				}

			}

			printf("Num points to be added = %d\n", tmpPoints->count());
			pCurve->SetPoints(tmpPoints);
			qDeleteAll(tmpPoints->begin(), tmpPoints->end());
			tmpPoints->clear();
			delete tmpPoints;

			if (i != 0)
			{
				printf("slice %d, total num of points = %d, added points = %d\n", iSliceOriginIndex, iNumPoints, pCurve->GetNumOfPoints());
			}

			if (pMaxNumPoints[iSliceOriginIndex] < iNumPoints)
				pMaxNumPoints[iSliceOriginIndex] = iNumPoints;

		}

		// standardize start
		int iNumCreatedCurves = pCurveStack->GetNumOfCurves();
		for (int i = 0;i < iNumCreatedCurves;i++)
		{
			inurbsPlanarCurve *pCurve;

			pCurve = pCurveStack->GetCurve(i);
			if (pCurve)
			{
				int iNumPoints = pCurve->GetNumOfPoints();
				if (pCurve->GetNumOfPoints() < 3) // delete curve
				{
					pSubModel->RemoveCurve(pCurve);
				}
				else
				{
					pCurve->StandardizeStart(M_PI);
				}

			}
		}

		if (pSubModel->GetNumOfCurves() == 0)
			DeleteLesionSubModel(pSubModel);
		else
			pSubModel->BuildSurface();
	}

	// remove dir immediately after reading
	RemoveDir(strDest);
	delete[] pMaxNumPoints;
	//delete [] pOrigins;


	return true;
}
