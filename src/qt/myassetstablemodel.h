// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MYASSET_TABLEMODEL_H
#define BITCOIN_MYASSET_TABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QString>

#include <uint256.h>

class ClientModel;
class WalletModel;

struct MyAssetTableObject
{
    uint32_t nID;
    QString strTicker;
    int64_t nAssetAmount;
    QString strHeadline;
    uint256 payload;
    int nOutputN;
    int nControlN;
    int nConfirmations;
    int64_t nAssetAmountIn;
    uint256 creationTxid;
    uint256 outputTxid;
};

class MyAssetsTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MyAssetsTableModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    void setWalletModel(WalletModel *model);
    void setClientModel(ClientModel *model);

    enum RoleIndex {
        IDRole = Qt::UserRole,
        TickerRole,
        AmountRole,
        HeadlineRole,
        TxIDRole,
    };

public Q_SLOTS:
    void Update();

private:
    QList<QVariant> model;

    WalletModel *walletModel;
    ClientModel *clientModel;
};

#endif // BITCOIN_MYASSET_TABLE_MODEL_H
