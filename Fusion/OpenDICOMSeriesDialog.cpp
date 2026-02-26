#include "OpenDICOMSeriesDialog.h"
#include "ui_OpenDICOMSeriesDialog.h"

OpenDICOMSeriesDialog::OpenDICOMSeriesDialog(QStandardItemModel* pModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenDICOMSeriesDialog)
{
    ui->setupUi(this);

	//QStandardItemModel* model = SurgeryController::getInstance()->GetSeriesModel();
	ui->SeriesTableView->setModel(pModel);
	ui->SeriesTableView->setColumnWidth(1,100);
	ui->SeriesTableView->setColumnWidth(2,160);
	ui->SeriesTableView->setColumnWidth(3,100);
	ui->SeriesTableView->setColumnWidth(4,65);
	ui->SeriesTableView->setColumnWidth(5,65);
	ui->SeriesTableView->setColumnWidth(6,65);
	

//	ui->SeriesTableView->resizeColumnsToContents();
	ui->SeriesTableView->horizontalHeader()->setStretchLastSection(true);
	ui->SeriesTableView->setStyleSheet("QHeaderView::section { background-color: transparent; }");

	m_nRowSelected = 0;
	m_vecSelectedRows.clear();
	// init checkbox first
	for (int row = 0; row < pModel->rowCount(); row++)
	{
		QStandardItem* checkBoxItem = pModel->item(row, 6);
		checkBoxItem->setCheckState(Qt::Unchecked);
	}

	for (int row = 0; row < pModel->rowCount(); row++)
	{
		QStandardItem* seriesDesc = pModel->item(row, 2);
		if (seriesDesc && seriesDesc->text().contains("T2", Qt::CaseInsensitive))
		{
			m_nRowSelected = row;			
			break;
		}
	}
	QStandardItem* checkBoxItem = pModel->item(m_nRowSelected, 6);
	checkBoxItem->setCheckState(Qt::Checked);
	ui->SeriesTableView->selectRow(m_nRowSelected);
}

OpenDICOMSeriesDialog::~OpenDICOMSeriesDialog()
{
    delete ui;
}

void OpenDICOMSeriesDialog::on_OKButton_clicked()
{
	QStandardItemModel* seriesTabelModel = (QStandardItemModel*)ui->SeriesTableView->model();
	for (int row = 0; row< seriesTabelModel->rowCount(); row++)
	{
		QStandardItem* checkBoxItem = seriesTabelModel->item(row, 6);		
		if (checkBoxItem && checkBoxItem->checkState() == Qt::Checked)  
		{
			m_vecSelectedRows.append(row);
		}		
	}
	m_nRowSelected = m_vecSelectedRows[0];
	QDialog::accept();
}

void OpenDICOMSeriesDialog::on_CancelButton_clicked()
{
	QDialog::reject();
}

void OpenDICOMSeriesDialog::on_SeriesTableView_clicked(const QModelIndex &index)
{	
	int selectedRow = index.row();
	QTableView* pTabelView = ui->SeriesTableView;	
	QStandardItemModel* seriesTabelModel = (QStandardItemModel*)ui->SeriesTableView->model();
	QStandardItem* checkBoxItem = seriesTabelModel->item(selectedRow, 6);
	if (checkBoxItem && checkBoxItem->checkState() == Qt::Checked)
	{
		pTabelView->selectionModel()->clear();
		QModelIndex index = seriesTabelModel->index(selectedRow, 0);
		QItemSelection selection(index, index);
		pTabelView->selectionModel()->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
	}
}