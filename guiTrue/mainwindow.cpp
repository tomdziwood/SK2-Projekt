#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gamefield.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    tcpSocket(new QTcpSocket(this)), //dopis
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(tcpSocket, &QIODevice::readyRead, this, &MainWindow::readData); //dopis
    tcpSocket->connectToHost("localhost", 61596);
    //char * buffer;
    //this->tcpSocket->readAll();
    //ui->label->setText(tcpSocket->read(3));

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::connected()
{

}

//tworzymy pokoj
void MainWindow::on_pushButton_2_clicked()
{

    QString textName = ui->lineEdit->text();
    //QString textPort = ui->lineEditPort->text();
    if(textName.length() == 0) return;
    QString code = "1 " + textName;
    this->tcpSocket->write(code.toLocal8Bit());
    //ui->listWidget->addItem(tcpSocket->read(4));

}

void MainWindow::on_pushButton_clicked()
{

    QString string = ui->listWidget->currentItem()->text();
    if(string==Q_NULLPTR || string.length()==0) return;
    /*int doubledot=2;
    for(int i=0;i<string.length();i++) if(string[i]==':') {doubledot=i; break;}
    QString adres = string.mid(0,doubledot);
    int port = string.mid(doubledot+1,string.length()).toInt();
    ui->label->setText(adres);
    tcpSocket->connectToHost(adres, port);*/
    GameField *g = new GameField;
    g->setAtribute(string);
    g->setWindowTitle(string);
    g->show();
}

void MainWindow::readData()
{
    //QByteArray txt = tcpSocket->read(20);
    //ui->textBro1->append(txt);
}

void MainWindow::on_buttonRefresh_clicked()
{
    //this->tcpSocket->readLine();
    //this->tcpSocket->readLine();
    //this->tcpSocket->write("02");
    //if(tcpSocket->canReadLine()) ui->listWidget->addItem("tak");
    //else ui->listWidget->addItem("nie");
    ui->listWidget->addItem(tcpSocket->readLine());
}
