// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createassetpage.h>
#include <qt/forms/ui_createassetpage.h>

#include <base58.h>
#include <wallet/wallet.h>

#include <qt/amountfield.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <QMessageBox>

CreateAssetPage::CreateAssetPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CreateAssetPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

CreateAssetPage::~CreateAssetPage()
{
    delete ui;
}

void CreateAssetPage::setWalletModel(WalletModel *model)
{
    walletModel = model;
}

void CreateAssetPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
}

void CreateAssetPage::on_pushButtonCreate_clicked()
{
    QMessageBox messageBox;
    messageBox.setDefaultButton(QMessageBox::Ok);

    if (vpwallets.empty()) {
        // No active wallet message box
        messageBox.setWindowTitle("No active wallet found!");
        messageBox.setText("You must have an active wallet to create BitAssets.");
        messageBox.exec();
        return;
    }

    if (vpwallets[0]->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked.");
        messageBox.exec();
        return;
    }

    // Check fee amount
    if (!validateFeeAmount()) {
        // Invalid fee amount message box
        messageBox.setWindowTitle("Invalid fee amount!");
        messageBox.setText("Check the amount you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Check owner destination
    std::string strOwner = ui->lineEditOwner->text().toStdString();
    CTxDestination destOwner = DecodeDestination(strOwner);
    if (!IsValidDestination(destOwner)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid owner destination!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Check controller destination
    std::string strController = ui->lineEditController->text().toStdString();
    CTxDestination destController = DecodeDestination(strController);
    if (!IsValidDestination(destController)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid controller destination!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

    if (ui->lineEditTicker->text().isEmpty()) {
        messageBox.setWindowTitle("Missing ticker!");
        messageBox.setText("Please add a ticker and try again.");
        messageBox.exec();
        return;
    }
    if (ui->lineEditHeader->text().isEmpty()) {
        messageBox.setWindowTitle("Missing tagline!");
        messageBox.setText("Please add a tagline and try again.");
        messageBox.exec();
        return;
    }
    if (ui->lineEditHash->text().isEmpty()) {
        messageBox.setWindowTitle("Missing payload hash!");
        messageBox.setText("Please enter a payload hash and try again.");
        messageBox.exec();
        return;
    }

    CAmount feeAmount = ui->feeAmount->value();


    CTransactionRef tx;
    std::string strFail = "";
    std::string ticker = ui->lineEditTicker->text().toStdString();
    std::string headline = ui->lineEditTicker->text().toStdString();
    uint256 payload = uint256S(ui->lineEditHash->text().toStdString());
    int64_t nSupply = ui->spinBoxSupply->value();

    {
        LOCK(vpwallets[0]->cs_wallet);
        if (!vpwallets[0]->CreateAsset(tx, strFail, ticker, headline, payload, feeAmount, nSupply, strController, strOwner))
        {
            messageBox.setWindowTitle("Missing payload hash!");
            messageBox.setText("Please enter a payload hash and try again.");
            messageBox.exec();
            return;
        }
    }

    messageBox.setWindowTitle("BitAsset created!");
    QString strResult = "TxID:\n";
    strResult += QString::fromStdString(tx->GetHash().ToString());
    messageBox.setText(strResult);
    messageBox.exec();
}

void CreateAssetPage::on_pushButtonFile_clicked()
{

}

bool CreateAssetPage::validateFeeAmount()
{
    if (!ui->feeAmount->validate()) {
        ui->feeAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->feeAmount->value(0) <= 0) {
        ui->feeAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->lineEditOwner->text(), ui->feeAmount->value())) {
        ui->feeAmount->setValid(false);
        return false;
    }

    return true;
}
