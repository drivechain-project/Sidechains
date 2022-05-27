// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/assettransferdialog.h>
#include <qt/forms/ui_assettransferdialog.h>

#include <amount.h>
#include <base58.h>
#include <wallet/wallet.h>
#include <validation.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>

#include <QMessageBox>

AssetTransferDialog::AssetTransferDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AssetTransferDialog)
{
    ui->setupUi(this);
}

AssetTransferDialog::~AssetTransferDialog()
{
    delete ui;
}

void AssetTransferDialog::on_pushButtonTransfer_clicked()
{
    QMessageBox messageBox;
    messageBox.setDefaultButton(QMessageBox::Ok);

    if (vpwallets.empty()) {
        // No active wallet message box
        messageBox.setWindowTitle("No active wallet found!");
        messageBox.setText("You must have an active wallet to transfer assets.");
        messageBox.exec();
        return;
    }

    if (vpwallets[0]->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked to transfer assets.");
        messageBox.exec();
        return;
    }

    // Check fee amount
    if (!validateFeeAmount()) {
        // Invalid fee amount message box
        messageBox.setWindowTitle("Invalid fee amount!");
        messageBox.setText("Check the fee you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Check destination
    std::string strDest = ui->lineEditDest->text().toStdString();
    CTxDestination dest = DecodeDestination(strDest, false);
    if (!IsValidDestination(dest)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid destination!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

    CAmount amountFee = ui->amountFee->value();
    int64_t amount = ui->amount->value();
    QString strFee = BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, amountFee, false, BitcoinUnits::separatorAlways);

    if (amount > nAssetAmount) {
        // Amount too large message box
        messageBox.setWindowTitle("Insufficient Funds!");
        messageBox.setText("Cannot transfer more than " + QString::number(nAssetAmount) + "!");
        messageBox.exec();
        return;
    }

    // TODO confirmation dialog

    vpwallets[0]->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, vpwallets[0]->cs_wallet);

    uint256 txidOut;
    std::string strFail = "";
    if (!vpwallets[0]->TransferAsset(strFail, txidOut, txid, dest, amountFee, amount))
    {
        // Failed transfer message box
        messageBox.setWindowTitle("Transfer Failed!");
        messageBox.setText("Error: " + QString::fromStdString(strFail));
        messageBox.exec();
        return;
    }

    // Successful transfer message box
    messageBox.setWindowTitle("Transfer transaction created!");
    QString result = "txid: " + QString::fromStdString(txid.ToString());
    result += "\n";
    result += "Amount transfered: " + QString::number(amount);
    messageBox.setText(result);
    messageBox.exec();

    this->close();
}

void AssetTransferDialog::on_pushButtonMax_clicked()
{
    ui->amount->setValue(nAssetAmount);
}

bool AssetTransferDialog::validateFeeAmount()
{
    if (!ui->amountFee->validate()) {
        ui->amountFee->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->amountFee->value(0) <= 0) {
        ui->amountFee->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->lineEditDest->text(), ui->amountFee->value())) {
        ui->amountFee->setValid(false);
        return false;
    }

    return true;
}

void AssetTransferDialog::SetAsset(uint32_t nIDIn, uint256 txidIn, int64_t nAssetAmountIn, QString strTickerIn, QString strHeadlineIn)
{
    nID = nIDIn;
    txid = txidIn;
    nAssetAmount = nAssetAmountIn;
    strTicker = strTickerIn;
    strHeadline = strHeadlineIn;

    ui->labelID->setText(QString::number(nID));
    ui->labelTicker->setText(strTicker);
    ui->labelHeadline->setText(strHeadline);
    ui->labelBalance->setText(QString::number(nAssetAmountIn));
}
