// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createassetpage.h>
#include <qt/forms/ui_createassetpage.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <wallet/wallet.h>

CreateAssetPage::CreateAssetPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CreateAssetPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

CreateAssetPage::~CreateAssetPage()
{
    delete ui;
}

void CreateAssetPage::setWalletModel(WalletModel *model)
{
    walletModel = model;
}

void CreateAssetPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model)
    {
        connect(model, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)),
                this, SLOT(update()));
    }
}

void CreateAssetPage::update()
{

}
