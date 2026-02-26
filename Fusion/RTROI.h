/******************************************************************************
	RTROI.h

	Date      : 2 Mar 2022
 ******************************************************************************/

#ifndef RTROI_H
#define	RTROI_H

#include <QObject>

class RTContour
{
public:
	double* m_pPoints;
	int m_iNumPoints;
	QString m_strRefSOPInstanceUID;
};

class RTROI
{
public:
	RTROI();
	~RTROI();

	void AddContour(RTContour* pContour);
	void DeleteContour(RTContour* pContour);
	int GetNumContours();
	RTContour* GetContour(int iIndex);
	void SetName(const char* strName) { m_sName = strName; }

private:
	QList<RTContour *>* m_pContours;
	QString m_sName;

};

#endif