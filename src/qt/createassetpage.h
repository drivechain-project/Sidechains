// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CREATEASSETPAGE_H
#define CREATEASSETPAGE_H

#include <QWidget>

class PlatformStyle;
class ClientModel;
class WalletModel;

namespace Ui {
class CreateAssetPage;
}

class CreateAssetPage : public QWidget
{
    Q_OBJECT

public:
    explicit CreateAssetPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~CreateAssetPage();

    void setWalletModel(WalletModel *model);
    void setClientModel(ClientModel *model);

public Q_SLOTS:
    void update();

private:
    Ui::CreateAssetPage *ui;

    WalletModel *walletModel = nullptr;
    ClientModel *clientModel = nullptr;

    const PlatformStyle *platformStyle;
};

#endif // CREATEASSETPAGE_H
