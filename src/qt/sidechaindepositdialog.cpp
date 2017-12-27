// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindepositdialog.h"
#include "ui_sidechaindepositdialog.h"

#include "base58.h"
#include "bitcoinunits.h"
#include "chain.h"
#include "validation.h"
#include "guiutil.h"
#include "net.h"
#include "primitives/transaction.h"
#include "sidechain.h"
#include "txdb.h"
#include "wallet/wallet.h"

#include <QClipboard>
#include <QComboBox>
#include <QMessageBox>

SidechainDepositDialog::SidechainDepositDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SidechainDepositDialog)
{
    ui->setupUi(this);

    bool drivechainsEnabled = IsDrivechainEnabled(chainActive.Tip(), Params().GetConsensus());
    if (drivechainsEnabled) {
        for (const Sidechain& s : ValidSidechains) {
            ui->comboBoxSidechains->addItem(QString::fromStdString(s.GetSidechainName()));
        }
    } else {
        ui->pushButtonDeposit->setEnabled(false);
    }
}

SidechainDepositDialog::~SidechainDepositDialog()
{
    delete ui;
}

void SidechainDepositDialog::on_pushButtonDeposit_clicked()
{
    QMessageBox messageBox;

    if (pwalletMain->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked to create sidechain deposit.");
        messageBox.exec();
        return;
    }

    if (!validateDepositAmount()) {
        // Invalid deposit amount message box
        messageBox.setWindowTitle("Invalid deposit amount!");
        messageBox.setText("Check the amount you have entered and try again.");
        messageBox.exec();
        return;
    }

    unsigned int nSidechain = ui->comboBoxSidechains->currentIndex();

    if (!IsSidechainNumberValid(nSidechain)) {
        // Should never be displayed
        messageBox.setWindowTitle("Invalid sidechain selected");
        messageBox.exec();
        return;
    }

    // Get keyID
    CBitcoinAddress address(ui->payTo->text().toStdString());
    CKeyID keyID;
    if (!address.GetKeyID(keyID)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid Bitcoin address!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

#ifdef ENABLE_WALLET
    // Attempt to create the deposit
    const CAmount& nValue = ui->payAmount->value();
    CTransactionRef tx;
    std::string strFail = "";
    if (!pwalletMain->CreateSidechainDeposit(tx, strFail, nSidechain, nValue, keyID)) {
        // Create transaction error message box
        messageBox.setWindowTitle("Creating deposit transaction failed!");
        QString createError = "Error creating transaction!\n\n";
        createError += QString::fromStdString(strFail);
        messageBox.setText(createError);
        messageBox.exec();
        return;
    }

    // Successful deposit message box
    messageBox.setWindowTitle("Deposit transaction created!");
    QString result = "txid: " + QString::fromStdString(tx->GetHash().ToString());
    result += "\n";
    result += "Amount deposited: ";
    result += BitcoinUnits::formatWithUnit(BitcoinUnit::BTC, nValue, false, BitcoinUnits::separatorAlways);
    messageBox.setText(result);
    messageBox.exec();
#endif
}

void SidechainDepositDialog::on_pushButtonPaste_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SidechainDepositDialog::on_pushButtonClear_clicked()
{
    ui->payTo->clear();
}

bool SidechainDepositDialog::validateDepositAmount()
{
    if (!ui->payAmount->validate()) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        return false;
    }

    return true;
}
