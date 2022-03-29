// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/myassetspage.h>
#include <qt/forms/ui_myassetspage.h>

#include <qt/assettransferdialog.h>
#include <qt/clientmodel.h>
#include <qt/myassetstablemodel.h>
#include <qt/walletmodel.h>

#include <QMenu>
#include <QScrollBar>
#include <QSortFilterProxyModel>

#include <uint256.h>
#include <wallet/wallet.h>

MyAssetsPage::MyAssetsPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MyAssetsPage),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    // Style table

    // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    ui->tableViewMyAssets->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableViewMyAssets->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

#endif

    // Don't stretch last cell of horizontal header
    ui->tableViewMyAssets->horizontalHeader()->setStretchLastSection(false);

    // Hide vertical header
    ui->tableViewMyAssets->verticalHeader()->setVisible(false);

    // Left align the horizontal header text
    ui->tableViewMyAssets->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Set horizontal scroll speed to per 3 pixels (very smooth, default is awful)
    ui->tableViewMyAssets->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewMyAssets->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels

    // Disable word wrap
    ui->tableViewMyAssets->setWordWrap(false);

    // Select rows
    ui->tableViewMyAssets->setSelectionBehavior(QAbstractItemView::SelectRows);

    tableModel = new MyAssetsTableModel(this);

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);
    proxyModel->setSortRole(Qt::EditRole);

    ui->tableViewMyAssets->setModel(proxyModel);

    ui->tableViewMyAssets->setSortingEnabled(true);
    ui->tableViewMyAssets->sortByColumn(0, Qt::DescendingOrder);

    ui->tableViewMyAssets->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction *showTransferDialog = new QAction(tr("Transfer BitAsset"), this);

    contextMenu = new QMenu(this);
    contextMenu->setObjectName("contextMenu");
    contextMenu->addAction(showTransferDialog);

    connect(ui->tableViewMyAssets, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(showTransferDialog, SIGNAL(triggered()), this, SLOT(showTransferDialog()));
}

MyAssetsPage::~MyAssetsPage()
{
    delete ui;
}

void MyAssetsPage::setWalletModel(WalletModel *model)
{
    walletModel = model;
    tableModel->setWalletModel(model);
}

void MyAssetsPage::setClientModel(ClientModel *model)
{
    clientModel = model;
    tableModel->setClientModel(model);
}

void MyAssetsPage::on_tableViewMyAssets_doubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    uint32_t nID = index.data(MyAssetsTableModel::IDRole).toUInt();
    uint256 txid = uint256S(index.data(MyAssetsTableModel::TxIDRole).toString().toStdString());
    int64_t nAssetAmount = index.data(MyAssetsTableModel::AmountRole).toInt();
    QString strTicker = index.data(MyAssetsTableModel::TickerRole).toString();
    QString strHeadline = index.data(MyAssetsTableModel::HeadlineRole).toString();

    AssetTransferDialog dialog;
    dialog.SetAsset(nID, txid, nAssetAmount, strTicker, strHeadline);
    dialog.exec();
}

void MyAssetsPage::contextualMenu(const QPoint& point)
{
    QModelIndex index = ui->tableViewMyAssets->indexAt(point);
    if (index.isValid())
        contextMenu->popup(ui->tableViewMyAssets->viewport()->mapToGlobal(point));
}

void MyAssetsPage::showTransferDialog()
{
    if (!ui->tableViewMyAssets->selectionModel())
        return;

    QModelIndexList selection = ui->tableViewMyAssets->selectionModel()->selectedRows();
    if (!selection.isEmpty())
        on_tableViewMyAssets_doubleClicked(selection.front());
}
