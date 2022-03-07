// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ASSET_TABLEMODEL_H
#define BITCOIN_ASSET_TABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QString>

#include <uint256.h>

class ClientModel;
class WalletModel;

struct AssetTableObject
{
    uint32_t nID;
    QString strTicker;
    int64_t nAssetSupply;
    QString strHeadline;
    uint256 payload;
    uint256 creationTxid;
    QString strController;
    QString strOwner;
};

class AssetTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit AssetTableModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    void setWalletModel(WalletModel *model);
    void setClientModel(ClientModel *model);

public Q_SLOTS:
    void Update();

private:
    QList<QVariant> model;

    WalletModel *walletModel;
    ClientModel *clientModel;
};

#endif // BITCOIN_ASSET_TABLE_MODEL_H
