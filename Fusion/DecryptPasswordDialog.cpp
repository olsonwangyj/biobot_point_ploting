#include "DecryptPasswordDialog.h"
#include "ui_DecryptPasswordDialog.h"
#include "CommonClasses.h"

DecryptPasswordDialog::DecryptPasswordDialog(QWidget *parent, QString sPassword) :
    QDialog(parent),
    ui(new Ui::DecryptPasswordDialog)
{
    ui->setupUi(this);
	//ui->encryptPwd->setText(sPassword);

	connect(ui->cbShowPassword, SIGNAL(stateChanged(int)), this, SLOT(ShowPassword(int)));
}

DecryptPasswordDialog::~DecryptPasswordDialog()
{
    delete ui;
}

QString DecryptPasswordDialog::GetDecryptPassword()
{
	return ui->decryptPwd->text();
}

void DecryptPasswordDialog::on_btnOk_clicked()
{
//	QString sPassword = ui->decryptPwd->text();
//	QString sValid = CheckValidPassword(sPassword, m_iMinPasswordLength);
//	if(sValid.length() > 0)
//	{
//		ShowPasswordLabelText(sValid);
//		return;
//	}

	QDialog::accept();
}

void DecryptPasswordDialog::ShowPassword(int iState)
{
	if(iState == Qt::Checked)
		ui->decryptPwd->setEchoMode(QLineEdit::Normal);
	else
		ui->decryptPwd->setEchoMode(QLineEdit::Password);
}


void DecryptPasswordDialog::on_btnCancel_clicked()
{
    QDialog::reject();
}

//void DecryptPasswordDialog::ShowPasswordLabelText(QString sText)
//{
//	ui->pwdLabelText->setText(sText);
//	ui->pwdLabelText->setStyleSheet("background-color: rgb(204, 0, 0); color: rgb(255, 255, 255);");
//}

