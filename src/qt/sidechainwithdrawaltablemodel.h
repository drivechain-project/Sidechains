#ifndef SIDECHAINWITHDRAWALTABLEMODEL_H
#define SIDECHAINWITHDRAWALTABLEMODEL_H

#include "uint256.h"

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct SidechainWithdrawalTableObject
{
    uint8_t nSidechain;
    QString hashWTPrime;
    uint16_t nAcks;
    uint32_t nAge;
    uint16_t nWaitPeriod;
    uint32_t nMaxAge;
    uint16_t nThreshold;
    bool fApproved;
};

class SidechainWithdrawalTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SidechainWithdrawalTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public Q_SLOTS:
    void updateModel();

private:
    QList<QVariant> model;
    QTimer *pollTimer;
};

#endif // SIDECHAINWITHDRAWALTABLEMODEL_H
