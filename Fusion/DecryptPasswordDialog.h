#ifndef DECRYPTPASSWORDDIALOG_H
#define DECRYPTPASSWORDDIALOG_H

#include <QDialog>

namespace Ui {
    class DecryptPasswordDialog;
}

class DecryptPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DecryptPasswordDialog(QWidget *parent, QString sPassword="");
    ~DecryptPasswordDialog();

	QString GetDecryptPassword();

private slots:

    void on_btnCancel_clicked();
    void on_btnOk_clicked();

	void ShowPassword(int iState);

private:
    Ui::DecryptPasswordDialog *ui;
//	void ShowPasswordLabelText(QString sText);
};

#endif