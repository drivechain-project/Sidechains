// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createassetpage.h>
#include <qt/forms/ui_createassetpage.h>

#include <base58.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <streams.h>
#include <uint256.h>
#include <utilstrencodings.h>
#include <wallet/wallet.h>

#include <qt/amountfield.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <QFile>
#include <QMessageBox>
#include <QTextStream>

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

    bool fImmutable = ui->checkBoxImmutable->isChecked();

    // Check controller destination
    std::string strController = ui->lineEditController->text().toStdString();
    CTxDestination destController = DecodeDestination(strController);
    if (!fImmutable && !IsValidDestination(destController)) {
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
            messageBox.setWindowTitle("Failed to create asset!");
            messageBox.setText("Error: " + QString::fromStdString(strFail));
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
    QString filename = GUIUtil::getOpenFileName(this, tr("Select file to hash"), "", "", nullptr);
    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Import Failed"),
            tr("File cannot be opened!\n"),
            QMessageBox::Ok);
        return;
    }

    // Read
    QTextStream in(&file);
    QString str = in.readAll();
    file.close();

    const std::string data = str.toStdString();

    // Create SHA256 hash
    std::vector<unsigned char> vch256;
    vch256.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&data[0], data.size()).Finalize(&vch256[0]);

    ui->labelPath->setText("path: " + filename);

    QString hash = QString::fromStdString(HexStr(vch256.begin(), vch256.end()));

    QString json;
    json += "{\n";
    json += "  \"BitAsset\":{\n";
    json += "    \"file\": \"" + filename + "\",\n";
    json += "    \"SHA256\": \"" + hash + "\"\n";
    json += "  }\n";
    json += "}";

    ui->plainTextEditDetails->appendPlainText(json);
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

void CreateAssetPage::on_checkBoxImmutable_toggled(bool fChecked)
{
    ui->lineEditController->setEnabled(!fChecked);
    if (fChecked) {
        ui->lineEditController->setText("Unspendable (OP_RETURN)");
    } else {
        ui->lineEditController->clear();
    }
}

void CreateAssetPage::on_plainTextEditDetails_textChanged()
{
    const std::string str = ui->plainTextEditDetails->toPlainText().toStdString();
    if (!str.size()) {
        ui->labelPath->setText("Path:");
        ui->lineEditHash->clear();
    }

    // TODO detect and warn / handle hex input?

    // Create SHA256 hash
    std::vector<unsigned char> vch256;
    vch256.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256[0]);

    ui->lineEditHash->setText(QString::fromStdString(HexStr(vch256.begin(), vch256.end())));
}
