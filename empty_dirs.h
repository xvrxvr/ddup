#pragma once

#include "ui_empty_dirs.h"
#include "scan_thread.h"

#include <QDialog>
#include <QProgressBar>

class EmptyDirsDialog : public QDialog {
    Q_OBJECT;

    Ui::EmptyDirs ui;
public:
    EmptyDirsDialog();

    void fill(ScanThread*);

    void do_remove(QProgressBar*);

public slots:
    void on_btn_cancel_pressed() {reject(); }
    void on_btn_remove_pressed() {accept(); }
};
