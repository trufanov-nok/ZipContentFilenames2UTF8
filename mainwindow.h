#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnBrowse_pressed();

    void on_btnBrowseResult_pressed();

    void on_btnStart_clicked();

public slots:

    void log(const QString& txt);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
