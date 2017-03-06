// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINPAGE_H
#define SIDECHAINPAGE_H

#include "sidechainhistorytablemodel.h"

#include "amount.h"

#include <QTableView>
#include <QWidget>

class WalletModel;

namespace Ui {
class SidechainPage;
}

class SidechainPage : public QWidget
{
    Q_OBJECT

public:
    explicit SidechainPage(QWidget *parent = 0);
    ~SidechainPage();

    void generateQR(QString data);

    void setWalletModel(WalletModel *model);

public Q_SLOTS:
    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                    const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                    const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

private Q_SLOTS:
    void on_pushButtonWithdraw_clicked();

    void on_pushButtonDeposit_clicked();

    void on_pushButtonCopy_clicked();

    void on_pushButtonNew_clicked();

    void on_pushButtonWT_clicked();

    void on_addressBookButton_clicked();

    void on_pasteButton_clicked();

    void on_deleteButton_clicked();

private:
    Ui::SidechainPage *ui;

    WalletModel *walletModel;

    QTableView *incomingTableView;
    QTableView *outgoingTableView;

    SidechainHistoryTableModel *incomingTableModel;
    SidechainHistoryTableModel *outgoingTableModel;

    bool validateWTAmount();

    void generateAddress();
};

#endif // SIDECHAINPAGE_H
