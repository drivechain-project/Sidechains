// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BROWSEASSETSPAGE_H
#define BROWSEASSETSPAGE_H

#include <QWidget>

class AssetTableModel;
class ClientModel;
class PlatformStyle;

namespace Ui {
class BrowseAssetsPage;
}

QT_BEGIN_NAMESPACE
class QSortFilterProxyModel;
QT_END_NAMESPACE

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

    AssetTableModel *tableModel = nullptr;
    QSortFilterProxyModel *proxyModel = nullptr;
};

#endif // BROWSEASSETSPAGE_H
