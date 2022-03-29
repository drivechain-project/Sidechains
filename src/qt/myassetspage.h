// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYASSETSPAGE_H
#define MYASSETSPAGE_H

#include <QWidget>

class PlatformStyle;
class ClientModel;
class WalletModel;
class MyAssetsTableModel;

namespace Ui {
class MyAssetsPage;
}

QT_BEGIN_NAMESPACE
class QMenu;
class QModelIndex;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class MyAssetsPage : public QWidget
{
    Q_OBJECT

public:
    explicit MyAssetsPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~MyAssetsPage();

    void setWalletModel(WalletModel *model);
    void setClientModel(ClientModel *model);

private Q_SLOTS:
    void on_tableViewMyAssets_doubleClicked(const QModelIndex& index);
    void contextualMenu(const QPoint &);
    void showTransferDialog();

private:
    Ui::MyAssetsPage *ui;

    WalletModel *walletModel = nullptr;
    ClientModel *clientModel = nullptr;

    const PlatformStyle *platformStyle;

    MyAssetsTableModel *tableModel = nullptr;
    QSortFilterProxyModel *proxyModel = nullptr;

    QMenu *contextMenu = nullptr;

};

#endif // MYASSETSPAGE_H
