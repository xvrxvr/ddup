#pragma once

#include "ui_find.h"

#include <QDialog>

class FindDialog : public QDialog {
    Q_OBJECT;

    Ui::FindDialog ui;

    std::function<bool(QString, QString)> aka_callback;
    std::function <void(QStringList, FileNodeMode)> action_callback;
    std::function<void(QString)> file_higlight_callback;

    QStringList files;

public:
    FindDialog(const QStringList& files) : files(files) {ui.setupUi(this);}

    // Callback for AKA testing. Arguments: <full file name>, <aka (also full name)>. Return true if both exists and belong to the same hash
    void set_aka_test(std::function<bool(QString, QString)> cb) {aka_callback = cb;}

    // Callback function for action. Arguments: list of files (full names), action
    void set_action_callback(std::function <void(QStringList, FileNodeMode)> cb) {action_callback = cb;}

    // File highlight callback
    void set_file_higlight_callback(std::function<void(QString)> cb) { file_higlight_callback  = cb;}

public slots:
    void on_btn_find_pressed();
    void on_btn_exec_pressed();
    void on_files_itemDoubleClicked(QListWidgetItem* item)
    {
        if (file_higlight_callback) file_higlight_callback(item->text());
    }

    void on_btn_cancel_pressed() {done(0); }
    void on_btn_close_pressed() {done(0); }
};