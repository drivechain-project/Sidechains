// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/txdetails.h>
#include <qt/forms/ui_txdetails.h>

#include <core_io.h>
#include <primitives/transaction.h>

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

enum TopLevelIndex {
    INDEX_P2SH = 0,
    INDEX_P2WSH,
    INDEX_WITNESS_PROGRAM,
    INDEX_WITNESS_COMMIT,
    INDEX_PREV_BLOCK_COMMIT,
    INDEX_WITHDRAWAL_BUNDLE_HASH_COMMIT,
    INDEX_BLOCK_VERSION_COMMIT,
    INDEX_UNKNOWN_OPRETURN,
    INDEX_BITASSETS,
};

TxDetails::TxDetails(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TxDetails)
{
    ui->setupUi(this);

    strHex = "";
    strTx = "";
}

TxDetails::~TxDetails()
{
    delete ui;
}

void  TxDetails::SetTransaction(const CMutableTransaction& mtx)
{
    CTransaction tx = CTransaction(mtx);

    strHex = EncodeHexTx(mtx);
    strTx = tx.ToString();

    // Display
    ui->textBrowserTx->setText(QString::fromStdString(strTx));
    ui->textBrowserHex->setText(QString::fromStdString(strHex));

    ui->labelHash->setText(QString::fromStdString(tx.GetHash().ToString()));
    ui->labelNumIn->setText(QString::number(mtx.vin.size()));
    ui->labelNumOut->setText(QString::number(mtx.vout.size()));
    ui->labelLockTime->setText(QString::number(mtx.nLockTime));
    ui->labelValueOut->setText(QString::number(tx.GetValueOut()));

    // Set note
    if (tx.IsCoinBase()) {
        ui->labelNote->setText("This is a coinbase transaction.");
    }
    else
    if (tx.nVersion == TRANSACTION_BITASSET_CREATE_VERSION) {
        ui->labelNote->setText("This is a BitAsset creation transaction.");
    }

    // TODO more things to display?
    // - sig op count
    // - script to asm str
    // - size in bytes

    // Look for outputs that we recognize the type of or can decode

    // Cache of values decoded from outputs
    // Witness program
    int nWitVersion = -1;
    std::vector<unsigned char> vWitProgram;
    uint256 hashWithdrawalBundle = uint256();
    uint256 hashPrevMain = uint256();
    uint256 hashPrevSide = uint256();
    int32_t nVersion = 0;

    ui->treeWidgetDecoded->clear();
    // TODO A lot of these output types can only be in the coinbase so we
    // shouldn't check for them in other transactions.
    for (size_t i = 0; i < tx.vout.size(); i++) {
        // TODO move these

        nWitVersion = -1;
        vWitProgram.clear();
        hashPrevMain.SetNull();
        hashPrevSide.SetNull();

        const CScript scriptPubKey = tx.vout[i].scriptPubKey;
        if (scriptPubKey.empty())
            continue;

        if (scriptPubKey.IsPayToScriptHash()) {
            // Create a p2sh item
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "txout #" + QString::number(i));
            subItem->setText(1, "P2SH:\n" +
                        QString::fromStdString(ScriptToAsmStr(scriptPubKey)));
            AddTreeItem(INDEX_P2SH, subItem);
        }
        else
        if (scriptPubKey.IsPayToWitnessScriptHash()) {
            // Create a p2wsh item
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "txout #" + QString::number(i));
            subItem->setText(1, "P2WSH:\n" +
                        QString::fromStdString(ScriptToAsmStr(scriptPubKey)));
            AddTreeItem(INDEX_P2WSH, subItem);
        }
        else
        if (scriptPubKey.IsWitnessProgram(nWitVersion, vWitProgram)) {
            // Create a witness program item
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "txout #" + QString::number(i));
            subItem->setText(1, "Witness Program:\n" +
                        QString::fromStdString(ScriptToAsmStr(scriptPubKey)));
            AddTreeItem(INDEX_WITNESS_PROGRAM, subItem);
        }
        else
        if (scriptPubKey.IsPrevBlockCommit(hashPrevMain, hashPrevSide)) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "txout #" + QString::number(i));
            QString str = "PrevBlock Commit: \n";
            str += "Previous mainchain block hash:\n";
            str += QString::fromStdString(hashPrevMain.ToString());
            str += "\n";
            str += "Previous sidechain block hash:\n";
            str += QString::fromStdString(hashPrevSide.ToString());

            subItem->setText(1, str);
            AddTreeItem(INDEX_PREV_BLOCK_COMMIT, subItem);
        }
        else
        if (scriptPubKey.IsWithdrawalBundleHashCommit(hashWithdrawalBundle)) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "txout #" + QString::number(i));
            QString str = "WithdrawalBundle Hash Commit: \n";
            str += QString::fromStdString(hashWithdrawalBundle.ToString());

            subItem->setText(1, str);
            AddTreeItem(INDEX_WITHDRAWAL_BUNDLE_HASH_COMMIT, subItem);
        }
        else
        if (scriptPubKey.IsBlockVersionCommit(nVersion)) {
            QTreeWidgetItem *subItem = new QTreeWidgetItem();
            subItem->setText(0, "txout #" + QString::number(i));
            QString str = "Block Version Commit: \n";
            str += QString::number(nVersion);

            subItem->setText(1, str);
            AddTreeItem(INDEX_BLOCK_VERSION_COMMIT, subItem);
        }
        else
        if (scriptPubKey.front() == OP_RETURN && scriptPubKey.size() == 38) {
            // Check for witness commitment. There is no IsWitnessCommit script
            // function so we have to check ourselves here:
            if (scriptPubKey[1] == 0x24 &&
                    scriptPubKey[2] == 0xaa &&
                    scriptPubKey[3] == 0x21 &&
                    scriptPubKey[4] == 0xa9 &&
                    scriptPubKey[5] == 0xed) {

                // Create a witness commit item
                QTreeWidgetItem *subItem = new QTreeWidgetItem();
                subItem->setText(0, "txout #" + QString::number(i));
                subItem->setText(1, "Witness Commitment:\n" +
                        QString::fromStdString(ScriptToAsmStr(scriptPubKey)));
                AddTreeItem(INDEX_WITNESS_COMMIT, subItem);
            }
        }
    }

    // Add BitAsset tx details
    if (tx.nVersion == TRANSACTION_BITASSET_CREATE_VERSION) {
        QTreeWidgetItem *subItem = new QTreeWidgetItem();
        subItem->setText(0, "BitAsset Creation");
        QString str = "Ticker: " + QString::fromStdString(tx.ticker) + "\n";
        str += "Headline: " + QString::fromStdString(tx.headline) + "\n";
        str += "Payload: " + QString::fromStdString(tx.payload.ToString()) + "\n";

        int64_t nSupply = tx.vout.size() >= 2 ? tx.vout[1].nValue : 0;

        str += "Supply: " + QString::number(nSupply) + "\n";

        subItem->setText(1, str);
        AddTreeItem(INDEX_BITASSETS, subItem);
    }

    ui->treeWidgetDecoded->expandAll();
    ui->treeWidgetDecoded->resizeColumnToContents(0);
    ui->treeWidgetDecoded->resizeColumnToContents(1);
}

void TxDetails::AddTreeItem(int index, QTreeWidgetItem *item)
{
    if (!item)
        return;

    if (index < 0 || index > INDEX_BITASSETS)
        return;

    QTreeWidgetItem *topItem = ui->treeWidgetDecoded->topLevelItem(index);
    if (!topItem) {
        topItem = new QTreeWidgetItem(ui->treeWidgetDecoded);
        if (index == INDEX_P2SH)
            topItem->setText(0, "P2SH");
        else
        if (index == INDEX_P2WSH)
            topItem->setText(0, "P2WSH");
        else
        if (index == INDEX_WITNESS_PROGRAM)
            topItem->setText(0, "Witness Program");
        else
        if (index == INDEX_WITNESS_COMMIT)
            topItem->setText(0, "Witness Commit");
        else
        if (index == INDEX_PREV_BLOCK_COMMIT)
            topItem->setText(0, "PrevBlock Commit");
        if (index == INDEX_WITHDRAWAL_BUNDLE_HASH_COMMIT)
            topItem->setText(0, "WithdrawalBundle Hash Commit");
        else
        if (index == INDEX_UNKNOWN_OPRETURN)
            topItem->setText(0, "Unknown OP_RETURN");
        else
        if (index == INDEX_BITASSETS)
            topItem->setText(0, "BitAssets");

        ui->treeWidgetDecoded->insertTopLevelItem(index, topItem);
    }

    if (!topItem)
        return;

    topItem->addChild(item);
}
