#pragma once

#include <QtWidgets/QMainWindow>
#include <QProgressBar>

#include "ui_qdupfind.h"

#include "scan_thread.h"

enum FileNodeMode {
    FNM_None = 0,

    FNM_KeepManual      = 0x01,
    FNM_KeepAuto        = 0x02,
    FNM_Keep            = 0x03,

    FNM_DeleteManual    = 0x04,
    FNM_DeleteAuto      = 0x08,
    FNM_Delete          = 0x0C,

    FNM_Hide            = 0x10
};
Q_DECLARE_FLAGS(FileNodeModes, FileNodeMode)
Q_DECLARE_OPERATORS_FOR_FLAGS(FileNodeModes)


// Information about one File
struct FileInfo {
    QByteArray hash;
    FileNodeModes file_mode{ FNM_None };
    QTreeWidgetItem* item = NULL; // File entry in DirTree
};

using FilesMap = QMap<QString, FileInfo>; // <full file name> -> <file-info>
using FilePtr = FilesMap::iterator; // Pointer to FilesMap

class QDupFind : public QMainWindow
{
    Q_OBJECT

    ScanThread* scanner;
    QProgressBar* progress_bar;

    FilesMap all_files; // <file name> -> <file info>
    QMultiHash<QByteArray, FilePtr> files_by_hash; // <hash> -> <pointer to file in all_files>

    void add_error(QString);
    void sb_message(QString);

    QTreeWidgetItem* add_dir(QString path);

    QString tree_item_to_path(QTreeWidgetItem* item);

/*
    void set_file_mode(QString fname, FileNodeMode mode)
    {
        if (auto p = path_to_fnode(fname)) p->set_file_mode(fname, mode);
    }

    void set_file_mode(std::function<int(int)> mode_functor);
    void set_file_mode_all(FileNodeMode new_mode);
*/

    bool do_delete(QString);

    // Hide TreeList entry and returns 'true' if no more items to show in parent
//    bool remove_from_dir_tree(const QStringList& file_name, int index, DirTreeNode& root);

public:
    QDupFind(QWidget *parent = nullptr);
    ~QDupFind();


public slots:
    void scan_new_dup(QString, QByteArray);
    void scan_new_dir(QString dir) {ui.dir_to_process->setText(dir); }
    void scan_stat_update(ScanState event);
    void scan_error(QString msg) {add_error("Dir Scanner ERROR: " + msg); }

    void on_actionAdd_directory_triggered(bool);
//    void on_pause_cb_stateChanged(int state) { scanner->suspend_resume(ui.pause_cb, state);}

//    void on_dirs_itemDoubleClicked(QTreeWidgetItem* item, int column);

    void on_dirs_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_files_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);

/*
    void on_btn_keep_me_pressed()
    {
        set_file_mode([](int mode) {return FNM_Keep; });
        set_file_mode_all(FNM_Delete);
        move_to_next_file();
    }
    void on_btn_keep_pressed() { set_file_mode([](int mode) {return FNM_Keep; }); }
    void on_btn_remove_pressed() {set_file_mode([](int mode) {return FNM_Delete; });}
    void on_btn_invert_pressed() { set_file_mode([](int mode) {return (mode + 1) % 3; }); }

    void on_btn_auto_pressed();
    void on_btn_apply_pressed();
*/

private:
    Ui::QDupFindClass ui;
};
