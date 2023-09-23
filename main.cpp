#include "stdafx.h"
#include "qdupfind.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QDupFind w;
    w.show();
    return a.exec();
}
