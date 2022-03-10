// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/browseassetspage.h>
#include <qt/forms/ui_browseassetspage.h>

#include <qt/clientmodel.h>
#include <qt/assettablemodel.h>

#include <QScrollBar>
#include <QSortFilterProxyModel>

BrowseAssetsPage::BrowseAssetsPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BrowseAssetsPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    // Style table

    // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    ui->tableViewAssets->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableViewAssets->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Don't stretch last cell of horizontal header
    ui->tableViewAssets->horizontalHeader()->setStretchLastSection(false);

    // Hide vertical header
    ui->tableViewAssets->verticalHeader()->setVisible(false);

    // Left align the horizontal header text
    ui->tableViewAssets->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Set horizontal scroll speed to per 3 pixels (very smooth, default is awful)
    ui->tableViewAssets->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewAssets->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels

    // Disable word wrap
    ui->tableViewAssets->setWordWrap(false);

    // Select rows
    ui->tableViewAssets->setSelectionBehavior(QAbstractItemView::SelectRows);

    tableModel = new AssetTableModel(this);

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);
    proxyModel->setSortRole(Qt::EditRole);

    ui->tableViewAssets->setModel(proxyModel);

    ui->tableViewAssets->setSortingEnabled(true);
    ui->tableViewAssets->sortByColumn(0, Qt::DescendingOrder);
}

BrowseAssetsPage::~BrowseAssetsPage()
{
    delete ui;
}

void BrowseAssetsPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model)
    {
        tableModel->setClientModel(model);
    }
}
