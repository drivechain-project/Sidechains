// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HASHCALCDIALOG_H
#define HASHCALCDIALOG_H

#include <QDialog>

class PlatformStyle;

namespace Ui {
class HashCalcDialog;
}

class HashCalcDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HashCalcDialog(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~HashCalcDialog();

public Q_SLOTS:

    // Basic

    void on_plainTextEdit_textChanged();

    void on_pushButtonClear_clicked();
    void on_pushButtonPaste_clicked();
    void on_pushButtonHelp_clicked();
    void on_pushButtonHelpInvalidHex_clicked();
    void on_radioButtonHex_toggled(bool fChecked);
    void on_pushButtonFlip_clicked();

    // HMAC

    void on_plainTextEditHMAC_textChanged();
    void on_lineEditHMACKey_textChanged(QString);
    void on_pushButtonClearHMAC_clicked();
    void on_pushButtonHelpHMAC_clicked();
    void on_pushButtonHelpInvalidHexHMAC_clicked();
    void on_radioButtonHexHMAC_toggled(bool fChecked);

private:
    Ui::HashCalcDialog *ui;

    const PlatformStyle *platformStyle;

    void ShowInvalidHexWarning(bool fShow);
    void ClearOutput();
    void UpdateOutput();

    void ShowInvalidHexWarningHMAC(bool fShow);
    void ClearOutputHMAC();
    void UpdateOutputHMAC();
};

#endif // HASHCALCDIALOG_H
