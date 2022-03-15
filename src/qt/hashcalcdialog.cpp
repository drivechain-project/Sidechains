// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/hashcalcdialog.h>
#include <qt/forms/ui_hashcalcdialog.h>

#include <qt/guiutil.h>
#include <qt/platformstyle.h>

#include <crypto/ripemd160.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <primitives/block.h>
#include <streams.h>
#include <uint256.h>
#include <utilstrencodings.h>

#include <QApplication>
#include <QClipboard>
#include <QScrollBar>

HashCalcDialog::HashCalcDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HashCalcDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    ui->plainTextEdit->clear();

    // Set icons
    ui->pushButtonClear->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->pushButtonPaste->setIcon(platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->pushButtonFlip->setIcon(platformStyle->SingleColorIcon(":/icons/flip"));

    ui->pushButtonHelp->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->pushButtonHelpInvalidHex->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));

    ui->pushButtonHexWarning->setIcon(platformStyle->SingleColorIcon(":/icons/warning"));

    // Make text browsers transparent
    ui->textBrowserHex->setStyleSheet("background: rgb(0,0,0,0)");

    ui->pushButtonFlip->setEnabled(false);
}

HashCalcDialog::~HashCalcDialog()
{
    delete ui;
}

void HashCalcDialog::on_plainTextEdit_textChanged()
{
    UpdateOutput();
}

void HashCalcDialog::on_pushButtonClear_clicked()
{
    ui->plainTextEdit->clear();
}

void HashCalcDialog::on_pushButtonPaste_clicked()
{
    ui->plainTextEdit->clear();
    ui->plainTextEdit->insertPlainText(QApplication::clipboard()->text());
}

void HashCalcDialog::on_pushButtonHelp_clicked()
{
    QMessageBox::information(this, tr("DriveChain - information"),
        tr(""
           "Hex:\n"
           "The hexadecimal (base 16) representation.\n\n"
           "SHA-256:\n"
           "256 bit output from the Secure Hash Algorithm 2 hash function.\n\n"
           "SHA-256D:\n"
           "256 bit output from Bitcoin's SHA-256D / Hash256 [sha256(sha256())] hash function.\n"
           "Note that Bitcoin Core will output in Little-Endian byte order.\n\n"
           "RIPEMD160:\n"
           "160 bit RIPE Message Digest.\n\n"
           "Hash160:\n"
           "160 bit output from Bitcoin's Hash160 [RIPEMD160(sha256())] hash function.\n"
           "Note that Bitcoin Core will output in Little-Endian byte order.\n\n"
           ),
        QMessageBox::Ok);
}

void HashCalcDialog::on_pushButtonHelpInvalidHex_clicked()
{
    QMessageBox::information(this, tr("DriveChain - information"),
        tr("Please enter valid Hex without spaces or 0x."),
        QMessageBox::Ok);
}

void HashCalcDialog::on_radioButtonHex_toggled(bool fChecked)
{
    const std::string str = ui->plainTextEdit->toPlainText().toStdString();

    bool fHex = IsHex(str);

    if (fChecked && !fHex) {
        ClearOutput();
        ShowInvalidHexWarning(true);
    } else {
        ShowInvalidHexWarning(false);
        UpdateOutput();
    }

    ui->pushButtonFlip->setEnabled(fChecked);
}

void HashCalcDialog::ShowInvalidHexWarning(bool fShow)
{
    ui->pushButtonHexWarning->setVisible(fShow);
    ui->pushButtonHelpInvalidHex->setVisible(fShow);
    ui->labelInvalidHex->setVisible(fShow);
}

void HashCalcDialog::ClearOutput()
{
    ui->labelSHA256D->clear();
    ui->labelHash160->clear();
    ui->labelRIPEMD160->clear();
    ui->labelSHA256->clear();
    ui->textBrowserHex->clear();
}

void HashCalcDialog::UpdateOutput()
{
    const std::string str = ui->plainTextEdit->toPlainText().toStdString();

    if (str.empty()) {
        ClearOutput();
        ShowInvalidHexWarning(false);
        return;
    }

    bool fHexChecked = ui->radioButtonHex->isChecked();
    bool fHex = IsHex(str);

    if (fHexChecked && !fHex) {
        ShowInvalidHexWarning(true);
        ClearOutput();
        return;
    } else if (fHexChecked && fHex) {
        ShowInvalidHexWarning(false);
    }

    // Create a byte vector of the hex values if input is hex
    std::vector<unsigned char> vBytes = ParseHex(str);

    // CHash256 (SHA256D)
    if (fHexChecked) {
        uint256 hash = Hash(vBytes.begin(), vBytes.end());
        ui->labelSHA256D->setText(QString::fromStdString(hash.ToString()));
    }
    else {
        std::vector<unsigned char> vch256D;
        vch256D.resize(CHash256::OUTPUT_SIZE);
        CHash256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256D[0]);
        ui->labelSHA256D->setText(QString::fromStdString(HexStr(vch256D.begin(), vch256D.end())));
    }

    // CHash160 (SHA256 + ripemd160)
    if (fHexChecked) {
        uint160 hash = Hash160(vBytes.begin(), vBytes.end());
        ui->labelHash160->setText(QString::fromStdString(hash.ToString()));
    }
    else {
        std::vector<unsigned char> vchHash160;
        vchHash160.resize(CHash160::OUTPUT_SIZE);
        CHash160().Write((unsigned char*)&str[0], str.size()).Finalize(&vchHash160[0]);
        ui->labelHash160->setText(QString::fromStdString(HexStr(vchHash160.begin(), vchHash160.end())));
    }

    // RIPEMD160
    std::vector<unsigned char> vch160;
    vch160.resize(CRIPEMD160::OUTPUT_SIZE);
    if (fHexChecked)
        CRIPEMD160().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch160[0]);
    else
        CRIPEMD160().Write((unsigned char*)&str[0], str.size()).Finalize(&vch160[0]);

    ui->labelRIPEMD160->setText(QString::fromStdString(HexStr(vch160.begin(), vch160.end())));

    // SHA256
    std::vector<unsigned char> vch256;
    vch256.resize(CSHA256::OUTPUT_SIZE);
    if (fHexChecked)
        CSHA256().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch256[0]);
    else
        CSHA256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256[0]);

    ui->labelSHA256->setText(QString::fromStdString(HexStr(vch256.begin(), vch256.end())));

    // Hex
    std::string strHex = HexStr(str.begin(), str.end());
    if (fHexChecked) {
        ui->textBrowserHex->setText(QString::fromStdString(str));
    }
    else {
        ui->textBrowserHex->setText(QString::fromStdString(strHex));
    }

    // Scroll to bottom
    ui->textBrowserHex->verticalScrollBar()->setValue(ui->textBrowserHex->verticalScrollBar()->maximum());
}

void HashCalcDialog::on_pushButtonFlip_clicked()
{
    const std::string str = ui->plainTextEdit->toPlainText().toStdString();

    if (str.empty())
        return;

    std::vector<unsigned char> vBytes = ParseHex(str);

    if (vBytes.empty())
        return;

    std::reverse(vBytes.begin(), vBytes.end());

    ui->plainTextEdit->clear();
    ui->plainTextEdit->insertPlainText(QString::fromStdString(HexStr(vBytes.begin(), vBytes.end())));
}
