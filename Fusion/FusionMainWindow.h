/******************************************************************************
    FusionMainWindow.h

    Date      : 26 Oct 2018
 ******************************************************************************/

#ifndef FUSION_MAIN_WINDOW_H
#define FUSION_MAIN_WINDOW_H

#include "BaseMainWindow.h"
#include <QToolButton>
#include <QStandardItemModel>
#include <itkGDCMImageIO.h>
#include <itkImageSeriesReader.h>
#include <QSplitter>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QPainter>

#include <QVTKOpenGLWidget.h>
#include <QVBoxLayout>
#include <QGridLayout>
#include <vtkSmartPointer.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageViewer2.h>
#include <vtkLookupTable.h>
#include <vtkDICOMMetaData.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleImage.h>
#include <vtkImageReslice.h>

#include "DicomDirImporter.h"
#include "ImageStack.h"
#include "ImageEntity.h"
#include "FusionSurgery.h"
#include "MouseInteractor.h"
#include "AxialMousInteractor.h"

class FusionVisualEngine;
class FusionSurgeryController;

class TransversalWidget : public QWidget
{
	Q_OBJECT

public:
	explicit TransversalWidget(QWidget* parent = nullptr) : QWidget(parent) {};
	int* selectedSeriesID = nullptr;
	QVector<int> m_selectedSeriesIndx;

protected:
	void mousePressEvent(QMouseEvent* event) override {
		QPoint clickPos = event->pos();
		QGridLayout* gridLayout = qobject_cast<QGridLayout*>(this->layout());
		if (!gridLayout)
			return;
		for (int i = 0; i < gridLayout->count(); ++i) {
			QLayoutItem* item = gridLayout->itemAt(i);
			if (item) {				
				QWidget* containerWidget = item->widget();
				if (containerWidget && containerWidget->geometry().contains(clickPos)) {
					if (i < m_selectedSeriesIndx.count())
					{
						int seriesIdx = m_selectedSeriesIndx[i];  // 获取系列索引
						*selectedSeriesID = seriesIdx;
					}					
					break;
				}
			}
		}
		QWidget::mouseMoveEvent(event);
	}

	void paintEvent(QPaintEvent* event) override
	{
		QPainter painter(this);
		painter.setBrush(Qt::black);  // 设置背景色为黑色
		painter.setPen(Qt::NoPen);    // 不绘制边框
		painter.drawRect(this->rect()); // 填充整个窗口
	}
};

class FusionMainWindow : public BaseMainWindow
{
	Q_OBJECT

public:

	// constructors and destructors
	explicit FusionMainWindow::FusionMainWindow(QWidget *parent = 0);
	~FusionMainWindow();

	static FusionMainWindow* GetInstance();
	static void DeleteInstance();

	// state function
	void GoBackToState(int state);

	// init functions
	void InitConnections();
	void InitWidgets();

	// lesions functions
	void UpdatePiradsControl();
	void UpdateRemarksControl();

	void PopupApplicationSetting();

	QStringList SelectFiles(QString &password);

	int m_currentSelectedSeriesId = 0;
protected:
	// resize functions
    void resizeEvent(QResizeEvent* event);

	// state functions
	void SetStageLayout(int state, int subState);
	void SetRightFrame(int state, int subState);
	void SetFunctionFrame(int state, int subState);
	void SetMessageLabel(int state, int subState);

	// start stage functions
	void UpdatePatientDetails();

	// graphic panel functions
	void SwitchViewTo(int iView);

	// lesion functions
	void SetLesionNavigationButtons();
	bool ChangeActiveLesionCheck();

	QString GetFusionTempDir();

	static FusionMainWindow* m_pInstance;

	QString m_sSelectDir;
	QString m_sTempDir;

	QString m_sDecryptDicomPassword;

	QStringList m_imageFiles;

public slots:
	void UpdateState(int state, int subState);
	void EnterLesionStage();

protected slots:
	// stage buttons clicked
	void on_stageStart_clicked();
	void on_stageImageImport_clicked();
	void on_stageModel_clicked();
	void on_stageModelLesion_clicked();
	void on_stageFinish_clicked();

	// start stage functions
	void on_startCreateCase_clicked();
	void on_startOpenCase_clicked();

	// import image stage functions
	void on_imageImportMRIImages_clicked();
	void on_imageImportSelectSeries_clicked();
	void on_imageImportFlipAntPos_clicked();
	void on_imageImportFlipApexBase_clicked();
	void on_imageImportResetImage_clicked();
	void on_imageImportApproveImage_clicked();

	// model stage functions
	void on_modelLoadRTStruct_clicked();
	void on_modelDeleteCurve_clicked();
	void on_modelResetModel_clicked();

	// lesion stage functions
	void on_lesionCreateLesion_clicked();
	void on_lesionDeleteLesion_clicked();
	void on_lesionPreviousLesion_clicked();
	void on_lesionNextLesion_clicked();
	void on_lesionDeleteContour_clicked();
	void on_lesionPirads_currentIndexChanged(int index);
	void on_lesionApproveLesionModels_clicked();

	// finish stage functions
	void on_finishExportCase_clicked();
	void on_finishExportAnonymousCase_clicked();

	// switch
	void LesionShowModelValueChanged(int value);

private:
	typedef short PixelType;
	typedef itk::Image<PixelType, 3> ImageType;
	typedef itk::GDCMImageIO ImageIOType;
	typedef itk::ImageSeriesReader< ImageType > ReaderType;

	enum FLIP { FLIP_NONE = 0, FLIP_X = 1, FLIP_Y = 2, FLIP_Z = 4 };

	DicomDirImporter*	m_pDicomDirImporter;
	QStandardItemModel*	m_pSeriesModel;

	QMap<int, QString> m_mapStudyID;
	QMap<int, QString> m_mapSeriesID;
	QMap<int, bool> m_multiFrameMap;
	QVector <QVector< QString >> m_seriesVec;
	QMap<int, ImageEntity> m_mapSeriesImageEntity;

	bool m_bIsDicomDir;
	int m_iDicomFlip;
	double m_fDirCosines[6];
	int m_iDicomWindowCenter, m_iDicomWindowWidth;

	bool m_bLoadImage;
	bool m_bMoreThanOneSeries;
	bool m_bIsMultiFrame;

	// Multi sequence widget handler
	TransversalWidget* m_ptrtransversalWidget; // display transversal view for multi sequence 
	QStackedWidget* m_pStackedWidget; // graphicPanelStackedWidget
	QWidget* m_pLayout2DView; // layout2DView
	QStackedWidget* m_pTransversal; // transversalImageContextStackedWidget
	QWidget* m_pSagCoronalWidget;
	QWidget* m_pSagitalWidget;
	QWidget* m_pCoronalWidget;

	// current selected series cor and sag viewer widget
	QWidget* m_pCurSagitalWidget;
	QWidget* m_pCurCoronalWidget;

	QStringList m_originalFiles;
	QStringList m_loadedFiles;
	QStringList m_selectedFiles;
	QVector<QStringList> m_selectedSeriesFiles;
	QVector<int> m_selectedSeriesIndx;

	QString m_strPatientName;
	QString m_strPatientID;
	QString m_strPatientBirthDate;

	void AnalyzeStudySeries(const QStringList& name);
	QString GetFirstDicomDir(QStringList files);

	int LoadSeries();
	QVector<QStringList> GetSeriesFiles(QVector<int> vecIndex);
	QStringList GetSeriesFiles(int nIndex);
	bool AnalyzeImageOrientation();
	void SetDICOMImage_LevelWindow(QVector <QVector< QString >> vec, int nIndex, int nCount);
	bool SetDICOMImage_Orientation(QVector <QVector< QString >> vec, int nIndex);
	void UploadImportedDicom();
	void UploadMultiSequenceDicom();
	void MultiSeqWidgetInit();
	void CreateVtkViewer(QWidget* parentWidget, int seriesIdx, ViewType orientation);
	void CreateSagCorViewer(int seriesIdx);
	void LoadDicomSequecne();
	void AdjustDisplayDirection(vtkSmartPointer<vtkImageData> imageData, double angleDegrees, vtkSmartPointer<vtkImageData>& outputImage, ViewType orientation);

public:
	void ResetDicomDirImporter();
	int Load(QStringList inputFiles);
	QString GetPatientName() { return m_strPatientName; }
	QString GetPatientID() { return m_strPatientID; }
	QString GetPatientBirthDate() { return m_strPatientBirthDate; }
	
	//coronal and sagittal views need to be dynamically updated based on the currently selected sequence
	vtkSmartPointer<vtkImageViewer2> m_currentSagittalViewer;
	vtkSmartPointer<vtkImageViewer2> m_currentCoronalViewer;
};


#endif