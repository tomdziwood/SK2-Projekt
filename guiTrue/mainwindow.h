#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include "klient_saper.h"


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
    void on_pushButton_2_clicked();

    void on_pushButton_clicked();

    void on_buttonRefresh_clicked();

    void connected();

//public slots:
    void readData();

private:
    Ui::MainWindow *ui;
    QTcpSocket *tcpSocket;
    Klient_saper * klientSaper;
};

#endif // MAINWINDOW_H
