#include <QFileInfo>
#include "DicomDirImporter.h"

DicomDirImporter::DicomDirImporter(void)
{
}


DicomDirImporter::~DicomDirImporter(void)
{
}

QString DicomDirImporter::Getabsolutefilepath(QString dcmdirpath, QString dcmfilepath)
{
	QString dirpath = dcmdirpath.left(dcmdirpath.length() - 8);
	QString filepath = dirpath.append(dcmfilepath.trimmed());
	QFileInfo fpath(filepath);
	return fpath.absoluteFilePath();
}

bool DicomDirImporter::DicomDirParser(QString dicodirpath)
{
	DicomDirInfo ddInfo;
	QVector<QString> Studylist;
	QVector<QString> Serieslist;
	QVector<QString> Imagelist;
	ddInfo.filespath.clear();
	int count = 0;
	bool isContainImage = false;
	std::stringstream strm;
	
	gdcm::Reader reader;
	reader.SetFileName( dicodirpath.toStdString().c_str());
	if( !reader.Read() )
	{
		return false;
	}
	
	gdcm::File &file = reader.GetFile();
	gdcm::DataSet &ds = file.GetDataSet();
	gdcm::FileMetaInformation &fmi = file.GetHeader();

	gdcm::MediaStorage ms;
	ms.SetFromFile(file);
	if( ms != gdcm::MediaStorage::MediaStorageDirectoryStorage )
	{
		return false;
	}
	if (fmi.FindDataElement( gdcm::Tag (0x0002, 0x0002)))
    {   
		strm.str("");
		fmi.GetDataElement( gdcm::Tag (0x0002, 0x0002) ).GetValue().Print(strm);
    }
	else
    {
		return false;
    }
	if ("1.2.840.10008.1.3.10"!=strm.str())
    {
		return false;
    }

	ConstIterator it = ds.GetDES().begin();
	for( ; it != ds.GetDES().end(); ++it)
	{
		if (it->GetTag()==gdcm::Tag (0x0004, 0x1220))
		{
			const gdcm::DataElement &de = (*it);
			// ne pas utiliser GetSequenceOfItems pour extraire les items
			gdcm::SmartPointer<gdcm::SequenceOfItems> sqi =de.GetValueAsSQ();
			unsigned int itemused = 1;
			while (itemused <= sqi->GetNumberOfItems())
			{
				strm.str("");

				if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1430)))
					sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1430)).GetValue().Print(strm);

				while((strm.str()=="PATIENT")||((strm.str()=="PATIENT ")))
				{
					/////////////
					//Patient Name
					strm.str("");
					if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0010, 0x0010)))
					{
						if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0010, 0x0010)).IsEmpty())
							sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0010, 0x0010)).GetValue().Print(strm);
					}
					ddInfo.PatientName = QString::fromStdString(strm.str());
					/////////////
					itemused++;
					strm.str("");
					if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1430)))
						sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1430)).GetValue().Print(strm);

					while((strm.str()=="STUDY")||((strm.str()=="STUDY ")))
					{
						////////////
						//UID
						strm.str("");
						if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0020, 0x000d)))
						{
							if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0020, 0x000d)).IsEmpty())
								sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0020, 0x000d)).GetValue().Print(strm);
						} 
						QString studyUId = QString::fromStdString(strm.str());
						//STUDY DATE
						strm.str("");
						if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0008, 0x0020)))
						{
							if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x0020)).IsEmpty())
								sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x0020)).GetValue().Print(strm);
						}
						ddInfo.StudyDate = QString::fromStdString(strm.str());
						//STUDY DESCRIPTION
						strm.str("");
						if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0008, 0x1030)))
						{
							if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x1030)).IsEmpty())
								sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x1030)).GetValue().Print(strm);
						}
						ddInfo.StudyDescription = QString::fromStdString(strm.str());
						////////////
						itemused++;
						strm.str("");
						if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1430)))
						  sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1430)).GetValue().Print(strm);

						while((strm.str()=="SERIES")||((strm.str()=="SERIES ")))
						{
							///////////////////////////
							//SERIES UID
							if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0020, 0x000e)))
							sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0020, 0x000e)).GetValue().Print(strm);
							QString seriesUid = QString::fromStdString(strm.str());
							//SERIE MODALITY
							strm.str("");
							if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0008, 0x0060)))
							{
								if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x0060)).IsEmpty())
									sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x0060)).GetValue().Print(strm);
							}
							ddInfo.Modality = QString::fromStdString(strm.str());
							//SERIE DESCRIPTION
							strm.str("");
							if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0008, 0x103e)))
							{
								//bool len = sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x103e)).IsEmpty();
								if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x103e)).IsEmpty())
									sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0008, 0x103e)).GetValue().Print(strm);
							}
							ddInfo.SeriesDescription = QString::fromStdString(strm.str());
							//////////////////////////
							itemused++;
							strm.str("");
							if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1430)))
							sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1430)).GetValue().Print(strm);

							// If tag contains other than IMAGE, ignore it. 
							if((strm.str()!="IMAGE") && ((strm.str()!="IMAGE ")) )
							{
								if(itemused < sqi->GetNumberOfItems())
								{
									itemused++;
								}
								strm.str("");
								if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1430)))
								sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1430)).GetValue().Print(strm);
							}
							//

							 while ((strm.str()=="IMAGE")||((strm.str()=="IMAGE ")))
							 {
								isContainImage = true;
								strm.str("");
								if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1500)))
								{
									if(!sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1500)).IsEmpty())
										sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1500)).GetValue().Print(strm);
								}
								  
								QString imagePath = QString::fromStdString(strm.str());
								QString fileabsolutepath = Getabsolutefilepath(dicodirpath,imagePath);
								//filespath.append(fileabsolutepath);
								ddInfo.filespath.append(fileabsolutepath);
								/*ADD TAG TO READ HERE*/
								
								if(itemused < sqi->GetNumberOfItems())
								{
									itemused++;
								}
								else
								{
									break;
								}

								/*MapDicomDirInfo.insert(count, ddInfo);
								ddInfo.filespath.clear();*/
								strm.str("");

								if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0004, 0x1430)))
									sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0004, 0x1430)).GetValue().Print(strm);
							 }
							 //Image
							 if(isContainImage)
							 {
								 MapDicomDirInfo.insert(count, ddInfo);
								ddInfo.filespath.clear();
								count++;
								isContainImage = false;
							 }
							 
						}
						//Series
						
						//MapDicomDirInfo.insert(count, ddInfo);
						
					}
					//Study
				}
				//Patient

			itemused++;
			}
		}
	}
	return true;
}
