// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYASSETSPAGE_H
#define MYASSETSPAGE_H

#include <QWidget>

class PlatformStyle;
class ClientModel;
class WalletModel;

namespace Ui {
class MyAssetsPage;
}

class MyAssetsPage : public QWidget
{
    Q_OBJECT

public:
    explicit MyAssetsPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~MyAssetsPage();

    void setWalletModel(WalletModel *model);
    void setClientModel(ClientModel *model);

public Q_SLOTS:
    void update();

private:
    Ui::MyAssetsPage *ui;

    WalletModel *walletModel = nullptr;
    ClientModel *clientModel = nullptr;

    const PlatformStyle *platformStyle;
};

#endif // MYASSETSPAGE_H
