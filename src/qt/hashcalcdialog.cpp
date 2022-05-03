// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/hashcalcdialog.h>
#include <qt/forms/ui_hashcalcdialog.h>

#include <qt/guiutil.h>
#include <qt/platformstyle.h>

#include <crypto/ripemd160.h>
#include <crypto/sha256.h>
#include <crypto/hmac_sha256.h>
#include <crypto/hmac_sha512.h>
#include <hash.h>
#include <primitives/block.h>
#include <streams.h>
#include <uint256.h>
#include <utilstrencodings.h>

#include <QApplication>
#include <QClipboard>
#include <QScrollBar>

std::string HexToBinStr(const std::string strHex)
{
    if (!IsHex(strHex))
        return "";

    std::string strBin = "";
    for (const char& c : strHex) {
        switch (c) {
        case ('0'): strBin += "0000"; break;
        case ('1'): strBin += "0001"; break;
        case ('2'): strBin += "0010"; break;
        case ('3'): strBin += "0011"; break;
        case ('4'): strBin += "0100"; break;
        case ('5'): strBin += "0101"; break;
        case ('6'): strBin += "0110"; break;
        case ('7'): strBin += "0111"; break;
        case ('8'): strBin += "1000"; break;
        case ('9'): strBin += "1001"; break;
        case ('a'): strBin += "1010"; break;
        case ('b'): strBin += "1011"; break;
        case ('c'): strBin += "1100"; break;
        case ('d'): strBin += "1101"; break;
        case ('e'): strBin += "1110"; break;
        case ('f'): strBin += "1111"; break;
        }
    }
    return strBin;
}

HashCalcDialog::HashCalcDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HashCalcDialog),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    ui->plainTextEdit->clear();
    ui->plainTextEditHMAC->clear();

    // Set icons

    // Basic
    ui->pushButtonClear->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->pushButtonPaste->setIcon(platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->pushButtonFlip->setIcon(platformStyle->SingleColorIcon(":/icons/flip"));
    ui->pushButtonHelp->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->pushButtonHelpInvalidHex->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->pushButtonHexWarning->setIcon(platformStyle->SingleColorIcon(":/icons/warning"));

    // HMAC
    ui->pushButtonClearHMAC->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->pushButtonHelpHMAC->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->pushButtonHelpInvalidHexHMAC->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->pushButtonHexWarningHMAC->setIcon(platformStyle->SingleColorIcon(":/icons/warning"));

    // Make text browsers transparent
    ui->textBrowserHex->setStyleSheet("background: rgb(0,0,0,0)");
    ui->textBrowserDecode->setStyleSheet("background: rgb(0,0,0,0)");
    // Also set font to gray
    ui->textBrowserHexBIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserSHA256BIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserSHA512BIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserHash160BIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserHash256BIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserRIPEMDBIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserHMAC256BIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");
    ui->textBrowserHMAC512BIN->setStyleSheet("background: rgb(0,0,0,0); color: gray");

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
    QMessageBox::information(this, tr("Drivechain - information"),
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
    QMessageBox::information(this, tr("Drivechain - information"),
        tr("Please enter valid Hex without spaces or 0x prefix."),
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

    if (fChecked) {
        ui->plainTextEdit->setPlaceholderText("Enter Hex");
    } else {
        ui->plainTextEdit->setPlaceholderText("Enter plain text");
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
    ui->labelSHA512->clear();
    ui->textBrowserHex->clear();
    ui->textBrowserHexBIN->clear();
    ui->textBrowserDecode->clear();

    ui->textBrowserSHA256BIN->clear();
    ui->textBrowserSHA512BIN->clear();
    ui->textBrowserHash160BIN->clear();
    ui->textBrowserHash256BIN->clear();
    ui->textBrowserRIPEMDBIN->clear();
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
        ui->textBrowserHash256BIN->setText(QString::fromStdString(HexToBinStr(hash.ToString())));
    }
    else {
        std::vector<unsigned char> vch256D;
        vch256D.resize(CHash256::OUTPUT_SIZE);
        CHash256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256D[0]);
        std::string strHex = HexStr(vch256D.begin(), vch256D.end());
        ui->labelSHA256D->setText(QString::fromStdString(strHex));
        ui->textBrowserHash256BIN->setText(QString::fromStdString(HexToBinStr(strHex)));
    }

    // CHash160 (SHA256 + ripemd160)
    if (fHexChecked) {
        uint160 hash = Hash160(vBytes.begin(), vBytes.end());
        ui->labelHash160->setText(QString::fromStdString(hash.ToString()));
        ui->textBrowserHash160BIN->setText(QString::fromStdString(HexToBinStr(hash.ToString())));
    }
    else {
        std::vector<unsigned char> vchHash160;
        vchHash160.resize(CHash160::OUTPUT_SIZE);
        CHash160().Write((unsigned char*)&str[0], str.size()).Finalize(&vchHash160[0]);
        std::string strHex = HexStr(vchHash160.begin(), vchHash160.end());
        ui->labelHash160->setText(QString::fromStdString(strHex));
        ui->textBrowserHash160BIN->setText(QString::fromStdString(HexToBinStr(strHex)));
    }

    // RIPEMD160
    std::vector<unsigned char> vch160;
    vch160.resize(CRIPEMD160::OUTPUT_SIZE);
    if (fHexChecked)
        CRIPEMD160().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch160[0]);
    else
        CRIPEMD160().Write((unsigned char*)&str[0], str.size()).Finalize(&vch160[0]);

    std::string strHex160 = HexStr(vch160.begin(), vch160.end());
    ui->labelRIPEMD160->setText(QString::fromStdString(strHex160));
    ui->textBrowserRIPEMDBIN->setText(QString::fromStdString(HexToBinStr(strHex160)));

    // SHA256
    std::vector<unsigned char> vch256;
    vch256.resize(CSHA256::OUTPUT_SIZE);
    if (fHexChecked)
        CSHA256().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch256[0]);
    else
        CSHA256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256[0]);

    std::string strHex256 = HexStr(vch256.begin(), vch256.end());
    ui->labelSHA256->setText(QString::fromStdString(strHex256));
    ui->textBrowserSHA256BIN->setText(QString::fromStdString(HexToBinStr(strHex256)));

    // SHA512
    std::vector<unsigned char> vch512;
    vch512.resize(CSHA512::OUTPUT_SIZE);
    if (fHexChecked)
        CSHA512().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch512[0]);
    else
        CSHA512().Write((unsigned char*)&str[0], str.size()).Finalize(&vch512[0]);

    std::string strHex512 = HexStr(vch512.begin(), vch512.end());
    ui->labelSHA512->setText(QString::fromStdString(HexStr(vch512.begin(), vch512.end())));
    ui->textBrowserSHA512BIN->setText(QString::fromStdString(HexToBinStr(strHex512)));

    // Decode
    if (fHexChecked) {
        std::string strDecode;
        for (size_t i = 0; i < vBytes.size(); i++)
            strDecode += vBytes[i];
        ui->textBrowserDecode->setText(QString::fromStdString(strDecode));
    } else {
        ui->textBrowserDecode->setText(QString::fromStdString(str));
    }

    // Hex
    if (fHexChecked) {
        ui->textBrowserHex->setText(QString::fromStdString(str));
        ui->textBrowserHexBIN->setText(QString::fromStdString(HexToBinStr(str)));
    }
    else {
        std::string strHex = HexStr(str.begin(), str.end());
        ui->textBrowserHex->setText(QString::fromStdString(strHex));
        ui->textBrowserHexBIN->setText(QString::fromStdString(HexToBinStr(strHex)));
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

void HashCalcDialog::on_plainTextEditHMAC_textChanged()
{
    UpdateOutputHMAC();
}

void HashCalcDialog::on_lineEditHMACKey_textChanged(QString)
{
    UpdateOutputHMAC();
}

void HashCalcDialog::on_pushButtonClearHMAC_clicked()
{
    ui->lineEditHMACKey->clear();
    ui->plainTextEditHMAC->clear();

}

void HashCalcDialog::on_pushButtonHelpHMAC_clicked()
{
    QMessageBox::information(this, tr("Drivechain - information"),
        tr(""
           "HMAC: Keyed-Hashing for Message Authentication\n\n"
           "HMAC-SHA256:\n"
           "256 bit keyed-hash output using the Secure Hash Algorithm 2 hash function.\n\n"
           "HMAC-SHA512:\n"
           "512 bit keyed-hash output using the Secure Hash Algorithm 2 hash function."
           ),
        QMessageBox::Ok);
}

void HashCalcDialog::on_pushButtonHelpInvalidHexHMAC_clicked()
{
    QMessageBox::information(this, tr("Drivechain - information"),
        tr("Please enter valid Hex without spaces or 0x prefix."),
        QMessageBox::Ok);
}

void HashCalcDialog::on_radioButtonHexHMAC_toggled(bool fChecked)
{
    const std::string strData = ui->plainTextEditHMAC->toPlainText().toStdString();
    const std::string strKey = ui->lineEditHMACKey->text().toStdString();

    bool fHexKey = IsHex(strKey);
    bool fHexData = IsHex(strData);

    if (fChecked && (!fHexKey || !fHexData)) {
        ShowInvalidHexWarningHMAC(true);
        ClearOutputHMAC();
    }
    else {
        ShowInvalidHexWarningHMAC(false);
        UpdateOutputHMAC();
    }

    if (fChecked) {
        ui->plainTextEditHMAC->setPlaceholderText("Enter message Hex");
        ui->lineEditHMACKey->setPlaceholderText("Enter key Hex");
    } else {
        ui->plainTextEditHMAC->setPlaceholderText("Enter plain text");
        ui->lineEditHMACKey->setPlaceholderText("Enter key plain text");
    }
}

void HashCalcDialog::ShowInvalidHexWarningHMAC(bool fShow)
{
    ui->pushButtonHexWarningHMAC->setVisible(fShow);
    ui->pushButtonHelpInvalidHexHMAC->setVisible(fShow);
    ui->labelInvalidHexHMAC->setVisible(fShow);
}

void HashCalcDialog::ClearOutputHMAC()
{
    ui->labelHMACSHA256->clear();
    ui->labelHMACSHA512->clear();
    ui->textBrowserHMAC256BIN->clear();
    ui->textBrowserHMAC512BIN->clear();
}

void HashCalcDialog::UpdateOutputHMAC()
{
    const std::string strKey = ui->lineEditHMACKey->text().toStdString();
    const std::string strData = ui->plainTextEditHMAC->toPlainText().toStdString();

    if (strKey.empty()) {
        ClearOutputHMAC();
        ShowInvalidHexWarningHMAC(false);
        return;
    }
    if (strData.empty()) {
        ClearOutputHMAC();
        ShowInvalidHexWarningHMAC(false);
        return;
    }

    bool fHexChecked = ui->radioButtonHexHMAC->isChecked();
    bool fHexKey = IsHex(strData);
    bool fHexData = IsHex(strData);

    if (fHexChecked && (!fHexKey || !fHexData)) {
        ShowInvalidHexWarningHMAC(true);
        ClearOutputHMAC();
        return;
    }
    else if (fHexChecked && (fHexKey && fHexData)) {
        ShowInvalidHexWarningHMAC(false);
    }

    // Create a byte vector of the hex values
    std::vector<unsigned char> vKey = ParseHex(strKey);
    std::vector<unsigned char> vBytes = ParseHex(strData);

    // HMAC-SHA256
    std::vector<unsigned char> vch256;
    vch256.resize(CHMAC_SHA256::OUTPUT_SIZE);
    if (fHexChecked)
        CHMAC_SHA256((unsigned char*)&vKey[0], vKey.size()).Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch256[0]);
    else
        CHMAC_SHA256((unsigned char*)&strKey[0], strKey.size()).Write((unsigned char*)&strData[0], strData.size()).Finalize(&vch256[0]);

    std::string strHex256 = HexStr(vch256.begin(), vch256.end());
    ui->labelHMACSHA256->setText(QString::fromStdString(strHex256));
    ui->textBrowserHMAC256BIN->setText(QString::fromStdString(HexToBinStr(strHex256)));

    // HMAC-SHA512
    std::vector<unsigned char> vch512;
    vch512.resize(CHMAC_SHA512::OUTPUT_SIZE);
    if (fHexChecked)
        CHMAC_SHA512((unsigned char*)&vKey[0], vKey.size()).Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch512[0]);
    else
        CHMAC_SHA512((unsigned char*)&strKey[0], strKey.size()).Write((unsigned char*)&strData[0], strData.size()).Finalize(&vch512[0]);

    std::string strHex512 = HexStr(vch512.begin(), vch512.end());
    ui->labelHMACSHA512->setText(QString::fromStdString(strHex512));
    ui->textBrowserHMAC512BIN->setText(QString::fromStdString(HexToBinStr(strHex512)));
}
