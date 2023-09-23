#pragma once

#include <QtWidgets/QMainWindow>
#include <QProgressBar>

#include "ui_qdupfind.h"

#include "scan_thread.h"

struct DirTreeNode {
    QMap<QString, DirTreeNode> children;
    int priority = -1;
    QTreeWidgetItem* item = NULL;

    QString get_visual_name(int parent_priority, QString raw_name);
    void update_visual_widget(int parent_priority);
};

enum FileNodeMode {
    FNM_Keep = 1,
    FNM_Delete = 2,
    FNM_Hide = 4
};

struct FileNode {
    struct DirNode {
        QTreeWidgetItem* item = NULL;
        int entry_mode = 0; // bitset of FileNodeMode
    };
    
    QString file_name; // Keeps common file name or all file names delimitered by '|'
    QTreeWidgetItem* item = NULL;
    QMap<QString, DirNode> dirs; // <full-file-name> -> <DirNode>

    void add_dir_node(QString file_name, QTreeWidget*);

    void set_file_mode(QString fname, FileNodeMode mode);

private:
    bool mixed_file_names = false; // Set to 'true' if file names in 'dirs' differs
    void to_mixed_file_name();
    void update_file_name_in_widget();
    enum FilledContents {
        FC_None,
        FC_Some,
        FC_All
    };
    FilledContents check_filled() const;
};

class QDupFind : public QMainWindow
{
    Q_OBJECT

    ScanThread* scanner;
    QProgressBar* progress_bar;

    DirTreeNode dir_tree_root;

    QMap<QByteArray, QString> dups_hash_toc; // <hash> -> <initial visual string for file> (filename + ' ' + hash-in-hex)
    QMap<QString, QByteArray> dups_backrefs; // <full-file-name> -> <hash>
    QMap<QString, FileNode> dups_visual; // <initial visual string for file> -> <dup-file-record>. Real visual in Widget can be differ from initial

    QString cur_dir_selected; // Current directory selected in 'Dirs' TreeWidget (for pushing back Priority)
    bool suppress_new_prio_value = false; // Do not update 'Priority' in SpinBox callback

    void add_error(QString);
    void sb_message(QString);

    std::pair<DirTreeNode*, int> add_dir(QStringList path, int trim = 0, bool set_icons=true);
    std::pair<DirTreeNode*, int> add_dir(QString path) {return add_dir(path.split("/", Qt::SkipEmptyParts));}

    QString tree_item_to_path(QTreeWidgetItem* item);

    FileNode* path_to_fnode(QString fname);

    void set_file_mode(QString fname, FileNodeMode mode)
    {
        if (auto p = path_to_fnode(fname)) p->set_file_mode(fname, mode);
    }

    void set_file_mode(std::function<int(int)> mode_functor);
    void set_file_mode_all(FileNodeMode new_mode);
    void move_to_next_file();

    bool do_delete(QString);

    // Hide TreeList entry and returns 'true' if no more items to show in parent
    bool remove_from_dir_tree(const QStringList& file_name, int index, DirTreeNode& root);

public:
    QDupFind(QWidget *parent = nullptr);
    ~QDupFind();


public slots:
    void scan_new_dup(QString, QByteArray);
    void scan_new_dir(QString dir) {ui.dir_to_process->setText(dir); }
    void scan_stat_update(ScanState event);
    void scan_error(QString msg) {add_error("Dir Scanner ERROR: " + msg); }

    void on_actionAdd_directory_triggered(bool);
    void on_pause_cb_stateChanged(int state) { scanner->suspend_resume(ui.pause_cb, state);}

    void on_dirs_itemDoubleClicked(QTreeWidgetItem* item, int column);
    void on_files_itemDoubleClicked(QTreeWidgetItem* item, int column);

    void on_dirs_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_priority_spin_valueChanged(int i);

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

private:
    Ui::QDupFindClass ui;
};
