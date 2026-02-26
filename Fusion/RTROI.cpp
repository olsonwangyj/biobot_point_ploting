/******************************************************************************
	RTROI.cpp

	Date      : 2 Mar 2022
 ******************************************************************************/

#include "RTROI.h"


/******************************************************************************/
/* Constructos and Destructors
/******************************************************************************/

RTROI::RTROI()
{
	m_pContours = new QList<RTContour *>();
}

RTROI::~RTROI()
{
	// destruct rois
	if (m_pContours)
	{
		int numROIs = m_pContours->size();
		for (int i = 0; i < numROIs; i++)
		{
			RTContour* pContour = m_pContours->at(i);
			DeleteContour(pContour);
		}
		qDeleteAll(m_pContours->begin(), m_pContours->end());
		m_pContours->clear();
	}
}

/******************************************************************************/
/* Contour functions
/******************************************************************************/

void RTROI::AddContour(RTContour* pContour)
{
	m_pContours->append(pContour);
}

void RTROI::DeleteContour(RTContour* pContour)
{
	if (pContour)
	{
		delete[] pContour->m_pPoints;
		pContour->m_iNumPoints = 0;
		pContour->m_strRefSOPInstanceUID = "";
	}

}

int RTROI::GetNumContours()
{
	if (m_pContours)
		return m_pContours->size();
	return 0;
}

RTContour* RTROI::GetContour(int iIndex)
{
	if (m_pContours && iIndex >= GetNumContours())
		return NULL;
	return m_pContours->at(iIndex);
}


