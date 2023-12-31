#pragma once

#include <QtWidgets/QMainWindow>
#include <QProgressBar>

#include "ui_qdupfind.h"

#include "scan_thread.h"

// Information about one File
struct FileInfo {
    QByteArray hash;
    FileNodeModes file_mode{ FNM_None };
    QTreeWidgetItem* item = NULL; // File entry in DirTree
};

using FilesMap = QMap<QString, FileInfo>; // <full file name> -> <file-info>
using FilePtr = FilesMap::iterator; // Pointer to FilesMap

class PrioDirTree {
    static constexpr const int NoPrio = std::numeric_limits<int>::max();

    struct DirNode {
        int priority = NoPrio;
        QMap<QString, DirNode> children;
    };
    DirNode root;

    void add_dir(DirNode& root, const QStringList& path, int index, int priority);
    int get_dir(DirNode& root, const QStringList& path, int index, int parent_priority);

public:
    void add_dir(QString path, int priority) {add_dir(root, path.split("/", Qt::SkipEmptyParts), 0, priority); }
    int get_dir(QString path) {return get_dir(root, path.split("/", Qt::SkipEmptyParts), 0, 0); }

    bool empty() const {return root.children.isEmpty(); }

    // Just put them here. This class do not use it, but outside code use as temporary storage
    int total_hashes = 0;
    int total_files = 0;
};


class QDupFind : public QMainWindow
{
    Q_OBJECT

    ScanThread* scanner;
    QProgressBar* progress_bar;
    QLabel* dir_queue_pending;
    QLabel* time;


    FilesMap all_files; // <file name> -> <file info>
    QMultiHash<QByteArray, FilePtr> files_by_hash; // <hash> -> <pointer to file in all_files>
    using HashPtr = QMultiHash<QByteArray, FilePtr>::iterator;

    QHash<FileNodeModes, QIcon> icons;

    QTime start_of_scan;

    // We use dir_tree_cache to hold delayed update info for ui.dirs
    // This structure will not used for navigation
    struct DirTreeNode {
        QTreeWidgetItem* item = NULL;
        bool is_dir = true;
        bool pending_insert = true;
        bool follow_children = true;
        QMap<QString, DirTreeNode> children;
    };
    DirTreeNode dir_tree_cache;
    QTime dir_tree_last_update = QTime::currentTime();
    size_t dir_tree_added_items = 0;

    static const int DirTreeMaxItems = 1000; // How many items can be queied
    static const int DirTreeMaxTimeout = 500; // How long (in ms) dir tree update can be held

    QTreeWidgetItem* add_dir_node_to_cache(QString);
    void dir_node_flush(bool force = false);
    void dir_node_flush(DirTreeNode&, QTreeWidgetItem* root_item);

    QVector<int> classify(QByteArray hash, std::initializer_list<FileNodeModes> filter, QString ignore = {})
    {
        QVector<int> result(2 + filter.size());
        auto range = files_by_hash.equal_range(hash);
        for (auto iter = range.first; iter != range.second; ++iter)
        {
            ++result[0];
            if (iter.value().key() == ignore) continue;
            auto fm = iter.value()->file_mode;
            if (!fm) {++result[1]; continue;}
            int idx = 2;
            for(auto tst: filter)
            {
                if (fm & tst) ++result[idx]; else
                if (!(tst & FNM_Hide) && (fm & FNM_Hide)) break; // If we do not test for Hide but current item is Hiddeen - stop processing futher filter items
                ++idx;
            }
        }
        return result;
    }


    bool is_all_assigned(QByteArray hash) {return classify(hash, {})[1] == 0; }

    void add_error(QString);
    void sb_message(QString);

    QTreeWidgetItem* add_dir(QString path);

    static QString tree_item_to_path(QTreeWidgetItem* item) {return XDirTree::tree_item_to_path(item);}

    QIcon get_icon(FileNodeModes mode);

    void set_file_mode(QString fname, FileNodeModes mode);
    void set_file_mode_rec(QTreeWidgetItem* root, FileNodeModes);
    void hide_file(QString fname);
    void hide_dir_item(QTreeWidgetItem*);
    void hide_dir_tree();
    void show_dir_tree();

    void set_file_mode_all(QByteArray hash, FileNodeModes new_mode);

    QString get_current_file_name();
    void set_current_file_mode(FileNodeModes new_mode);

    bool do_delete(QString);

    void process_prio_range(PrioDirTree&, HashPtr begin, HashPtr end);

    void keep_me(QString file);
    void keep_other(QString file);


public:
    QDupFind(QWidget *parent = nullptr);
    ~QDupFind();


public slots:
    void scan_new_dup(QString, QByteArray);
    void scan_new_dir(QString dir) {ui.dir_to_process->setText(dir); }
    void scan_stat_update(ScanState event);
    void scan_error(QString msg) {add_error("Dir Scanner ERROR: " + msg); }

    void on_actionAdd_directory_triggered(bool);
    void on_actionProcess_by_mask_triggered(bool);

    void on_dirs_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_files_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);

    void on_actionScan_for_Empty_dirs_triggered(bool);
    void on_actionAuto_by_Dirs_triggered(bool);
    void on_actionRun_triggered(bool);
    void on_actionPause_triggered(bool checked) {scanner->suspend_resume(ui.actionPause, checked);}
    void on_actionShow_processed_entries_triggered(bool);

    void on_actionKeep_me_triggered(bool) { keep_me(get_current_file_name()); }
    void on_actionKeep_other_triggered(bool) {keep_other(get_current_file_name()); }
    void on_actionKeep_triggered(bool) { set_current_file_mode(FNM_Keep); }
    void on_actionKeep_as_intended_duplicate_triggered(bool);
    void on_actionRemove_triggered(bool) { set_current_file_mode(FNM_Delete); }
    void on_actionInvert_triggered(bool);

private:
    Ui::QDupFindClass ui;
};
