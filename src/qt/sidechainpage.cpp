// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sidechainpage.h>
#include <qt/forms/ui_sidechainpage.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/confgeneratordialog.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/manualbmmdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/sidechainbmmtablemodel.h>
#include <qt/sidechainwithdrawalconfirmationdialog.h>
#include <qt/sidechainwithdrawalbundlehistorydialog.h>
#include <qt/sidechainwithdrawaltablemodel.h>
#include <qt/walletmodel.h>

#include <base58.h>
#include <bmmcache.h>
#include <coins.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <miner.h>
#include <net.h>
#include <policy/withdrawalbundle.h>
#include <primitives/block.h>
#include <sidechain.h>
#include <sidechainclient.h>
#include <txdb.h>
#include <utilmoneystr.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QScrollBar>
#include <QBrush>
#include <QPalette>

#include <sstream>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h> /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

static const int nConnectionCheckInterval = 30 * 1000; // 30 seconds

static const int PAGE_DEFAULT_INDEX = 0;
static const int PAGE_RESTART_INDEX = 1;
static const int PAGE_CONNERR_INDEX = 2;
static const int PAGE_CONFIG_INDEX = 3;

SidechainPage::SidechainPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SidechainPage),
    platformStyle(_platformStyle),
    nBlocks(0)
{
    ui->setupUi(this);

    // Connection error message box
    connectionErrorMessage = new QMessageBox(this);
    connectionErrorMessage->setDefaultButton(QMessageBox::Ok);
    connectionErrorMessage->setWindowTitle("Failed to connect to the mainchain!");
    std::string str;
    str = "The sidechain has failed to connect to the mainchain!\n\n";
    str += "If this is your first time running the sidechain ";
    str += "please visit the \"Parent Chain\" tab.\n\n";
    str += "This may also be due to configuration issues. ";
    str += "Please check that you have set up configuration files.\n\n";
    str += "Also make sure that the mainchain node is running!\n\n";
    str += "Networking will be disabled until the connection is restored\n\n";
    str += "Will retry in a few seconds after you close this window...\n";
    connectionErrorMessage->setText(QString::fromStdString(str));

    // Initialize configuration generator dialog - only for the conf page of
    // the stacked widget. Clicking on "redo mainchain connection" will spawn
    // its own instance.
    confGeneratorDialog = new ConfGeneratorDialog(this, false /* fDialog */);
    connect(confGeneratorDialog, SIGNAL(Applied()), this, SLOT(ShowRestartPage()));

    // Initialize the BMM Automation refresh timer
    bmmTimer = new QTimer(this);
    connect(bmmTimer, SIGNAL(timeout()), this, SLOT(RefreshBMM()));

    // Initialize and start the connection check timer
    connectionCheckTimer = new QTimer(this);
    connect(connectionCheckTimer, SIGNAL(timeout()), this, SLOT(CheckConnection()));
    connectionCheckTimer->start(nConnectionCheckInterval);

    // Initialize pending Withdrawaltable model
    unspentWTModel = new SidechainWithdrawalTableModel(this);

    // Initialize BMM table model;
    bmmModel = new SidechainBMMTableModel(this);

    // Pending Withdrawaltable custom context menu

    ui->tableViewUnspentWT->setContextMenuPolicy(Qt::CustomContextMenu);

    wtContextMenu = new QMenu(this);
    wtContextMenu->setObjectName("wtContextMenu");

    copyWithdrawalIDAction = new QAction(tr("Copy Withdrawal ID"), this);
    withdrawalRefundAction = new QAction(tr("Cancel Withdrawal"), this);

    wtContextMenu->addAction(copyWithdrawalIDAction);
    wtContextMenu->addAction(withdrawalRefundAction);

    connect(ui->tableViewUnspentWT, SIGNAL(customContextMenuRequested(QPoint)), this,
            SLOT(WTContextMenu(QPoint)));
    connect(copyWithdrawalIDAction, SIGNAL(triggered()), this, SLOT(CopyWithdrawalID()));
    connect(withdrawalRefundAction, SIGNAL(triggered()), this, SLOT(RequestRefund()));

    // Withdrawal confirmation dialog
    wtConfDialog = new SidechainWithdrawalConfirmationDialog(this);

    // Table style

#if QT_VERSION < 0x050000
    ui->tableWidgetWTs->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidgetWTs->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewUnspentWT->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewUnspentWT->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    ui->tableWidgetWTs->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidgetWTs->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewUnspentWT->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewUnspentWT->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    // Hide vertical header
    ui->tableWidgetWTs->verticalHeader()->setVisible(false);
    ui->tableViewUnspentWT->verticalHeader()->setVisible(false);
    ui->tableViewBMM->verticalHeader()->setVisible(false);
    // Left align the horizontal header text
    ui->tableWidgetWTs->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tableViewUnspentWT->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tableViewBMM->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    // Set horizontal scroll speed to per 3 pixels
    ui->tableWidgetWTs->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewUnspentWT->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableWidgetWTs->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels
    ui->tableViewUnspentWT->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels
    ui->tableViewBMM->horizontalHeader()->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableViewBMM->horizontalHeader()->horizontalScrollBar()->setSingleStep(3); // 3 Pixels
    // Select entire row
    ui->tableWidgetWTs->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewUnspentWT->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewBMM->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Select only one row
    ui->tableWidgetWTs->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewUnspentWT->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewBMM->setSelectionMode(QAbstractItemView::SingleSelection);
    // Disable word wrap
    ui->tableWidgetWTs->setWordWrap(false);
    ui->tableViewUnspentWT->setWordWrap(false);

    // Set unspent Withdrawal table model
    ui->tableViewUnspentWT->setModel(unspentWTModel);

    // Set BMM table model
    ui->tableViewBMM->setModel(bmmModel);

    // Set BMM table column sizes
    ui->tableViewBMM->setColumnWidth(0, COLUMN_BMM_TXID);
    ui->tableViewBMM->setColumnWidth(1, COLUMN_MAINCHAIN_HEIGHT);
    ui->tableViewBMM->setColumnWidth(2, COLUMN_SIDECHAIN_HEIGHT);
    ui->tableViewBMM->setColumnWidth(3, COLUMN_TXNS);
    ui->tableViewBMM->setColumnWidth(4, COLUMN_FEES);
    ui->tableViewBMM->setColumnWidth(5, COLUMN_BMM_AMOUNT);
    ui->tableViewBMM->setColumnWidth(6, COLUMN_PROFIT);
    ui->tableViewBMM->setColumnWidth(7, COLUMN_STATUS);

    // Right align BMM table header
    ui->tableViewBMM->horizontalHeader()->setDefaultAlignment(Qt::AlignRight);

    std::string strAddress = GenerateDepositAddress(GenerateAddress("Sidechain Deposit"));
    ui->lineEditDepositAddress->setText(QString::fromStdString(strAddress));
    generateQR(strAddress);

    connect(ui->checkBoxAutoWithdrawalBundleRefresh, SIGNAL(stateChanged(int)), this,
            SLOT(on_checkBoxAutoWithdrawalBundleRefresh_changed(int)));

    // Set the fee label
    QString strFee = "Note: this sidechain will collect its own fee of: ";
    strFee += BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, SIDECHAIN_DEPOSIT_FEE);
    ui->labelFee->setText(strFee);

    // Check configuration & connection - set page to configuration
    // page if configuration / connection check fails
    bool fConfiguration = false;
    bool fConnection = false;
    CheckConfiguration(fConfiguration, fConnection);

    // Display the configuration widget if we need to - hide sidechain page
    if (!fConfiguration) {
        ui->stackedWidget->addWidget(confGeneratorDialog);
        ui->stackedWidget->setCurrentIndex(PAGE_CONFIG_INDEX);
    }
    else
    if (!fConnection) {
        ui->stackedWidget->setCurrentIndex(PAGE_CONNERR_INDEX);
    } else {
        ui->stackedWidget->setCurrentIndex(PAGE_DEFAULT_INDEX);
    }

    ui->bmmBidAmount->setValue(DEFAULT_CRITICAL_DATA_AMOUNT);

    ui->payAmount->setValue(0);
    ui->feeAmount->setValue(0);
    ui->mainchainFeeAmount->setValue(0);

    // Initialize WithdrawalBundle history dialog. We want users to be able to keep
    // this window open while using the rest of the software.
    withdrawalBundleHistoryDialog = new SidechainWithdrawalBundleHistoryDialog();
    withdrawalBundleHistoryDialog->setParent(this, Qt::Window);

    connect(withdrawalBundleHistoryDialog, SIGNAL(doubleClickedWithdrawalBundle(uint256)),
            this, SLOT(on_withdrawalBundle_doubleClicked(uint256)));

    // Update the total Withdrawalamount when withdrawal values are changed
    connect(ui->payAmount, SIGNAL(valueChanged()),
            this, SLOT(UpdateWTTotal()));
    connect(ui->feeAmount, SIGNAL(valueChanged()),
            this, SLOT(UpdateWTTotal()));
    connect(ui->mainchainFeeAmount, SIGNAL(valueChanged()),
            this, SLOT(UpdateWTTotal()));

    // Setup wealth tab. This isn't a usable tab (right now) and is actually
    // just a trick to show a label next to the tabs on the tab widget. There's
    // also an unused (hidden) spacer tab to move the wealth label over a bit.
    // TODO consts for tab index
    // Hide the spacer tab that separates the label we have inserted from the
    // other tabs. We have a custom style sheet for disabled tabs.
    ui->tabWidgetMain->setStyleSheet("QTabBar::tab:disabled {background: transparent;}");
    ui->tabWidgetMain->setTabEnabled(3, false);
    // Set the total wealth tab disabled as well
    ui->tabWidgetMain->setTabEnabled(4, false);

    ui->tabWidgetMain->tabBar()->setTabTextColor(4, QApplication::palette().text().color());

    // Start with the stopBMM button disabled
    ui->pushButtonStopBMM->setEnabled(false);
    ui->pushButtonNewBMM->setEnabled(false);

    ui->tableViewBMM->setSelectionMode(QAbstractItemView::NoSelection);

    // Setup platform style single color icons

    // Buttons
    ui->pushButtonNewBMM->setIcon(platformStyle->SingleColorIcon(":/movies/spinner-000"));
    ui->pushButtonWTHelp->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->pasteButton->setIcon(platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->deleteButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->pushButtonCopy->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));
    ui->pushButtonNew->setIcon(platformStyle->SingleColorIcon(":/movies/spinner-000"));

    // Main sidechain tab widget
    ui->tabWidgetMain->setTabIcon(0, platformStyle->SingleColorIcon(":/icons/tx_inout"));
    ui->tabWidgetMain->setTabIcon(1, platformStyle->SingleColorIcon(":/icons/history"));
    ui->tabWidgetMain->setTabIcon(2, platformStyle->SingleColorIcon(":/icons/tx_mined"));

    // Transfer tab widget
    ui->tabWidgetTransfer->setTabIcon(0, platformStyle->SingleColorIcon(":/icons/left"));
    ui->tabWidgetTransfer->setTabIcon(1, platformStyle->SingleColorIcon(":/icons/right"));

    // Setup the total Withdrawalamount line edit
    ui->lineEditTotalWT->setValue(CAmount(0));
    ui->lineEditTotalWT->setDisplayMode();
}

SidechainPage::~SidechainPage()
{
    delete ui;
}

void SidechainPage::generateQR(std::string data)
{
    if (data.empty())
        return;

#ifdef USE_QRCODE
    ui->QRCode->clear();

    QString encode = QString::fromStdString(data);
    QRcode *code = QRcode_encodeString(encode.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);

    if (code) {
        QImage qr = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
        qr.fill(0xffffff);

        unsigned char *data = code->data;
        for (int y = 0; y < code->width; y++) {
            for (int x = 0; x < code->width; x++) {
                qr.setPixel(x + 4, y + 4, ((*data & 1) ? 0x0 : 0xffffff));
                data++;
            }
        }

        QRcode_free(code);
        ui->QRCode->setPixmap(QPixmap::fromImage(qr).scaled(200, 200));
    }
#endif
}

void SidechainPage::setWalletModel(WalletModel *model)
{
    walletModel = model;
    withdrawalBundleHistoryDialog->setWalletModel(model);
    unspentWTModel->setWalletModel(model);
    bmmModel->setWalletModel(model);
    if (model && model->getOptionsModel())
    {
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this,
                SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        // Also set the WithdrawalBundle explorer to the latest WithdrawalBundle. We don't do this in the
        // constructor because it depends on having the wallet model set.
        UpdateToLatestWithdrawalBundle(false /* fRequested */);

        // Set the sidechain wealth, which also requires the wallet model
        UpdateSidechainWealth();

        // Set Withdrawaltotal to 0, formatting requires wallet model -> options model
        UpdateWTTotal();

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();
    }
}

void SidechainPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    withdrawalBundleHistoryDialog->setClientModel(model);
    unspentWTModel->setClientModel(model);
    if (model)
    {
        connect(model, SIGNAL(numBlocksChanged(int, QDateTime, double, bool)),
                this, SLOT(setNumBlocks(int)));

        setNumBlocks(model->getNumBlocks());
    }
}

void SidechainPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance,
                               const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                               const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    ui->available->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
}

void SidechainPage::setNumBlocks(const int nBlocksIn)
{
    if (!clientModel)
        return;

    UpdateSidechainWealth();

    nBlocks = nBlocksIn;

    // Check on updates to current / next WithdrawalBundle

    uint256 hashLatest;
    if (!psidechaintree->GetLastWithdrawalBundleHash(hashLatest)) {
        // Update the next bundle label on the transfer tab
        ui->labelNextBundle->setText("Waiting for withdrawals.");

        // If there hasn't been a WithdrawalBundle created yet, display message on banner
        QString str = "Bundle: None yet. Waiting for withdrawals.";
        Q_EMIT WithdrawalBundleBannerUpdate(str);
        return;
    }

    if (hashLatest.IsNull()) {
        ui->labelNextBundle->setText("Waiting for withdrawals.");
        QString str = "Bundle: None yet. Waiting for withdrawals.";
        Q_EMIT WithdrawalBundleBannerUpdate(str);
        return;
    }

    SidechainWithdrawalBundle withdrawalBundle;
    if (!psidechaintree->GetWithdrawalBundle(hashLatest, withdrawalBundle)) {
        ui->labelNextBundle->setText("Error...");

        QString str = "Bundle: Error...";
        Q_EMIT WithdrawalBundleBannerUpdate(str);
        return;
    }

    // Update UI to the latest WithdrawalBundle if wanted
    if (ui->checkBoxAutoWithdrawalBundleRefresh->isChecked()) {
        // Update to the current WithdrawalBundle
        SetCurrentWithdrawalBundle(hashLatest.ToString(), false);
    }

    if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_FAILED) {
        // If the last WithdrawalBundle failed, display how many blocks should be remaining
        // in the cool down period before the next WithdrawalBundle
        int nWaitPeriod = WITHDRAWAL_BUNDLE_FAIL_WAIT_PERIOD - (nBlocksIn - withdrawalBundle.nFailHeight);
        if (nWaitPeriod < 0)
            nWaitPeriod = 0;

        ui->labelNextBundle->setText(QString::number(nWaitPeriod) + " blocks.");

        QString str;
        str = "Bundle: None right now. Next in: ";
        str += QString::number(nWaitPeriod);
        str += " blocks.";
        Q_EMIT WithdrawalBundleBannerUpdate(str);
        return;
    }
    else
    if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_SPENT) {
        ui->labelNextBundle->setText("Waiting for withdrawals.");
        QString str = "Bundle: None right now. Waiting for withdrawals.";
        Q_EMIT WithdrawalBundleBannerUpdate(str);
        return;
    }
    else
    if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_CREATED) {
        // If the WithdrawalBundle has created status, display the required work score minus
        // the current work score.
        SidechainClient client;
        int nWorkScore = 0;
        if (client.GetWorkScore(hashLatest, nWorkScore)) {
            ui->labelNextBundle->setText(QString::number(MAINCHAIN_WITHDRAWAL_BUNDLE_MIN_WORKSCORE - nWorkScore) + " blocks.");
        } else {
            ui->labelNextBundle->setText(QString::number(nWorkScore) + " blocks.");
        }

        QString strBanner = "Bundle: ";
        strBanner += QString::fromStdString(hashLatest.ToString());
        Q_EMIT WithdrawalBundleBannerUpdate(strBanner);
        return;
    }
}

void SidechainPage::on_pushButtonCopy_clicked()
{
    GUIUtil::setClipboard(ui->lineEditDepositAddress->text());
}

void SidechainPage::on_pushButtonNew_clicked()
{
    std::string strAddress = GenerateDepositAddress(GenerateAddress("Sidechain Deposit"));
    ui->lineEditDepositAddress->setText(QString::fromStdString(strAddress));

    // Generate QR code
    generateQR(strAddress);
}

void SidechainPage::on_pushButtonWithdraw_clicked()
{
    QMessageBox messageBox;
    messageBox.setDefaultButton(QMessageBox::Ok);

    if (vpwallets.empty()) {
        // No active wallet message box
        messageBox.setWindowTitle("No active wallet found!");
        messageBox.setText("You must have an active wallet to withdraw from sidechain");
        messageBox.exec();
        return;
    }

    if (vpwallets[0]->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked to withdraw from sidechain.");
        messageBox.exec();
        return;
    }

    // Check Withdrawalamount
    if (!validateWTAmount()) {
        // Invalid withdrawal amount message box
        messageBox.setWindowTitle("Invalid withdrawal amount!");
        messageBox.setText("Check the amount you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Check fee amount
    if (!validateFeeAmount()) {
        // Invalid fee amount message box
        messageBox.setWindowTitle("Invalid fee amount!");
        messageBox.setText("Check the amount you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Check mainchain fee amount
    if (!validateMainchainFeeAmount()) {
        // Invalid mainchain fee amount message box
        messageBox.setWindowTitle("Invalid mainchain fee amount!");
        messageBox.setText("Check the amount you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Check destination
    std::string strDest = ui->payTo->text().toStdString();
    CTxDestination dest = DecodeDestination(strDest, true);
    if (!IsValidDestination(dest)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid withdrawal destination!");
        messageBox.setText("Check the address you have entered and try again.");
        messageBox.exec();
        return;
    }

    // Generate refund destination
    std::string strRefundDest = GenerateAddress("WithdrawalRefund");
    CTxDestination refundDest = DecodeDestination(strRefundDest, false);
    if (!IsValidDestination(refundDest)) {
        // Invalid address message box
        messageBox.setWindowTitle("Invalid refund destination!");
        messageBox.setText("Check the refund address you have entered and try again.");
        messageBox.exec();
        return;
    }

    CAmount burnAmount = ui->payAmount->value();
    CAmount feeAmount = ui->feeAmount->value();
    CAmount mainchainFeeAmount = ui->mainchainFeeAmount->value();

    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QString strWTAmount = BitcoinUnits::formatWithUnit(unit, burnAmount, false, BitcoinUnits::separatorAlways);
    QString strFeeAmount = BitcoinUnits::formatWithMainchainUnit(unit, feeAmount, false, BitcoinUnits::separatorAlways);
    QString strMcFeeAmount = BitcoinUnits::formatWithMainchainUnit(unit, mainchainFeeAmount, false, BitcoinUnits::separatorAlways);

    // Show the Withdrawalconfirmation dialog and check results before executing
    wtConfDialog->SetInfo(strWTAmount, strFeeAmount, strMcFeeAmount, QString::fromStdString(strDest), QString::fromStdString(strRefundDest));
    wtConfDialog->exec();
    if (!wtConfDialog->GetConfirmed())
        return;

    std::string strError = "";
    uint256 txid;
    uint256 wtid;
    if (!vpwallets[0]->CreateWithdrawal(burnAmount, feeAmount, mainchainFeeAmount, strDest, strRefundDest, strError, txid, wtid)) {
        // Create burn transaction error message box
        messageBox.setWindowTitle("Creating withdrawal transaction failed!");
        QString createError = "Error creating transaction: ";
        createError += QString::fromStdString(strError);
        createError += "\n";
        messageBox.setText(createError);
        messageBox.exec();
        return;
    }

    // Cache users WithdrawalID
    bmmCache.CacheWithdrawalID(wtid);

    // Successful withdraw message box
    messageBox.setWindowTitle("Withdrawal transaction created!");
    QString result = "txid: " + QString::fromStdString(txid.ToString());
    result += "\n";
    result += "Amount withdrawn: ";
    result += BitcoinUnits::formatWithUnit(unit, burnAmount, false, BitcoinUnits::separatorAlways);
    messageBox.setText(result);
    messageBox.exec();
}

void SidechainPage::on_checkBoxAutoWithdrawalBundleRefresh_changed(int state)
{
    if (state == Qt::Checked) {
        UpdateToLatestWithdrawalBundle();
    }
}

void SidechainPage::on_withdrawalBundle_doubleClicked(uint256 hashWithdrawalBundle)
{
    ui->checkBoxAutoWithdrawalBundleRefresh->setChecked(false);
    SetCurrentWithdrawalBundle(hashWithdrawalBundle.ToString(), true);
}

void SidechainPage::on_lineEditWithdrawalBundleHash_returnPressed()
{
    ui->checkBoxAutoWithdrawalBundleRefresh->setChecked(false);
    std::string strHash = ui->lineEditWithdrawalBundleHash->text().toStdString();
    SetCurrentWithdrawalBundle(strHash);
}

void SidechainPage::UpdateWTTotal()
{
    // Update the total amount on the sidechain withdrawal area
    CAmount amountTotal = 0;
    amountTotal += ui->payAmount->value();
    amountTotal += ui->feeAmount->value();
    amountTotal += ui->mainchainFeeAmount->value();
    ui->lineEditTotalWT->setValue(amountTotal);
}

void SidechainPage::on_pushButtonWTHelp_clicked()
{
    QString help;
    help =
        "Withdrawal:\n"
        "The exact number of coins you'd like your mainchain address to "
        "receive.\n\n"

        "Transaction Fee:\n"
        "The usual transaction fee — every sidechain transaction pays a "
        "sidechain transaction fee, including this one.\n\n"

        "Mainchain Withdrawal Fee:\n"
        "Your withdrawal will be paid out in a mainchain txn. That txn needs "
        "to pay a transaction fee (in BTC) over there on the mainchain, "
        "to encourage mainchain miners to include it in a block. If your "
        "mainchain txn fee is too low, it may not be included in the "
        "withdrawal-constructor. The constructor automatically sorts all "
        "withdrawals by their mainchain fee/byte rate — you can view other "
        "withdrawal-candidates on the Withdrawal Explorer page.\n\n"

        " * You can cancel the withdrawal via the withdrawal explorer."
        " This costs a second sidechain txn fee.\n\n"
        " * Once included in a Bundle, withdrawals cannot be canceled."
        " Bundles succeed or fail as a group.\n\n"
        " * If a bundle fails, its withdrawals reenter the pool of Candidate"
        " WTs. A grace period of 144 SC blocks (~24 hours) allows frustrated"
        " users to bail out of the withdrawal process (and reclaim their SC"
        " coins).\n";

    QMessageBox messageBox;
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle("Sidechain withdrawal info");
    messageBox.setText(help);
    messageBox.exec();
}

void SidechainPage::on_pushButtonStartBMM_clicked()
{
    StartBMM();
}

void SidechainPage::on_pushButtonStopBMM_clicked()
{
    StopBMM();
}

void SidechainPage::on_addressBookButton_clicked()
{

}

void SidechainPage::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SidechainPage::on_deleteButton_clicked()
{
    ui->payTo->clear();
}

bool SidechainPage::validateWTAmount()
{
    if (!ui->payAmount->validate()) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0) {
        ui->payAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        return false;
    }

    return true;
}

bool SidechainPage::validateFeeAmount()
{
    if (!ui->feeAmount->validate()) {
        ui->feeAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->feeAmount->value(0) <= 0) {
        ui->feeAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->feeAmount->value())) {
        ui->feeAmount->setValid(false);
        return false;
    }

    return true;
}

bool SidechainPage::validateMainchainFeeAmount()
{
    if (!ui->mainchainFeeAmount->validate()) {
        ui->mainchainFeeAmount->setValid(false);
        return false;
    }

    // Sending a zero amount is invalid
    if (ui->mainchainFeeAmount->value(0) <= 0) {
        ui->mainchainFeeAmount->setValid(false);
        return false;
    }

    // Reject dust outputs:
    if (GUIUtil::isDust(ui->payTo->text(), ui->mainchainFeeAmount->value())) {
        ui->mainchainFeeAmount->setValid(false);
        return false;
    }

    return true;
}

std::string SidechainPage::GenerateAddress(const std::string& strLabel)
{
    if (vpwallets.empty())
        return "";

    LOCK2(cs_main, vpwallets[0]->cs_wallet);

    vpwallets[0]->TopUpKeyPool();

    CPubKey newKey;
    std::string strAddress = "";
    if (vpwallets[0]->GetKeyFromPool(newKey)) {
        // We want a "legacy" type address
        OutputType type = OUTPUT_TYPE_LEGACY;
        CTxDestination dest = GetDestinationForKey(newKey, type);

        // Watch the script
        vpwallets[0]->LearnRelatedScripts(newKey, type);

        strAddress = EncodeDestination(dest);

        // Add to address book
        vpwallets[0]->SetAddressBook(dest, strLabel, "receive");
    }

    return strAddress;
}

void SidechainPage::RefreshBMM()
{
    QMessageBox messageBox;
    messageBox.setDefaultButton(QMessageBox::Ok);

    // Get & check the BMM amount
    CAmount amount = ui->bmmBidAmount->value();
    if (amount <= 0) {
        StopBMM();

        messageBox.setWindowTitle("Automated BMM failed - invalid BMM amount!");
        std::string str;
        str = "The amount set for the BMM request is invalid!\n\n";
        str += "The BMM request must pay the mainchain miner an amount ";
        str += "greater than zero. Please set an amount greater than ";
        str += "zero and try again.\n";
        messageBox.setText(QString::fromStdString(str));
        messageBox.exec();
        return;
    }

    if (!CheckMainchainConnection()) {
        UpdateNetworkActive(false /* fMainchainConnected */);
        StopBMM();

        messageBox.setWindowTitle("Automated BMM failed - mainchain connection failed!");
        std::string str;
        str = "The sidechain has failed to connect to the mainchain!\n\n";
        str += "Please check configuration file settings and verify that the ";
        str += "mainchain is running!";
        messageBox.setText(QString::fromStdString(str));
        messageBox.exec();
        return;
    }

    bool fReorg = false;
    std::vector<uint256> vOrphan;
    if (!UpdateMainBlockHashCache(fReorg, vOrphan))
    {
        StopBMM();
        UpdateNetworkActive(false /* fMainchainConnected */);
        messageBox.setWindowTitle("Automated BMM failed - couldn't update mainchain block cache!");
        std::string str;
        str = "The sidechain has failed to update the mainchain block cache!\n\n";
        str += "Please check configuration file settings and verify that the ";
        str += "mainchain is running!";
        messageBox.setText(QString::fromStdString(str));
        messageBox.exec();
        return;
    }
    if (fReorg)
        HandleMainchainReorg(vOrphan);

    SidechainClient client;
    std::string strError = "";
    uint256 hashCreatedMerkleRoot;
    uint256 hashConnected;
    uint256 hashMerkleRoot;
    uint256 txid;
    CAmount nFees = 0;
    int ntxn = 0;
    if (!client.RefreshBMM(amount, strError, hashCreatedMerkleRoot, hashConnected, hashMerkleRoot, txid, ntxn, nFees)) {
        UpdateNetworkActive(false /* fMainchainConnected */);
        StopBMM();

        messageBox.setWindowTitle("Automated BMM failed!");
        std::string str;
        str = "The sidechain has failed to refresh BMM status.\n\n";
        str += "This may be due to configuration issues.";
        str += " Please check that you have set up configuration files";
        str += " and re-enable automated BMM.\n\n";
        str += "Also make sure that the mainchain node is running!\n\n";
        str += "Networking will be disabled until mainchain connected!\n\n";
        str += "Error message:\n" + strError + "\n";
        messageBox.setText(QString::fromStdString(str));
        messageBox.exec();
        return;
    }

    UpdateNetworkActive(true /* fMainchainConnected */);

    if (txid.IsNull() && !hashCreatedMerkleRoot.IsNull()) {
        messageBox.setWindowTitle("Failed to create mainchain BMM request!");
        std::string str;
        str = "The sidechain failed to create a BMM request.\n\n";
        str += "Please check that you have sufficient mainchain funds and ";
        str += "confirm that this sidechain is active on the mainchain.\n";
        str += "Automated BMM will continue.\n";
        messageBox.setText(QString::fromStdString(str));
        messageBox.exec();
    }

    // Update GUI
    if (!hashCreatedMerkleRoot.IsNull()) {
        BMMTableObject object;

        if (amount > 0)
            object.amount = amount;

        if (!txid.IsNull())
            object.txid = txid;

        // Add attempt sidechain block height
        object.nSidechainHeight = nBlocks + 1;

        // Add mainchain block height. GetCachedBlockCount includes the
        // genesis block so it is actually one more than the reported
        // mainchain height. BMM requests are for the next mainchain
        // block so the +1 is okay.
        object.nMainchainHeight = bmmCache.GetCachedBlockCount();

        object.ntxn = ntxn;

        // Add total txn fees of the new block.
        object.amountTotalFees = nFees;

        object.hashMerkleRoot = hashCreatedMerkleRoot;

        bmmModel->AddAttempt(object);
    }

    if (!hashConnected.IsNull())
        bmmModel->UpdateForConnected(hashMerkleRoot);
}

void SidechainPage::on_spinBoxRefreshInterval_valueChanged(int n)
{
    // Check if the StartBMM button has been pushed
    if (!ui->pushButtonStartBMM->isEnabled()) {
        // Restart BMM with the new refresh interval
        StopBMM();
        StartBMM();
    }
}

void SidechainPage::on_pushButtonConfigureMainchainConnection_clicked()
{
    ConfGeneratorDialog *dialog = new ConfGeneratorDialog(this);
    dialog->exec();
}

void SidechainPage::ShowRestartPage()
{
    ui->stackedWidget->setCurrentIndex(PAGE_RESTART_INDEX);
}

void SidechainPage::on_pushButtonRetryConnection_clicked()
{
    bool fConfig = false;
    bool fConnection = false;
    CheckConfiguration(fConfig, fConnection);
    UpdateNetworkActive(fConnection);
}

void SidechainPage::on_pushButtonShowPastWithdrawalBundles_clicked()
{
    withdrawalBundleHistoryDialog->show();
}

void SidechainPage::UpdateNetworkActive(bool fMainchainConnected)
{
    // Enable or disable networking based on connection to mainchain
    SetNetworkActive(fMainchainConnected, "Sidechain page update.");

    if (fMainchainConnected) {
        // Close the connection warning popup if it is open
        connectionErrorMessage->close();
    }
    // Update the GUI to show or hide the network connection warning.
    // Only switch to the network warning if the configuration warning isn't
    // currently displayed.
    if (!fMainchainConnected && ui->stackedWidget->currentIndex() != PAGE_CONFIG_INDEX &&
            ui->stackedWidget->currentIndex() != PAGE_RESTART_INDEX) {
        ui->stackedWidget->setCurrentIndex(PAGE_CONNERR_INDEX);
    }
    else
    if (fMainchainConnected && ui->stackedWidget->currentIndex() != PAGE_CONFIG_INDEX){
        ui->stackedWidget->setCurrentIndex(PAGE_DEFAULT_INDEX);
    }
}

void SidechainPage::CheckConfiguration(bool& fConfig, bool& fConnection)
{
    fConfig = false;
    fConnection = false;

    fs::path pathHome = GetHomeDir();
    std::string strDrivechainData = "";

#ifdef WIN32
    strDrivechainData = "Drivechain";
#else
#ifdef MAC_OSX
    strDrivechainData = "Drivechain";
#else
    strDrivechainData = ".drivechain";
#endif
#endif

    // Does the drivechain directory exist?
    fs::path pathDrivechainData = pathHome / strDrivechainData;
    if (!fs::exists(pathDrivechainData))
        LogPrintf("%s: Configuration error - drivechain data directory not found!\n");

    // Does the sidechain directory exist?
    fs::path pathSide = GetDefaultDataDir();
    if (!fs::exists(pathSide)) {
        LogPrintf("%s: Configuration error - sidechain data directory not found!\n", __func__);
    }

    // Do we have configuration files for the mainchain & sidechain?
    fs::path pathConfMain = pathDrivechainData / "drivechain.conf";
    fs::path pathConfSide = pathSide / "trainchain.conf";

    // Do drivechain.conf & side.conf exist?
    if (fs::exists(pathConfMain) && fs::exists(pathConfSide))
        fConfig = true;

    // Check if we can connect to the mainchain
    fConnection = CheckMainchainConnection();

    // TODO maybe we shouldn't update network status here, it might not
    // be clear to someone calling this function that it will also
    // update network activity based on results...
    UpdateNetworkActive(fConnection);
}

void SidechainPage::SetCurrentWithdrawalBundle(const std::string& strHash, bool fRequested)
{
    if (!walletModel)
        return;

    // If the user didn't request this update (fRequested) themselves, don't
    // show error messages

    ClearWithdrawalBundleExplorer();

    int unit = walletModel->getOptionsModel()->getDisplayUnit();

    uint256 hash = uint256S(strHash);
    if (hash.IsNull()) {
        if (fRequested) {
            QMessageBox messageBox;
            messageBox.setDefaultButton(QMessageBox::Ok);

            messageBox.setWindowTitle("Invalid WithdrawalBundle hash");
            messageBox.setText("The WithdrawalBundle hash you have entered is invalid.");
            messageBox.exec();
        }
        return;
    }

    // Try to lookup the WithdrawalBundle
    SidechainWithdrawalBundle withdrawalBundle;
    if (!psidechaintree->GetWithdrawalBundle(hash, withdrawalBundle)) {
        if (fRequested) {
            QMessageBox messageBox;
            messageBox.setDefaultButton(QMessageBox::Ok);

            messageBox.setWindowTitle("Failed to lookup WithdrawalBundle");
            messageBox.setText("Could not locate specified WithdrawalBundle in the database.");
            messageBox.exec();
        }
        return;
    }

    QString qHash = QString::fromStdString(strHash);

    // Set the line edit to the current WithdrawalBundle hash
    ui->lineEditWithdrawalBundleHash->setText(qHash);
    ui->lineEditWithdrawalBundleHash->setCursorPosition(0);

    // Set number of Withdrawaloutputs
    ui->labelNumWT->setText(QString::number(withdrawalBundle.vWithdrawalID.size()));

    // If the WithdrawalBundle has WITHDRAWAL_BUNDLE_CREATED status, it should be being acked
    // by the mainchain (if it's already made it there). Try to request
    // the workscore and display it on the WithdrawalBundle explorer if we can.
    QString qStatus = "";
    if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_CREATED) {
        // Try to get the work score
        SidechainClient client;
        int nWorkScore = 0;
        if (client.GetWorkScore(hash, nWorkScore)) {
            qStatus = QString::number(nWorkScore);
            qStatus += " / ";
            qStatus += QString::number(MAINCHAIN_WITHDRAWAL_BUNDLE_MIN_WORKSCORE);
            qStatus += " ACK(s)";
        } else {
            qStatus = "Pending Proposal";
        }

    }
    else
    if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_FAILED) {
        qStatus = "Failed";
    }
    else
    if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_SPENT) {
        qStatus = "Spent";
    }

    // Set the status
    ui->labelStatus->setText(qStatus);

    // Add WTs to the table view
    CAmount amountTotal = 0;
    CAmount amountMainchainFees = 0;
    for (const uint256& id : withdrawalBundle.vWithdrawalID) {
        SidechainWithdrawal wt;
        if (!psidechaintree->GetWithdrawal(id, wt)) {
            if (fRequested) {
                QMessageBox messageBox;
                messageBox.setDefaultButton(QMessageBox::Ok);

                messageBox.setWindowTitle("Failed to lookup withdrawal from bundle");
                messageBox.setText("For the specified Withdrawal Bundle, one of the withdrawals could not be located in the database.");
                messageBox.exec();
            }
            ClearWithdrawalBundleExplorer();
            return;
        }

        // Add row for new data
        int nRows = ui->tableWidgetWTs->rowCount();
        ui->tableWidgetWTs->insertRow(nRows);

        // Add to total amount withdrawn
        amountTotal += wt.amount;

        // Add to total fees
        amountMainchainFees += wt.mainchainFee;

        // Add to table

        QString amount = BitcoinUnits::formatWithUnit(unit, wt.amount - wt.mainchainFee, false,
                BitcoinUnits::separatorAlways);

        QString fee = BitcoinUnits::formatWithMainchainUnit(unit, wt.mainchainFee, false,
                BitcoinUnits::separatorAlways);

        QTableWidgetItem* amountItem = new QTableWidgetItem(amount);
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem* feeItem = new QTableWidgetItem(fee);
        feeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem* destItem = new QTableWidgetItem(
                QString::fromStdString(wt.strDestination));

        ui->tableWidgetWTs->setItem(nRows /* row */, 0 /* col */, amountItem);
        ui->tableWidgetWTs->setItem(nRows /* row */, 1 /* col */, feeItem);
        ui->tableWidgetWTs->setItem(nRows /* row */, 2 /* col */, destItem);
    }

    // Set total amount withdrawn
    QString total = BitcoinUnits::formatWithUnit(unit, amountTotal, false,
            BitcoinUnits::separatorAlways);

    ui->labelTotalAmount->setText(total);

    // Set total fee amount
    QString fees = BitcoinUnits::formatWithMainchainUnit(unit, amountMainchainFees, false,
            BitcoinUnits::separatorAlways);

    ui->labelTotalFees->setText(fees);

    // Set block height
    ui->labelBlockHeight->setText(QString::number(withdrawalBundle.nHeight));

    // Set transaction size
    int64_t sz = GetTransactionWeight(withdrawalBundle.tx);

    QString size;
    size += QString::number(sz);
    size += " / ";
    size += QString::number(MAX_WITHDRAWAL_BUNDLE_WEIGHT);
    size += " wBytes";

    ui->labelTotalSize->setText(size);
}

void SidechainPage::StartBMM()
{
    bmmTimer->start(ui->spinBoxRefreshInterval->value() * 1000);
    ui->pushButtonStartBMM->setEnabled(false);
    ui->pushButtonStopBMM->setEnabled(true);
    ui->pushButtonNewBMM->setEnabled(true);
}

void SidechainPage::StopBMM()
{
    bmmTimer->stop();
    ui->pushButtonStartBMM->setEnabled(true);
    ui->pushButtonStopBMM->setEnabled(false);
    ui->pushButtonNewBMM->setEnabled(false);
}

void SidechainPage::CheckConnection()
{
    bool fConnected = CheckMainchainConnection();
    if (!fConnected) {
        UpdateNetworkActive(false /* fMainchainConnected */);
        connectionCheckTimer->stop();
        if (ui->stackedWidget->currentIndex() != PAGE_CONFIG_INDEX
                && ui->stackedWidget->currentIndex() != PAGE_RESTART_INDEX) {
            connectionErrorMessage->close();
            connectionErrorMessage->exec();

            // Start the timer again with a longer interval
            connectionCheckTimer->start(nConnectionCheckInterval * 2);
        }
    } else {
        // Set the timer to the normal interval
        connectionCheckTimer->stop();
        connectionCheckTimer->start(nConnectionCheckInterval);

        UpdateNetworkActive(true /* fMainchainConnected */);
    }
}

void SidechainPage::ClearWithdrawalBundleExplorer()
{
    ui->tableWidgetWTs->setRowCount(0);
    ui->labelNumWT->setText(QString::number(0));

    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QString zero = BitcoinUnits::formatWithUnit(unit, CAmount(0), false,
            BitcoinUnits::separatorAlways);

    ui->labelTotalAmount->setText(zero);

    ui->labelTotalFees->setText(zero);

    ui->labelStatus->setText("");
}

void SidechainPage::UpdateSidechainWealth()
{
    if (!walletModel)
        return;

    CAmount amountCTIP = CAmount(0);

    SidechainDeposit deposit;
    if (psidechaintree->GetLastDeposit(deposit)) {
        if (deposit.nBurnIndex >= deposit.dtx.vout.size())
            return;
        amountCTIP = deposit.dtx.vout[deposit.nBurnIndex].nValue;
    }

    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QString wealth = BitcoinUnits::formatWithUnit(unit, amountCTIP, false,
            BitcoinUnits::separatorAlways);

    QString label = "Total sidechain wealth: ";
    label += wealth;
    ui->tabWidgetMain->setTabText(4, label);
}

void SidechainPage::UpdateToLatestWithdrawalBundle(bool fRequested)
{
    uint256 hashLatest;
    if (!psidechaintree->GetLastWithdrawalBundleHash(hashLatest))
        return;

    SetCurrentWithdrawalBundle(hashLatest.ToString(), fRequested);
}

void SidechainPage::updateDisplayUnit()
{
    if (!walletModel)
        return;

    int nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    ui->bmmBidAmount->setDisplayUnit(nDisplayUnit);
    ui->payAmount->setDisplayUnit(nDisplayUnit);
    ui->feeAmount->setDisplayUnit(nDisplayUnit);
    ui->mainchainFeeAmount->setDisplayUnit(nDisplayUnit);
}

void SidechainPage::on_pushButtonNewBMM_clicked()
{
    if (bmmTimer->isActive()) {
        bmmCache.ClearBMMBlocks();
        RefreshBMM();
    }
}

void SidechainPage::on_checkBoxOnlyMyWithdrawals_toggled(bool fChecked)
{
    Q_EMIT(OnlyMyWithdrawalsToggled(fChecked));
}

void SidechainPage::WTContextMenu(const QPoint& point)
{
    QModelIndex index = ui->tableViewUnspentWT->indexAt(point);
    QModelIndexList selection = ui->tableViewUnspentWT->selectionModel()->selectedRows(0);
    if (selection.empty())
        return;

    if (index.isValid()) {
        bool fMine = selection.at(0).data(SidechainWithdrawalTableModel::IsMineRole).toBool();
        withdrawalRefundAction->setEnabled(fMine);
        wtContextMenu->popup(ui->tableViewUnspentWT->viewport()->mapToGlobal(point));
    }
}

void SidechainPage::CopyWithdrawalID()
{
    GUIUtil::copyEntryData(ui->tableViewUnspentWT, 0, SidechainWithdrawalTableModel::WithdrawalIDRole);
}

void SidechainPage::RequestRefund()
{
    if (!ui->tableViewUnspentWT->selectionModel())
        return;

    QModelIndexList selection = ui->tableViewUnspentWT->selectionModel()->selectedRows(0);
    if (selection.empty())
        return;

    // Get the WithdrawalID from the model
    uint256 wtID;
    wtID.SetHex(selection.at(0).data(SidechainWithdrawalTableModel::WithdrawalIDRole).toString().toStdString());

    QMessageBox messageBox;
    messageBox.setDefaultButton(QMessageBox::Ok);

    // Check for active wallet

    if (vpwallets.empty()) {
        // No active wallet message box
        messageBox.setWindowTitle("No active wallet found!");
        messageBox.setText("You must have an active wallet to request a Withdrawalrefund.");
        messageBox.exec();
        return;
    }
    if (vpwallets[0]->IsLocked()) {
        // Locked wallet message box
        messageBox.setWindowTitle("Wallet locked!");
        messageBox.setText("Wallet must be unlocked.");
        messageBox.exec();
        return;
    }

    // Get WT
    SidechainWithdrawal wt;
    if (!psidechaintree->GetWithdrawal(wtID, wt)) {
        messageBox.setWindowTitle("Failed to look up WT!");
        messageBox.setText("Specified withdrawal not found in database.");
        messageBox.exec();
        return;
    }

    // Check Withdrawal status
    if (wt.status != WITHDRAWAL_UNSPENT) {
        messageBox.setWindowTitle("Invalid Withdrawalstatus!");
        messageBox.setText("Withdrawalmust be unspent to refund.");
        messageBox.exec();
        return;
    }

    // Get refund address
    CTxDestination dest = DecodeDestination(wt.strRefundDestination);
    if (!IsValidDestination(dest)) {
        messageBox.setWindowTitle("Failed to decode refund destination!");
        messageBox.setText("Failed to decode refund destination address.");
        messageBox.exec();
        return;
    }

    // Double check that address is for KeyID
    const CKeyID* id = boost::get<CKeyID>(&dest);
    if (!id) {
        messageBox.setWindowTitle("Invalid refund destination!");
        messageBox.setText("The refund destination must be a \"legacy\" address.");
        messageBox.exec();
        return;
    }

    int unit = walletModel->getOptionsModel()->getDisplayUnit();

    // Confirm that the user wants to create refund request
    QMessageBox confirmMessage;
    confirmMessage.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    confirmMessage.setDefaultButton(QMessageBox::Cancel);
    confirmMessage.setIcon(QMessageBox::Information);
    confirmMessage.setWindowTitle("Confirm Withdrawal Refund Request");
    QString refundMessage = "This will create a refund request for your withdrawal.\n\n";
    refundMessage += BitcoinUnits::formatWithUnit(unit, wt.amount, false, BitcoinUnits::separatorAlways);
    refundMessage += " will be refunded to your refund address:\n\n";
    refundMessage += QString::fromStdString(wt.strRefundDestination);
    refundMessage += "\n\n";
    refundMessage += "This will cost an additional transaction fee.\n\n";
    refundMessage += "Are you sure?";
    confirmMessage.setText(refundMessage);

    int nRes = confirmMessage.exec();
    if (nRes == QMessageBox::Cancel)
        return;

    // Get private key for refund address from the wallet
    CKey privKey;
    {
        LOCK2(cs_main, vpwallets[0]->cs_wallet);

        if (!vpwallets[0]->GetKey(*id, privKey)) {
            messageBox.setWindowTitle("Failed to get private key for refund destination!");
            messageBox.setText("Cannot request refund for withdrawal created by another wallet.");
            messageBox.exec();
            return;
        }
    }

    // Get refund message hash
    uint256 hashMessage = GetWithdrawalRefundMessageHash(wtID);

    // Sign refund message hash
    std::vector<unsigned char> vchSig;
    if (!privKey.SignCompact(hashMessage, vchSig)) {
        messageBox.setWindowTitle("Failed to sign refund message!");
        messageBox.setText("Failed to sign refund request.");
        messageBox.exec();
        return;
    }

    std::string strFail = "";
    uint256 txid;
    if (!vpwallets[0]->CreateWithdrawalRefundRequest(wtID, vchSig, strFail, txid)) {
        // Create refund transaction error message box
        messageBox.setWindowTitle("Creating refund request failed!");
        QString error = "Error creating transaction: ";
        error += QString::fromStdString(strFail);
        error += "\n";
        messageBox.setText(error);
        messageBox.exec();
        return;
    }

    // TODO cache refund request?
    // Cache users WithdrawalID
    //bmmCache.CacheWithdrawalID(wtid);

    // Successful refund request message box
    messageBox.setWindowTitle("Refund request created!");
    QString result = "txid: " + QString::fromStdString(txid.ToString());
    result += "\n";
    result += "Amount to be refunded: ";
    result += BitcoinUnits::formatWithUnit(unit, wt.amount, false, BitcoinUnits::separatorAlways);
    messageBox.setText(result);
    messageBox.exec();
}

void SidechainPage::on_pushButtonManualBMM_clicked()
{
    ManualBMMDialog dialog;
    dialog.exec();
}
