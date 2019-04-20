#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "zipscanner.h"
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->edInputFolder->setText(qApp->applicationDirPath());
    ui->edResultFile->setText(qApp->applicationDirPath() + QDir::separator() + "result.txt");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnBrowse_pressed()
{
    QString res = QFileDialog::getExistingDirectory(
                this,
                tr("Select input directory"),
                qApp->applicationDirPath(),
                QFileDialog::ShowDirsOnly
                );
    if (res.isEmpty() && ui->edInputFolder->text().isEmpty()) {
        res = qApp->applicationDirPath();
    }
    ui->edInputFolder->setText(res);
}

void MainWindow::on_btnBrowseResult_pressed()
{
    QString res = QFileDialog::getSaveFileName(
                this,
                tr("Select file to save result"),
                qApp->applicationDirPath(),
                tr("*.txt")
                );
    ui->edResultFile->setText(res);
}

void MainWindow::on_btnStart_clicked()
{
    ui->tbLog->clear();

    ZipScanner zs(ui->edInputFolder->text(),
                  ui->edFilemask->text(),
                  ui->cbSubdirs->isChecked(),
                  ui->cbRepack->isChecked(),
                  this);
    connect(&zs, &ZipScanner::log, this, &MainWindow::log);
    bool res = zs.startScan();

    if (!res) {
        QMessageBox::critical(this, QString(), tr("Errors were detected during the process! Please check the log.!"));
    }

    QFile fres(ui->edResultFile->text());
    if (fres.open(QIODevice::WriteOnly)) {
        fres.write(zs.result().join("\n").toStdString().c_str());
        fres.close();
        QMessageBox::information(this, QString(), tr("Done!"));
    } else {
        QMessageBox::critical(this, QString(), tr("Can't save the results file!"));
    }

}

void MainWindow::log(const QString& txt)
{
    ui->tbLog->append(txt);
}
