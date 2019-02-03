#ifndef GAMEFIELD_H
#define GAMEFIELD_H

#include <QDialog>
#include "ui_gamefield.h"


namespace Ui {
class GameField;
}

class GameField : public QDialog
{
    Q_OBJECT
private: QString atribute;
private: int ** bombMap;
private: QTableWidgetItem *** qtableItems;

public: void setAtribute(QString);
public: void setBombMap(int,int);
public:
    explicit GameField(QWidget *parent = nullptr);
    ~GameField();

private slots:
    void on_tableWidget_cellClicked(int row, int column);

private:
    Ui::GameField *ui;
private: struct room {
    char nazwa[256];
    int id, liczbaGraczy, gracze[16];
    int **plansza, **stanPlanszy, wysokoscPlanszy, szerokoscPlanszy;
    int liczbaMin, liczbaMinDoOznaczenia, stanGry, liczbaNieodkrytychPol;
};
};


#endif // GAMEFIELD_H
