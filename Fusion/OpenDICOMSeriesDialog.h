#ifndef OPENDICOMSERIESDIALOG_H
#define OPENDICOMSERIESDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
class OpenDICOMSeriesDialog;
}

class OpenDICOMSeriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenDICOMSeriesDialog(QStandardItemModel* pModel, QWidget *parent = 0);
    ~OpenDICOMSeriesDialog();
	int GetSelectedSeriesIndex() { return m_nRowSelected; }
    QVector<int> GetSelectedSeriesIndexList() { return m_vecSelectedRows; }

private slots:
    void on_OKButton_clicked();

    void on_CancelButton_clicked();

    void on_SeriesTableView_clicked(const QModelIndex &index);

private:
    Ui::OpenDICOMSeriesDialog *ui;
	QStandardItemModel* model;
    int m_nRowSelected;
    QVector<int> m_vecSelectedRows;
};

#endif // OPENDICOMSERIESDIALOG_H
