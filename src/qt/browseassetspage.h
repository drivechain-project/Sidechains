// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BROWSEASSETSPAGE_H
#define BROWSEASSETSPAGE_H

#include <QWidget>

class PlatformStyle;
class ClientModel;

namespace Ui {
class BrowseAssetsPage;
}

class BrowseAssetsPage : public QWidget
{
    Q_OBJECT

public:
    explicit BrowseAssetsPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~BrowseAssetsPage();

    void setClientModel(ClientModel *model);

private:
    Ui::BrowseAssetsPage *ui;

    ClientModel *clientModel = nullptr;

    const PlatformStyle *platformStyle;

private Q_SLOTS:
    void update();
};

#endif // BROWSEASSETSPAGE_H
