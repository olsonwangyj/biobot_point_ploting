
/******************************************************************************
	RTStruct.cpp

	Date      : 14 Jan 2022
 ******************************************************************************/

#include "RTStruct.h"
#include "RTROI.h"

 /******************************************************************************/
 /* Constructos and Destructors
 /******************************************************************************/

RTStruct::RTStruct()
{
	m_pROIs = new QList<RTROI *>();
}

RTStruct::~RTStruct()
{
	// destruct rois
	if (m_pROIs)
	{
		int numROIs = m_pROIs->size();
		for (int i = 0; i < numROIs; i++)
		{
			RTROI* pROI = m_pROIs->at(i);
			DeleteROI(pROI);
		}
		qDeleteAll(m_pROIs->begin(), m_pROIs->end());
		m_pROIs->clear();
	}

	delete m_pROIs;
}

/******************************************************************************/
 /* ROI functions
 /******************************************************************************/

void RTStruct::AddROI(RTROI* pROI)
{
	m_pROIs->append(pROI);
}

void RTStruct::DeleteROI(RTROI* pROI)
{
	if (pROI)
	{
		//int numContours = pROI->m_contours.size();
		//for (int i = 0; i < numContours; i++)
		//{
		//	delete [] pROI->m_contours.at(i);
		//}

	}
}

int RTStruct::GetNumROIs()
{
	if (!m_pROIs)
		return 0;
	return m_pROIs->size();
}

RTROI* RTStruct::GetROI(int iIndex)
{
	if (iIndex >= GetNumROIs())
		return NULL;
	return m_pROIs->at(iIndex);
}