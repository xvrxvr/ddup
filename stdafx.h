#pragma once

#include <stdint.h>

#include <QtWidgets>
#include <QMap>
#include <QSet>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QMultiHash>

struct WaitCursor {
    WaitCursor() { QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor)); }
    ~WaitCursor() { QGuiApplication::restoreOverrideCursor(); }
};

enum FileNodeMode {
    FNM_None = 0,

    FNM_KeepManual = 0x01,
    FNM_KeepAuto = 0x02,
    FNM_Keep = 0x03,

    FNM_KeepDup = 0x04,

    FNM_DeleteManual = 0x08,
    FNM_DeleteAuto = 0x10,
    FNM_Delete = 0x18,

    FNM_Hide = 0x20,

    FNM_AddFileIcon = 0x0100,   // Used internally by get_icon member, not used in FileInfo::file_mode

    FNM_KeepMe      = 0x1000, // Used only as action in Find dialog
    FNM_KeepOther   = 0x2000
};
Q_DECLARE_FLAGS(FileNodeModes, FileNodeMode)
Q_DECLARE_OPERATORS_FOR_FLAGS(FileNodeModes)

inline QStringList get_list(QListWidget* lst)
{
    QStringList result;
    for (int idx = 0; idx < lst->count(); ++idx)
    {
        auto item = lst->item(idx);
        if (item->checkState() == Qt::Checked) result << item->text();
    }
    return result;
}
