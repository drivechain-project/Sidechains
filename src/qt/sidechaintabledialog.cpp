#include "sidechaintabledialog.h"
#include "ui_sidechaintabledialog.h"

#include "sidechainescrowtablemodel.h"

SidechainTableDialog::SidechainTableDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainTableDialog)
{
    ui->setupUi(this);

    sidechainTableModel = new SidechainEscrowTableModel(this);

    ui->tableViewD1->setModel(sidechainTableModel);
    ui->tableViewD1->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

SidechainTableDialog::~SidechainTableDialog()
{
    delete ui;
}

void SidechainTableDialog::on_pushButtonRefresh_clicked()
{
    sidechainTableModel->updateModel();
}

void SidechainTableDialog::on_pushButtonClose_clicked()
{
    this->close();
}

void SidechainTableDialog::on_pushButtonRunSimulation_clicked()
{
    // TODO
    // Only enable this button in regest mode
}

void SidechainTableDialog::on_pushButtonReset_clicked()
{
    // TODO
    // begin model reset
    // end model reset
}
