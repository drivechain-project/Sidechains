#ifndef SIDECHAINESCROWTABLEMODEL_H
#define SIDECHAINESCROWTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct SidechainEscrowTableObject
{
    uint8_t nSidechain;
    uint16_t nWaitPeriod;
    uint16_t nVerificationPeriod;
    uint16_t nMinWorkScore;
    QString name;
    QString privKey;
    QString address;
    unsigned int thresholdGiven;
    unsigned int thresholdCalc;
    QString CTIPTxID;
    QString CTIPIndex;
};

class SidechainEscrowTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SidechainEscrowTableModel(QObject *parent = 0);
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

#endif // SIDECHAINESCROWTABLEMODEL_H
