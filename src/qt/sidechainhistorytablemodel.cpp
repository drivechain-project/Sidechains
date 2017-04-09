// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechainhistorytablemodel.h"

SidechainHistoryTableModel::SidechainHistoryTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    /*
     * Table Columns:
     * 1. Type (incoming, outgoing)
     * 2. WT / WT^ ID
     * 3. Height
     */
}

int SidechainHistoryTableModel::rowCount(const QModelIndex &parent) const
{
    return filteredObjects.length();
}

int SidechainHistoryTableModel::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant SidechainHistoryTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    // Get the current object
    QVariant filteredObject = filteredObjects.at(row);

    switch (role) {
    case Qt::DecorationRole:
    {
        // Type icon
        if (col == 0) {
            // Incoming icon
            // Outgoing icon
        }
    }
    case Qt::DisplayRole:
    {
        // WT ID
        if (col == 1) {
            // Incoming
            // Outgoing
        }
        // Height
        if (col == 2) {
            // Incoming
            // Outgoing
        }
    }
    }
    return QVariant();
}

QVariant SidechainHistoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Type/Icon");
            case 1:
                return QString("WT ID");
            case 2:
                return QString("Height");
            }
        }
    }
    return QVariant();
}
