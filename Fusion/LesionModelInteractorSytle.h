/******************************************************************************
    LesionModelInteractorStyle.h

    Date      : 26 Nov 2018
 ******************************************************************************/

#ifndef LESION_MODEL_INTERACTOR_STYLE
#define LESION_MODEL_INTERACTOR_STYLE

#include "CursorBoundResliceInteractorStyle.h"
#include "qelapsedtimer.h"

class inurbsPlanarCurve;
class inurbsSubModel;
class inurbsPoint;

class LesionModelInteractorStyle : public CursorBoundResliceInteractorStyle
{
	Q_OBJECT
public:

	static LesionModelInteractorStyle *New();

	LesionModelInteractorStyle();
	~LesionModelInteractorStyle();

	// mouse event handlers
	virtual void OnLeftButtonDown();
	virtual void OnLeftButtonUp();
	virtual void OnMouseMove();

	void SetActiveLesion(inurbsSubModel* pSubModel) {m_pActiveLesion = pSubModel;}
	inurbsSubModel* GetActiveLesion() { return m_pActiveLesion;}

	// other functions
	void StoreInitialPoints();

protected:

	// Move point functions
	void StartMovePoint();
	void EndMovePoint();
	void MovePoint();

	void StartDeletePoint();
	void EndDeletePoint();


	inurbsSubModel* m_pActiveLesion;
	int m_iPickedPointIndex;
	inurbsPlanarCurve* m_pCurCurve;

	int m_iNumInitialPoints;
	inurbsPoint* m_pInitialPoints;
	QElapsedTimer m_doubleClickTimer;

};

#endif