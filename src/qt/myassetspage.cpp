// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/myassetspage.h>
#include <qt/forms/ui_myassetspage.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <wallet/wallet.h>

MyAssetsPage::MyAssetsPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MyAssetsPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

MyAssetsPage::~MyAssetsPage()
{
    delete ui;
}

void MyAssetsPage::setWalletModel(WalletModel *model)
{
    walletModel = model;
}

void MyAssetsPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model)
    {
        connect(model, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)),
                this, SLOT(update()));
    }
}

void MyAssetsPage::update()
{

}
