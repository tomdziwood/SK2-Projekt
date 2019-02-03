/********************************************************************************
** Form generated from reading UI file 'gamefield.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_GAMEFIELD_H
#define UI_GAMEFIELD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTableWidget>

QT_BEGIN_NAMESPACE

class Ui_GameField
{
public:
    QTableWidget *tableWidget;
    QLabel *label;

    void setupUi(QDialog *GameField)
    {
        if (GameField->objectName().isEmpty())
            GameField->setObjectName(QString::fromUtf8("GameField"));
        GameField->resize(400, 473);
        tableWidget = new QTableWidget(GameField);
        if (tableWidget->columnCount() < 2)
            tableWidget->setColumnCount(2);
        if (tableWidget->rowCount() < 1)
            tableWidget->setRowCount(1);
        tableWidget->setObjectName(QString::fromUtf8("tableWidget"));
        tableWidget->setGeometry(QRect(20, 20, 341, 271));
        tableWidget->setRowCount(1);
        tableWidget->setColumnCount(2);
        tableWidget->horizontalHeader()->setDefaultSectionSize(20);
        tableWidget->horizontalHeader()->setMinimumSectionSize(20);
        tableWidget->verticalHeader()->setDefaultSectionSize(23);
        tableWidget->verticalHeader()->setMinimumSectionSize(20);
        label = new QLabel(GameField);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(110, 340, 47, 13));

        retranslateUi(GameField);

        QMetaObject::connectSlotsByName(GameField);
    } // setupUi

    void retranslateUi(QDialog *GameField)
    {
        GameField->setWindowTitle(QApplication::translate("GameField", "Dialog", nullptr));
        label->setText(QApplication::translate("GameField", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class GameField: public Ui_GameField {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_GAMEFIELD_H
