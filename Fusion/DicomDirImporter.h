#ifndef DICOM_DIR_IMPORTER_H
#define DICOM_DIR_IMPORTER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap.h>
#include <QVector>
#include <gdcmReader.h>
#include <gdcmMediaStorage.h>

class DicomDirImporter
{
public:
	struct DicomDirInfo
	{
		QString PatientName;
		QString StudyDescription;
		QString StudyDate;
		QString SeriesDescription;
		QString Modality;
		QStringList filespath;
	};
public:
	DicomDirImporter(void);
	~DicomDirImporter(void);
	bool DicomDirParser(QString dicodirpath);
	QVector<QString> Patientlist;
	QVector<QVector<QString>> Finalimagelist;
	QVector<QVector<QString>> Finalserieslist;
	QVector<QVector<QString>> Finalstudylist;
	QMap <int, DicomDirInfo> MapDicomDirInfo;
	typedef std::set<gdcm::DataElement> DataElementSet;
	typedef DataElementSet::const_iterator ConstIterator;

private:
	QString Getabsolutefilepath(QString dcmdirpath, QString dcmfilepath);
};

#endif



