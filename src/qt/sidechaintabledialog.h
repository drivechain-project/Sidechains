#ifndef SIDECHAINTABLEDIALOG_H
#define SIDECHAINTABLEDIALOG_H

#include <QDialog>

class SidechainEscrowTableModel;

namespace Ui {
class SidechainTableDialog;
}

class SidechainTableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SidechainTableDialog(QWidget *parent = 0);
    ~SidechainTableDialog();

private Q_SLOTS:
    void on_pushButtonRefresh_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonRunSimulation_clicked();
    void on_pushButtonReset_clicked();

private:
    Ui::SidechainTableDialog *ui;
    SidechainEscrowTableModel *sidechainTableModel;

};

#endif // SIDECHAINTABLEDIALOG_H
