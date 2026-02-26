#include <QDir>
#include <QFileInfoList>

#include "FusionSettingDialog.h"
#include "ui_FusionSettingDialog.h"
#include "QtGuiStyle.h"

FusionSettingDialog::FusionSettingDialog(QWidget *parent) :
    QDialog(parent),
        ui(new Ui::FusionSettingDialog)
{
    ui->setupUi(this);

	m_bCaseCreated = false;
	m_sPatientDataDrive = "";
}

FusionSettingDialog::~FusionSettingDialog()
{
    delete ui;
}

void FusionSettingDialog::Init()
{
	QtGuiStyle::SetComboBoxHeight(ui->comboBoxPatientDataDrive);

	QFileInfoList drivers = QDir::drives();
	int nCurrentDrive = 0;
	for(int k=0; k<drivers.count(); k++)
	{
		QString drive = drivers.at(k).absolutePath().left(2);
		ui->comboBoxPatientDataDrive->addItem(drive);
		if(m_sPatientDataDrive.left(2).toUpper() == drive.toUpper())
			nCurrentDrive = k;
	}
	ui->comboBoxPatientDataDrive->setCurrentIndex(nCurrentDrive);
	ui->comboBoxPatientDataDrive->setEnabled(!m_bCaseCreated);

	connect(ui->comboBoxPatientDataDrive, SIGNAL(currentIndexChanged(int)), this, SLOT(PatientDataDriveChanged(int)));

	m_bValueChanged = false;
	ui->ButtonApply->setEnabled(false);
}

void FusionSettingDialog::on_ButtonApply_clicked()
{
	QDialog::accept();
}

void FusionSettingDialog::on_ButtonCancel_clicked()
{
    QDialog::reject();
}

void FusionSettingDialog::PatientDataDriveChanged(int)
{
	m_sPatientDataDrive.replace(0, 2, ui->comboBoxPatientDataDrive->currentText());
	m_bValueChanged = true;
	ui->ButtonApply->setEnabled(true);
}
