// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/myassetstablemodel.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <sidechain.h>
#include <txdb.h>
#include <validation.h>
#include <wallet/wallet.h>

Q_DECLARE_METATYPE(MyAssetTableObject);

MyAssetsTableModel::MyAssetsTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int MyAssetsTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int MyAssetsTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 11;
}

QVariant MyAssetsTableModel::data(const QModelIndex &index, int role) const
{
    if (!walletModel)
        return false;

    if (!index.isValid())
        return false;

    int row = index.row();
    int col = index.column();

    if (row >= model.size())
        return QVariant();

    if (!model.at(row).canConvert<MyAssetTableObject>())
        return QVariant();

    MyAssetTableObject object = model.at(row).value<MyAssetTableObject>();

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
        // Asset balance
        if (col == 2) {
            return QString::number(object.nAssetAmount);
        }
        // Headline
        if (col == 3) {
            return object.strHeadline;
        }
        // Payload hash
        if (col == 4) {
            return QString::fromStdString(object.payload.ToString());
        }
        // Output N
        if (col == 5) {
            return QString::number(object.nOutputN);
        }
        // Controller output N
        if (col == 6) {
            return QString::number(object.nControlN);
        }
        // Confirmations
        if (col == 7) {
            return QString::number(object.nConfirmations);
        }
        // Amount asset input
        if (col == 8) {
            return QString::number(object.nAssetAmountIn);
        }
        // Creation txid
        if (col == 9) {
            return QString::fromStdString(object.creationTxid.ToString());
        }
        // Output txid
        if (col == 10) {
            return QString::fromStdString(object.outputTxid.ToString());
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
        // Asset balance
        if (col == 2) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Headline
        if (col == 3) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Payload hash
        if (col == 4) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Output N
        if (col == 5) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Controller output N
        if (col == 6) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Confirmations
        if (col == 7) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Amount asset input
        if (col == 8) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        // Creation txid
        if (col == 9) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        // Output txid
        if (col == 10) {
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    }
    return QVariant();
}

QVariant MyAssetsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("ID");
            case 1:
                return QString("Ticker");
            case 2:
                return QString("Balance");
            case 3:
                return QString("Headline");
            case 4:
                return QString("Payload hash");
            case 5:
                return QString("Output N");
            case 6:
                return QString("Controller output N");
            case 7:
                return QString("Confirmations");
            case 8:
                return QString("Amount asset input");
            case 9:
                return QString("Creation txid");
            case 10:
                return QString("Output txid");
            }
        }
    }
    return QVariant();
}

void MyAssetsTableModel::Update()
{
    beginResetModel();
    model.clear();
    endResetModel();

    if (vpwallets.empty())
        return;
    if (vpwallets[0]->IsLocked())
        return;

    LOCK(vpwallets[0]->cs_wallet);

    std::vector<COutput> vOutput;
    vpwallets[0]->AvailableAssets(vOutput);

    beginInsertRows(QModelIndex(), 0, vOutput.size() - 1);
    for (const COutput& o : vOutput) {
        MyAssetTableObject object;
        object.nAssetAmount = o.tx->tx->vout[o.i].nValue;
        object.outputTxid = o.tx->GetHash();
        object.nOutputN = o.i;
        object.nConfirmations = o.nDepth;
        object.nAssetAmountIn = o.tx->amountAssetIn;
        object.nControlN = o.tx->nControlN;
        object.nID = o.tx->nAssetID;

        // Get BitAssetDB data
        BitAsset asset;
        if (passettree->GetAsset(o.tx->nAssetID, asset)) {
            object.strTicker = QString::fromStdString(asset.strTicker);
            object.strHeadline = QString::fromStdString(asset.strHeadline);
            object.payload = asset.payload;;
            object.creationTxid = asset.txid;
        }

        model.append(QVariant::fromValue(object));

    }
    endInsertRows();
}

void MyAssetsTableModel::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void MyAssetsTableModel::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model)
    {
        connect(model, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)),
                this, SLOT(Update()));

        Update();
    }
}
