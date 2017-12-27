#include "sidechainescrowtablemodel.h"

#include "guiconstants.h"
#include "sidechain.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <math.h>

#include <QIcon>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

Q_DECLARE_METATYPE(SidechainEscrowTableObject)

SidechainEscrowTableModel::SidechainEscrowTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    // This timer will be fired repeatedly to update the model
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateModel()));
    pollTimer->start(MODEL_UPDATE_DELAY * 4);
}

int SidechainEscrowTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return 11;
}

int SidechainEscrowTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

QVariant SidechainEscrowTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(col).canConvert<SidechainEscrowTableObject>())
        return QVariant();

    SidechainEscrowTableObject object = model.at(col).value<SidechainEscrowTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Escrow Number
        if (row == 0) {
            return object.nSidechain;
        }
        // Active
        if (row == 1) {
            return true;
        }
        // Escrow Name
        if (row == 2) {
            return object.name;
        }
        // Private key
        if (row == 3) {
            return object.privKey;
        }
        // Address
        if (row == 4) {
            return object.address;
        }
        // Waiting Period
        if (row == 5) {
            return object.nWaitPeriod;
        }
        // Voting Period
        if (row == 6) {
            return object.nVerificationPeriod;
        }
        // Threshold given
        if (row == 7) {
            return object.thresholdGiven;
        }
        // Threshold calc
        if (row == 8) {
            return object.thresholdCalc;
        }
        // CTIP - TxID
        if (row == 9) {
            return object.CTIPTxID;
        }
        // CTIP - Index
        if (row == 10) {
            return object.CTIPIndex;
        }
    }
    }
    return QVariant();
}

QVariant SidechainEscrowTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Vertical) {
            switch (section) {
            case 0:
                return QString("Escrow Number");
            case 1:
                return QString("Active");
            case 2:
                return QString("Escrow Name");
            case 3:
                return QString("Private Key");
            case 4:
                return QString("Address");
            case 5:
                return QString("Waiting Period");
            case 6:
                return QString("Voting Period");
            case 7:
                return QString("Threshold Given");
            case 8:
                return QString("Threshold Calc");
            case 9:
                return QString("CTIP - TxID");
            case 10:
                return QString("CTIP - Index");
            }
        }
    }
    return QVariant();
}

void SidechainEscrowTableModel::updateModel()
{
    if (!pwalletMain)
        return;

    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(pwalletMain->cs_wallet, lockWallet);
    if(!lockWallet)
        return;

    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    int nSidechains = ValidSidechains.size();
    beginInsertColumns(QModelIndex(), model.size(), model.size() + nSidechains);

    for (const Sidechain& s : ValidSidechains) {
        SidechainEscrowTableObject object;
        object.nSidechain = s.nSidechain;
        object.nVerificationPeriod = s.nVerificationPeriod;
        object.nWaitPeriod = s.nWaitPeriod;
        object.name = QString::fromStdString(s.GetSidechainName());
        object.address = "";
        object.privKey = "";
        object.thresholdGiven = ceil(object.nVerificationPeriod / 256);
        object.thresholdCalc = ceil(object.nWaitPeriod * (object.nVerificationPeriod / 256));


        // Get the CTIP info
        {
            std::vector<COutput> vSidechainCoins;
#ifdef ENABLE_WALLET
            LOCK2(cs_main, pwalletMain->cs_wallet);
            pwalletMain->AvailableSidechainCoins(vSidechainCoins, 0);
#endif
            if (vSidechainCoins.size()) {
                object.CTIPIndex = vSidechainCoins.front().i;
                object.CTIPTxID = QString::fromStdString(vSidechainCoins.front().tx->GetHash().ToString());
            } else {
                object.CTIPIndex = "None";
                object.CTIPTxID = "None";
            }
        }
        model.append(QVariant::fromValue(object));
    }

    endInsertColumns();
}
