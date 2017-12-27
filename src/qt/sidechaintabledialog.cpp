#include "sidechaintabledialog.h"
#include "ui_sidechaintabledialog.h"

#include "chain.h"
#include "chainparams.h"
#include "sidechainescrowtablemodel.h"
#include "validation.h"

SidechainTableDialog::SidechainTableDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainTableDialog)
{
    ui->setupUi(this);

    sidechainTableModel = new SidechainEscrowTableModel(this);

    ui->tableViewD1->setModel(sidechainTableModel);
    ui->tableViewD1->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    bool drivechainsEnabled = IsDrivechainEnabled(chainActive.Tip(), Params().GetConsensus());
    if (!drivechainsEnabled) {
        ui->pushButtonRefresh->setEnabled(false);
        ui->pushButtonReset->setEnabled(false);
        ui->pushButtonRunSimulation->setEnabled(false);
    }
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
