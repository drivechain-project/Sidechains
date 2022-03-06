// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/browseassetspage.h>
#include <qt/forms/ui_browseassetspage.h>

#include <qt/clientmodel.h>

#include <wallet/wallet.h>



BrowseAssetsPage::BrowseAssetsPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BrowseAssetsPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
}

BrowseAssetsPage::~BrowseAssetsPage()
{
    delete ui;
}

void BrowseAssetsPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model)
    {
        connect(model, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)),
                this, SLOT(update()));
    }
}

void BrowseAssetsPage::update()
{

}
