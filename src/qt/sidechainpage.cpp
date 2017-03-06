// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechainpage.h"
#include "ui_sidechainpage.h"

#include "base58.h"
#include "bitcoinunits.h"
#include "coins.h"
#include "consensus/validation.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "init.h"
#include "net.h"
#include "optionsmodel.h"
#include "sidechaindb.h"
#include "txdb.h"
#include "validation.h"
#include "wallet/wallet.h"
#include "walletmodel.h"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QStackedWidget>

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h" /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

SidechainPage::SidechainPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SidechainPage)
{
    ui->setupUi(this);

    // Initialize models and table views
    incomingTableView = new QTableView(this);
    outgoingTableView = new QTableView(this);
    incomingTableModel = new SidechainHistoryTableModel(this);
    outgoingTableModel = new SidechainHistoryTableModel(this);

    // Set table models
    incomingTableView->setModel(incomingTableModel);
    outgoingTableView->setModel(outgoingTableModel);

    // Table style
#if QT_VERSION < 0x050000
    incomingTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    incomingTableView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    incomingTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    incomingTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    outgoingTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Display tables
    ui->frameIncoming->layout()->addWidget(incomingTableView);
    ui->frameOutgoing->layout()->addWidget(outgoingTableView);

    generateAddress();
}

SidechainPage::~SidechainPage()
{
    delete ui;
}

void SidechainPage::generateQR(QString data)
{
    if (data.isEmpty())
        return;

    CBitcoinAddress address(data.toStdString());
    if (!address.IsValid())
        return;

#ifdef USE_QRCODE
    ui->QRCode->clear();

    QRcode *code = QRcode_encodeString(data.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);

    if (code) {
        QImage qr = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
        qr.fill(0xffffff);

        unsigned char *data = code->data;
        for (int y = 0; y < code->width; y++) {
            for (int x = 0; x < code->width; x++) {
                qr.setPixel(x + 4, y + 4, ((*data & 1) ? 0x0 : 0xffffff));
                data++;
            }
        }

        QRcode_free(code);
        ui->QRCode->setPixmap(QPixmap::fromImage(qr).scaled(200, 200));
    }
#endif
}

void SidechainPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if (model && model->getOptionsModel())
    {
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this,
                SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));
    }
}

void SidechainPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                               const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{  
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    const CAmount& pending = immatureBalance + unconfirmedBalance;
    ui->available->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    ui->pending->setText(BitcoinUnits::formatWithUnit(unit, pending, false, BitcoinUnits::separatorAlways));
}

void SidechainPage::on_pushButtonWithdraw_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageWithdraw);
}

void SidechainPage::on_pushButtonDeposit_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageDeposit);
}

void SidechainPage::on_pushButtonCopy_clicked()
{
    GUIUtil::setClipboard(ui->lineEditDepositAddress->text());
}

void SidechainPage::on_pushButtonNew_clicked()
{
    generateAddress();
}

void SidechainPage::on_pushButtonWT_clicked()
{
    QMessageBox messageBox;
    messageBox.setDefaultButton(QMessageBox::Ok);

    if (pwalletMain->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked to withdraw from sidechain.");
        messageBox.exec();
        return;
    }

    if (!validateWTAmount()) {
        // Invalid withdrawal amount message box
        messageBox.setWindowTitle("Invalid withdrawal amount!");
        messageBox.setText("Check the amount you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Get KeyID
    CKeyID keyID;
    CBitcoinAddress address(ui->payTo->text().toStdString());
    if (!address.GetKeyID(keyID)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid destination address!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Send payment to wt script
    std::vector<CRecipient> vecSend;
    CScript script = CScript() << OP_WT << ToByteVector(keyID.GetHex());
    const CAmount& withdrawAmt = ui->payAmount->value();
    CRecipient recipient = {script, withdrawAmt, false};
    vecSend.push_back(recipient);

    CWalletTx wtx;
    CReserveKey reserveKey(pwalletMain);
    CAmount nFee;
    int nChangePos = -1;
    std::string strError;
    if (!pwalletMain->CreateTransaction(vecSend, wtx, reserveKey, nFee, nChangePos, strError))
    {
        // Create transaction error message box
        messageBox.setWindowTitle("Creating withdraw transaction failed!");
        QString createError = "Error creating transaction: ";
        createError += QString::fromStdString(strError);
        createError += "\n";
        messageBox.setText(createError);
        messageBox.exec();
        return;
    }

    CValidationState state;
    if (!pwalletMain->CommitTransaction(wtx, reserveKey, g_connman.get(), state))
    {
        // Commit transaction error message box
        messageBox.setWindowTitle("Committing withdraw transaction failed!");
        QString commitError = "Error committing transaction: ";
        commitError += QString::fromStdString(state.GetRejectReason());
        commitError += "\n";
        messageBox.setText(commitError);
        messageBox.exec();
        return;
    }

    // Successful withdraw message box
    messageBox.setWindowTitle("Withdraw transaction created!");
    QString result = "txid: " + QString::fromStdString(wtx.GetHash().ToString());
    result += "\n";
    result += "Amount withdrawn: ";
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    result += BitcoinUnits::formatWithUnit(unit, withdrawAmt, false, BitcoinUnits::separatorAlways);
    messageBox.setText(result);
    messageBox.exec();
}

void SidechainPage::on_addressBookButton_clicked()
{

}

void SidechainPage::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SidechainPage::on_deleteButton_clicked()
{
    ui->payTo->clear();
}

bool SidechainPage::validateWTAmount()
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

void SidechainPage::generateAddress()
{
    // TODO don't generate address here
    LOCK2(cs_main, pwalletMain->cs_wallet);

    pwalletMain->TopUpKeyPool();

    CPubKey newKey;
    if (pwalletMain->GetKeyFromPool(newKey)) {
        CKeyID keyID = newKey.GetID();

        CBitcoinAddress address(keyID);
        generateQR(QString::fromStdString(address.ToString()));

        ui->lineEditDepositAddress->setText(QString::fromStdString(address.ToString()));
    }
}
