// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/assettablemodel.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <sidechain.h>
#include <txdb.h>
#include <validation.h>

Q_DECLARE_METATYPE(AssetTableObject);

AssetTableModel::AssetTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int AssetTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int AssetTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 8;
}

QVariant AssetTableModel::data(const QModelIndex &index, int role) const
{
    if (!walletModel)
        return false;

    if (!index.isValid())
        return false;

    int row = index.row();
    int col = index.column();

    // Double check that the data pointed at by the index still exists, it is
    // possible for a Withdrawalto be removed from the model when a block is connected.
    if (row >= model.size())
        return QVariant();

    if (!model.at(row).canConvert<AssetTableObject>())
        return QVariant();

    AssetTableObject object = model.at(row).value<AssetTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // BitAsset ID #
        if (col == 0) {
            return QString::number(object.nID);
        }
        // Ticker
        if (col == 1) {
            return object.strTicker;
        }
        // Supply
        if (col == 2) {
            return QString::number(object.nAssetSupply);
        }
        // Headline
        if (col == 3) {
            return object.strHeadline;
        }
        // Payload hash
        if (col == 4) {
            return QString::fromStdString(object.payload.ToString());
        }
        // Creation txid
        if (col == 5) {
            return QString::fromStdString(object.creationTxid.ToString());
        }
        // Controller
        if (col == 6) {
            return object.strController;
        }
        // Owner
        if (col == 7) {
            return object.strOwner;
        }

        break;
    }
    case Qt::TextAlignmentRole:
    {
        // BitAsset ID #
        if (col == 0) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Ticker
        if (col == 1) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Supply
        if (col == 2) {
            return int(Qt::AlignRight| Qt::AlignVCenter);
        }
        // Headline
        if (col == 3) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Payload hash
        if (col == 4) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Creation txid
        if (col == 5) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Controller
        if (col == 6) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Owner
        if (col == 7) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    }
    return QVariant();
}

QVariant AssetTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("ID");
            case 1:
                return QString("Ticker");
            case 2:
                return QString("Supply");
            case 3:
                return QString("Headline");
            case 4:
                return QString("Payload hash");
            case 5:
                return QString("Creation txid");
            case 6:
                return QString("Controller");
            case 7:
                return QString("Owner");
            }
        }
    }
    return QVariant();
}

void AssetTableModel::Update()
{
    beginResetModel();
    model.clear();
    endResetModel();

    std::vector<BitAsset> vAsset = passettree->GetAssets();

    beginInsertRows(QModelIndex(), 0, vAsset.size() - 1);
    for (const BitAsset& asset : vAsset) {
        AssetTableObject object;
        object.nID = asset.nID;
        object.strTicker = QString::fromStdString(asset.strTicker);
        object.strHeadline = QString::fromStdString(asset.strHeadline);
        object.payload = asset.payload;;
        object.creationTxid = asset.txid;
        object.nAssetSupply = asset.nSupply;
        object.strController = QString::fromStdString(asset.strController);
        object.strOwner = QString::fromStdString(asset.strOwner);

        model.append(QVariant::fromValue(object));
    }
    endInsertRows();
}

void AssetTableModel::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void AssetTableModel::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model)
    {
        connect(model, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)),
                this, SLOT(Update()));

        Update();
    }
}
