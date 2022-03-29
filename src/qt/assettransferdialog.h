// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASSETTRANSFERDIALOG_H
#define ASSETTRANSFERDIALOG_H

#include <QDialog>
#include <QString>

#include <uint256.h>

namespace Ui {
class AssetTransferDialog;
}

class AssetTransferDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AssetTransferDialog(QWidget *parent = nullptr);
    ~AssetTransferDialog();

    void SetAsset(uint32_t nID, uint256 txid, int64_t nAssetAmount, QString strTicker, QString strHeadline);

public Q_SLOTS:
    void on_pushButtonTransfer_clicked();

private:
    Ui::AssetTransferDialog *ui;

    bool validateFeeAmount();

    uint32_t nID;
    uint256 txid;
    int64_t nAssetAmount;
    QString strTicker;
    QString strHeadline;
};

#endif // ASSETTRANSFERDIALOG_H
