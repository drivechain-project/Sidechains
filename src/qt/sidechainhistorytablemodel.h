// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINHISTORYTABLEMODEL_H
#define SIDECHAINHISTORYTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>

/** Model provides list of sidechain WT / WT^ transaction requests.
 * Filtered by height, ownership (are any wt(s) mine?), type (in / out).
 */
class SidechainHistoryTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    /* Display filtered list of sidechain WT requests */
    explicit SidechainHistoryTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private:
    QList<QVariant> filteredObjects;
};

#endif // SIDECHAINHISTORYTABLEMODEL_H
