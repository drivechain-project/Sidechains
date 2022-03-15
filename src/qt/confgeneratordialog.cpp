#include <qt/confgeneratordialog.h>

#include <qt/forms/ui_confgeneratordialog.h>

#include <fs.h>
#include <random.h>
#include <sidechain.h>
#include <util.h>

#include <QFile>
#include <QMessageBox>
#include <QTextStream>

ConfGeneratorDialog::ConfGeneratorDialog(QWidget *parent, bool fDialogIn) :
    QDialog(parent),
    ui(new Ui::ConfGeneratorDialog)
{
    ui->setupUi(this);

    fDialog = fDialogIn;

    // Configure for non-dialog use
    if (!fDialog) {
        ui->pushButtonClose->close();
    }
}

ConfGeneratorDialog::~ConfGeneratorDialog()
{
    delete ui;
}

void ConfGeneratorDialog::on_pushButtonClose_clicked()
{
    this->close();
}

void ConfGeneratorDialog::on_pushButtonApply_clicked()
{
    QMessageBox messageBox;
    if (ui->lineEditUser->text().isEmpty()) {
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setWindowTitle("Invalid RPC username");
        messageBox.setText("You must enter an RPC username!");
        messageBox.exec();
        return;
    }

    if (ui->lineEditPassword->text().isEmpty()) {
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setWindowTitle("Invalid RPC password");
        messageBox.setText("You must enter an RPC password!");
        messageBox.exec();
        return;
    }

    if (WriteConfigFiles(ui->lineEditUser->text(), ui->lineEditPassword->text())) {
        messageBox.setIcon(QMessageBox::Information);
        messageBox.setWindowTitle("Configuration files created!");
        QString str = "Configuration files created!\n\n";
        str += "You must restart Drivechain and any\n";
        str += "sidechains for changes to be applied.";
        messageBox.setText(str);
        messageBox.exec();

        // Close if dialog, otherwise signal that apply was pushed
        if (fDialog) {
            this->close();
        } else {
            Q_EMIT Applied();
        }
    }
}

void ConfGeneratorDialog::on_pushButtonRandom_clicked()
{
    std::string strSeed = GetRandHash().ToString();

    std::string strUser = strSeed.substr(0, 14);
    std::string strPass = strSeed.substr(15, 31);

    ui->lineEditUser->setText(QString::fromStdString(strUser));
    ui->lineEditPassword->setText(QString::fromStdString(strPass));
}

bool ConfGeneratorDialog::WriteConfigFiles(const QString& strUser, const QString& strPass)
{
    QMessageBox messageBox;
    messageBox.setWindowTitle("Error writing config files!");
    messageBox.setIcon(QMessageBox::Critical);

    // Does the sidechain directory exist?
    fs::path pathSide = GetDefaultDataDir();
    if (!fs::exists(pathSide)) {
        // Show error message
        messageBox.setText("Could not find bitassets data directory!");
        messageBox.exec();
        return false;
    }

    fs::path pathHome = GetHomeDir();

    std::string strData = "";
#ifdef WIN32
    strData = "Drivechain";
#else

#ifdef MAC_OSX
    strData = "Drivechain";
#else
    strData = ".drivechain";
#endif
#endif

    // Does the drivechain directory exist?
    fs::path pathData = pathHome / strData;
    if (!fs::exists(pathData)) {
        QString strError = "Drivechain data directory (~/.drivechain) not found!\n";
        messageBox.setText(strError);
        messageBox.exec();
        return false;
    }

    // If mainchain already has a configuration file, check if it is already
    // configured for RPC & BMM. If it is already configured we will copy the
    // username and password into the sidechain config file. If mainchain
    // doesn't have RPC configured we will generate a new mainchain config file.

    // Do we need to backup the old config file?
    fs::path pathConf = pathData / "drivechain.conf";
    bool fExists = fs::exists(pathConf);

    // Check for existing RPC configuration
    bool fMainchainConfigured = false;
    QString strMCUser = "";
    QString strMCPass = "";
    if (fExists) {
        QFile fileMain(QString::fromStdString(pathConf.string()));
        if (!fileMain.open(QIODevice::ReadWrite | QIODevice::Text)) {
            return false;
        }

        // Read the mainchain config file
        QTextStream in(&fileMain);
        QString strMain = in.readAll();
        fileMain.close();

        // Search for rpcuser, rpcpassword. Verify server & minerbreakforbmm
        bool fServer = false;
        bool fBreakBMM = false;

        // Split config file into list of strings
        QStringList listStrConf = strMain.split("\n", QString::SkipEmptyParts);
        for (const QString& str : listStrConf) {
            if (str.contains("rpcuser=")) {
                strMCUser = str.section("=", 1);
            }
            else
            if (str.contains("rpcpassword=")) {
                strMCPass = str.section("=", 1);
            }
            else
            if (str.contains("server=")) {
                fServer = true;
            }
            else
            if (str.contains("minerbreakforbmm=")) {
                fBreakBMM = true;
            }
        }

        if (!strMCUser.isEmpty() && !strMCPass.isEmpty() && fServer && fBreakBMM) {
            LogPrintf("%s: Detected existing mainchain configuration - copying!\n", __func__);
            fMainchainConfigured = true;
        }
    }

    // If the existing mainchain conf doesn't have RPC setup we will back it up
    // and delete the original.
    if (fExists && !fMainchainConfigured) {
        // Rename old configuration file
        QString strNew = QString::fromStdString(pathConf.string());
        strNew += ".OLD";

        // Check for existing backup
        //fs::path pathBackup = pathConf + ".OLD";
        if (fs::exists(strNew.toStdString())) {
            QString strError = "You must first remove ";
            strError += strNew;
            strError += ".\n";
            messageBox.setText(strError);
            messageBox.exec();
            return false;
        }

        if (!QFile::rename(QString::fromStdString(pathConf.string()), strNew)) {
            QString strError = "Failed to backup old configuration file!\n";
            strError += "Remove existing config files and try again.\n";
            messageBox.setText(strError);
            messageBox.exec();
            return false;
        }

        // Make sure that we moved it
        if (fExists && fs::exists(pathConf)) {
            QString strError = "Failed to rename: ";
            strError += QString::fromStdString(pathConf.string());
            strError += "!\n";
            strError += "You must remove ";
            strError += QString::fromStdString(pathConf.string());
            strError += ".\n";
            messageBox.setText(strError);
            messageBox.exec();
            return false;
        }

    }

    // Now if we removed the old configuration or didn't find one we will write
    // a new mainchain configuration file.
    if (!fExists || !fMainchainConfigured) {
        QFile file(QString::fromStdString(pathConf.string()));
        if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QString strError = "Error while opening for write: ";
            strError += QString::fromStdString(pathConf.string());
            strError += "!\n";
            messageBox.setText(strError);
            messageBox.exec();
            return false;
        }

        // Write new mainchain configuration file
        QTextStream out(&file);
        out << "rpcuser=";
        out << strUser;
        out << "\n";
        out << "rpcpassword=";
        out << strPass;
        out << "\n";
        out << "server=1\n";
        out << "minerbreakforbmm=1\n";
        file.close();
    }

    // Write sidechain configuration file
    // Do we need to backup the old config file?
    fs::path pathConfSide = pathSide / "bitassets.conf";
    if (fs::exists(pathConfSide)) {
        // Rename old configuration file
        QString strNew = QString::fromStdString(pathConfSide.string());
        strNew += ".OLD";

        // Make sure that we moved it
        if (!QFile::rename(QString::fromStdString(pathConfSide.string()), strNew)) {
            QString strError = "You must first remove ";
            strError += strNew;
            strError += ".\n";
            messageBox.setText(strError);
            messageBox.exec();
            return false;
        }
    }

    if (fs::exists(pathConfSide)) {
        QString strError = "Failed to rename: ";
        strError += QString::fromStdString(pathConfSide.string());
        strError += "!\n";
        strError += "You must remove ";
        strError += QString::fromStdString(pathConf.string());
        strError += ".\n";
        messageBox.setText(strError);
        messageBox.exec();

        // We failed to rename the conf file
        return false;
    }

    // Write new configuration file
    QFile fileSide(QString::fromStdString(pathConfSide.string()));
    if (!fileSide.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QString strError = "Error while opening for write: ";
        strError += QString::fromStdString(pathConfSide.string());
        strError += "!\n";
        messageBox.setText(strError);
        messageBox.exec();
        return false;
    }

    QTextStream outSide(&fileSide);
    outSide << "rpcuser=";
    outSide << (fMainchainConfigured ? strMCUser : strUser);
    outSide << "\n";
    outSide << "rpcpassword=";
    outSide << (fMainchainConfigured ? strMCPass : strPass);
    outSide << "\n";

    fileSide.close();

    return true;
}
