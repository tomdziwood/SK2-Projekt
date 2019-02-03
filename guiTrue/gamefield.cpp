#include "gamefield.h"
#include "ui_gamefield.h"

GameField::GameField(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GameField)
{
    ui->setupUi(this);
    int column_number = 3;
    int vertex_number = 3;
    //int map[3][3] = {-1,0,0,0,-1,-1,0,0,0};
    /*int ** map = new int * [3];
    for(int i=0;i<3;i++)
    {
        map[i]=new int [3];
        for(int j=0;i<3;j++) map[i][j]=0;
    }
    map[0][0]=-1;*/
    this->setBombMap(vertex_number,column_number);
    this->qtableItems = new QTableWidgetItem ** [vertex_number];
    for(int i=0;i<vertex_number;i++) qtableItems[i] = new QTableWidgetItem * [column_number];
    for(int i=0;i<vertex_number;i++) for (int j=0;j<column_number;j++) qtableItems[i][j] = new QTableWidgetItem();

    ui->label->setText(QString::number(this->bombMap[0][0]));
    //qDebug() << "App path : " << qApp->applicationDirPath();
     //ui->label->setText(QDir::currentPath());
    ui->tableWidget->setColumnCount(column_number);
    ui->tableWidget->setRowCount(vertex_number);
    ui->tableWidget->setIconSize(QSize(100, 100));
    for (int ridx = 0 ; ridx < vertex_number ; ridx++ )
    {

        for (int cidx = 0 ; cidx < column_number ; cidx++)
        {
          //ui->tableWidget->setColumnWidth(cidx,100);
          //QTableWidgetItem* item = new QTableWidgetItem();
          QTableWidgetItem* item = this->qtableItems[ridx][cidx];

          //QIcon icon("guiTrue\\pola\\empty.jpg");
          //item->setSizeHint(QSize(100, 100));
          QIcon icon("D:\\qtySK\\guiTrue\\pola\\empty.jpg");
          item->setIcon(icon);

          ui->tableWidget->setItem(ridx,cidx,item);
        }
    }
    //ui->tableWidget->setMaximumSize(ui->tableWidget->maximumSize());

}


int ** chooseBombMap(int vertex, int column, int probability)
{
    int ** bombMap = new int * [vertex];
    for(int i=0;i<vertex;i++) bombMap[i] = new int [column];
    //
    return bombMap;
}

GameField::~GameField()
{
    delete ui;
}

void GameField::setAtribute(QString atribute)
{
    this->atribute = atribute;
}

void GameField::on_tableWidget_cellClicked(int row, int column)
{
    ui->label->setText(QString::number(row));

    switch (this->bombMap[row][column]) {
        case -1:
            QTableWidgetItem* item = this->qtableItems[row][column];
            QIcon icon("D:\\qtySK\\guiTrue\\pola\\bomb.jpg");
            item->setIcon(icon);
            ui->tableWidget->setItem(row,column,item);
            break;
    }
}

void GameField::setBombMap(int vertex, int column)
{
    this->bombMap = new int * [vertex];
    for(int i=0;i<vertex;i++) bombMap[i] = new int [column];
    for(int i=0;i<vertex;i++) for(int j=0;j<column;j++) this->bombMap[i][j]=0;
    this->bombMap[0][0]=-1;
    this->bombMap=bombMap;
}
