/******************************************************************************
	RTStruct.h

	Date      : 14 Jan 2022
 ******************************************************************************/

#ifndef RT_STRUCT_H
#define RT_STRUCT_H

#include <QObject>
class RTROI;

class RTStruct
{

public:
	RTStruct();
	~RTStruct();

	// ROI functions
	void AddROI(RTROI* pROI);
	void DeleteROI(RTROI* pROI);
	int GetNumROIs();
	RTROI* GetROI(int iIndex);


private:
	QList<RTROI *>* m_pROIs;
};

#endif

