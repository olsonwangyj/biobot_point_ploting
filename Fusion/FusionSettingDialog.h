#ifndef FUSIONSETTINGDIALOG_H
#define FUSIONSETTINGDIALOG_H

#include <QDialog>
#include <QComboBox>

namespace Ui {
    class FusionSettingDialog;
}

class FusionSettingDialog : public QDialog
{
    Q_OBJECT

public:

    explicit FusionSettingDialog(QWidget *parent = 0);
    ~FusionSettingDialog();

	void SetCaseCreated() { m_bCaseCreated = true; }

	void SetPatientDataDrive(QString sPatientDataDrive) { m_sPatientDataDrive = sPatientDataDrive; }
	QString GetPatientDataDrive() { return m_sPatientDataDrive; }

	void Init();
	bool IsValueChanged() { return m_bValueChanged; }

private slots:
	void on_ButtonApply_clicked();
	void on_ButtonCancel_clicked();
        
	void PatientDataDriveChanged(int);

private:
    Ui::FusionSettingDialog *ui;

	QString m_sPatientDataDrive;

	bool m_bValueChanged;
	bool m_bCaseCreated;	// if true, data folder cannot be changed
};

#endif
