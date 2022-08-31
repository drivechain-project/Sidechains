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

std::string BinToHexStr(const std::string strBin)
{
    std::string strHex= "";
    for (size_t i = 0; (i + 4) < strBin.size(); i += 4) {
        std::string strBits = strBin.substr(i, 4);

        if (strBits == "0000")
            strHex += '0';
        else
        if (strBits == "0001")
            strHex += '1';
        else
        if (strBits == "0010")
            strHex += '2';
        else
        if (strBits == "0011")
            strHex += '3';
        else
        if (strBits == "0100")
            strHex += '4';
        else
        if (strBits == "0101")
            strHex += '5';
        else
        if (strBits == "0110")
            strHex += '6';
        else
        if (strBits == "0111")
            strHex += '7';
        else
        if (strBits == "1000")
            strHex += '8';
        else
        if (strBits == "1001")
            strHex += '9';
        else
        if (strBits == "1010")
            strHex += 'a';
        else
        if (strBits == "1011")
            strHex += 'b';
        else
        if (strBits == "1100")
            strHex += 'c';
        else
        if (strBits == "1101")
            strHex += 'd';
        else
        if (strBits == "1110")
            strHex += 'e';
        else
        if (strBits == "1111")
            strHex += 'f';
    }

    return strHex;
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
    ui->textBrowserOutput->setStyleSheet("background: rgb(0,0,0,0)");
    ui->textBrowserOutputHMAC->setStyleSheet("background: rgb(0,0,0,0)");

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
    ui->textBrowserOutput->clear();
}

void HashCalcDialog::UpdateOutput()
{
    ClearOutput();

    const std::string str = ui->plainTextEdit->toPlainText().toStdString();

    if (str.empty()) {
        ShowInvalidHexWarning(false);
        return;
    }

    bool fHexChecked = ui->radioButtonHex->isChecked();
    bool fHex = IsHex(str);

    if (fHexChecked && !fHex) {
        ShowInvalidHexWarning(true);
        return;
    } else if (fHexChecked && fHex) {
        ShowInvalidHexWarning(false);
    }

    // Create a byte vector of the hex values if input is hex
    std::vector<unsigned char> vBytes = ParseHex(str);

    // CHash256 (SHA256D)
    ui->textBrowserOutput->append("<b>SHA256D:</b>");
    if (fHexChecked) {
        uint256 hash = Hash(vBytes.begin(), vBytes.end());
        std::string strHex = hash.GetHex();

        ui->textBrowserOutput->append(QString::fromStdString(hash.ToString()) + "\n");
        ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex)) + "<br>");
    }
    else {
        std::vector<unsigned char> vch256D;
        vch256D.resize(CHash256::OUTPUT_SIZE);
        CHash256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256D[0]);
        std::string strHex = HexStr(vch256D.begin(), vch256D.end());

        ui->textBrowserOutput->append(QString::fromStdString(strHex) + "\n");
        ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex)) + "<br>");
    }

    // CHash160 (SHA256 + ripemd160)
    ui->textBrowserOutput->append("<b>Hash160 - RIPEMD160(SHA256):</b>");
    if (fHexChecked) {
        uint160 hash = Hash160(vBytes.begin(), vBytes.end());
        std::string strHex160 = hash.GetHex();

        ui->textBrowserOutput->append(QString::fromStdString(hash.ToString()) + "\n");
        ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex160)) + "<br>");
    }
    else {
        std::vector<unsigned char> vchHash160;
        vchHash160.resize(CHash160::OUTPUT_SIZE);
        CHash160().Write((unsigned char*)&str[0], str.size()).Finalize(&vchHash160[0]);
        std::string strHex = HexStr(vchHash160.begin(), vchHash160.end());

        ui->textBrowserOutput->append(QString::fromStdString(strHex) + "\n");
        ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex)) + "<br>");
    }

    // RIPEMD160
    std::vector<unsigned char> vch160;
    vch160.resize(CRIPEMD160::OUTPUT_SIZE);
    if (fHexChecked)
        CRIPEMD160().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch160[0]);
    else
        CRIPEMD160().Write((unsigned char*)&str[0], str.size()).Finalize(&vch160[0]);

    std::string strHex160 = HexStr(vch160.begin(), vch160.end());
    ui->textBrowserOutput->append("<b>RIPEMD160:</b>");
    ui->textBrowserOutput->append(QString::fromStdString(strHex160) + "\n");
    ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex160)) + "<br>");

    // SHA256
    std::vector<unsigned char> vch256;
    vch256.resize(CSHA256::OUTPUT_SIZE);
    if (fHexChecked)
        CSHA256().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch256[0]);
    else
        CSHA256().Write((unsigned char*)&str[0], str.size()).Finalize(&vch256[0]);

    std::string strHex256 = HexStr(vch256.begin(), vch256.end());
    ui->textBrowserOutput->append("<b>SHA256:</b>");
    ui->textBrowserOutput->append(QString::fromStdString(strHex256) + "\n");
    ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex256)) + "<br>");

    // SHA512
    std::vector<unsigned char> vch512;
    vch512.resize(CSHA512::OUTPUT_SIZE);
    if (fHexChecked)
        CSHA512().Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch512[0]);
    else
        CSHA512().Write((unsigned char*)&str[0], str.size()).Finalize(&vch512[0]);

    std::string strHex512 = HexStr(vch512.begin(), vch512.end());
    ui->textBrowserOutput->append("<b>SHA512:</b>");
    ui->textBrowserOutput->append(QString::fromStdString(strHex512) + "\n");
    ui->textBrowserOutput->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex512)) + "<br>");

    // Decode
    if (fHexChecked) {
        std::string strDecode;
        for (size_t i = 0; i < vBytes.size(); i++)
            strDecode += vBytes[i];
        ui->textBrowserOutput->append("<b>Decode:</b>");
        ui->textBrowserOutput->append(QString::fromStdString(strDecode) + "\n");
    } else {
        ui->textBrowserOutput->append("<b>Decode:</b>");
        ui->textBrowserOutput->append(QString::fromStdString(str) + "\n");
    }

    // Hex
    if (fHexChecked) {
        ui->textBrowserOutput->append("<b>Hex:</b>");
        ui->textBrowserOutput->append(QString::fromStdString(str) + "\n");
        ui->textBrowserOutput->append("<b>Bin:</b>");
        ui->textBrowserOutput->append(QString::fromStdString(HexToBinStr(str)) + "<br>");
    }
    else {
        std::string strHex = HexStr(str.begin(), str.end());
        ui->textBrowserOutput->append("<b>Hex:</b>");
        ui->textBrowserOutput->append(QString::fromStdString(strHex) + "\n");
        ui->textBrowserOutput->append("<b>Bin:</b>");
        ui->textBrowserOutput->append(QString::fromStdString(HexToBinStr(strHex)) + "<br>");
    }

    // Scroll to top
    ui->textBrowserOutput->verticalScrollBar()->setValue(ui->textBrowserOutput->verticalScrollBar()->minimum());
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
    ui->textBrowserOutputHMAC->clear();
}

void HashCalcDialog::UpdateOutputHMAC()
{
    ClearOutputHMAC();

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

    ui->textBrowserOutputHMAC->append("<b>HMAC-SHA256:</b>");
    ui->textBrowserOutputHMAC->append(QString::fromStdString(strHex256) + "\n");
    ui->textBrowserOutputHMAC->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex256)) + "<br>");

    // HMAC-SHA512
    std::vector<unsigned char> vch512;
    vch512.resize(CHMAC_SHA512::OUTPUT_SIZE);
    if (fHexChecked)
        CHMAC_SHA512((unsigned char*)&vKey[0], vKey.size()).Write((unsigned char*)&vBytes[0], vBytes.size()).Finalize(&vch512[0]);
    else
        CHMAC_SHA512((unsigned char*)&strKey[0], strKey.size()).Write((unsigned char*)&strData[0], strData.size()).Finalize(&vch512[0]);

    std::string strHex512 = HexStr(vch512.begin(), vch512.end());

    ui->textBrowserOutputHMAC->append("<b>HMAC-SHA512:</b>");
    ui->textBrowserOutputHMAC->append(QString::fromStdString(strHex512) + "\n");
    ui->textBrowserOutputHMAC->append("<font color=\"gray\" size=2px>" + QString::fromStdString(HexToBinStr(strHex512)) + "<br>");
}
