#pragma once

#include <stdint.h>

#include <QtWidgets>
#include <QMap>
#include <QSet>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>

struct WaitCursor {
    WaitCursor() { QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor)); }
    ~WaitCursor() { QGuiApplication::restoreOverrideCursor(); }
};
