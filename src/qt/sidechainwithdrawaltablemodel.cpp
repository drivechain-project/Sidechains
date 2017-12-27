#include "sidechainwithdrawaltablemodel.h"

#include "guiconstants.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <math.h>

#include <QIcon>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

Q_DECLARE_METATYPE(SidechainWithdrawalTableObject)

SidechainWithdrawalTableModel::SidechainWithdrawalTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    // This timer will be fired repeatedly to update the model
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateModel()));
    pollTimer->start(MODEL_UPDATE_DELAY * 4);
}

int SidechainWithdrawalTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return model.size();
}

int SidechainWithdrawalTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 8;
}

QVariant SidechainWithdrawalTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if (!model.at(col).canConvert<SidechainWithdrawalTableObject>())
        return QVariant();

    SidechainWithdrawalTableObject object = model.at(col).value<SidechainWithdrawalTableObject>();

    switch (role) {
    case Qt::DisplayRole:
    {
        // Escrow Number
        if (col == 0) {
            return object.nSidechain;
        }
        // WT^ hash
        if (col == 1) {
            return object.hashWTPrime;
        }
        // Acks
        if (row == 2) {
            return object.nAcks;
        }
        // Age
        if (row == 3) {
            return object.nAge;
        }
        // Wait period
        if (row == 4) {
            return object.nWaitPeriod;
        }
        // Max age
        if (row == 5) {
            return object.nMaxAge;
        }
        // Threshold
        if (row == 6) {
            return object.nThreshold;
        }
        // Approved
        if (row == 7) {
            return object.fApproved;
        }
    }
    }
    return QVariant();
}

QVariant SidechainWithdrawalTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Vertical) {
            switch (section) {
            case 0:
                return QString("Escrow Number");
            case 1:
                return QString("WT^");
            case 2:
                return QString("Acks");
            case 3:
                return QString("Age");
            case 4:
                return QString("Waiting Period");
            case 5:
                return QString("Max Age");
            case 6:
                return QString("Threshold");
            case 7:
                return QString("Approved");
            }
        }
    }
    return QVariant();
}

void SidechainWithdrawalTableModel::updateModel()
{
    if (!scdb.HasState())
        return;

    // Clear old data
    beginResetModel();
    model.clear();
    endResetModel();

    int nSidechains = ValidSidechains.size();
    beginInsertColumns(QModelIndex(), model.size(), model.size() + nSidechains);
    for (const Sidechain& s : ValidSidechains) {
        std::vector<SidechainWTPrimeState> vState = scdb.GetState(s.nSidechain);
        for (const SidechainWTPrimeState& wt : vState) {
            SidechainWithdrawalTableObject object;
            object.nSidechain = wt.nSidechain;
            object.hashWTPrime = QString::fromStdString(wt.hashWTPrime.ToString());
            object.nAcks = wt.nWorkScore;
            object.nAge = abs(wt.nBlocksLeft - s.nWaitPeriod);
            object.nWaitPeriod = s.nWaitPeriod;
            object.nMaxAge = s.GetTau();
            object.nThreshold = s.nMinWorkScore - object.nAcks;
            object.fApproved = scdb.CheckWorkScore(wt.nSidechain, wt.hashWTPrime);

            model.append(QVariant::fromValue(object));
        }
    }
    endInsertColumns();
}
