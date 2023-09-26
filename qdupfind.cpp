#include "stdafx.h"

#include <QFileDialog>

#include "qdupfind.h"
#include "empty_dirs.h"

// Define to fake Delete cycle (tool will be just print 'deleted ...' message in Eror pane
#define DEL_DRYRUN 0

// Define to add messages about delete/keep action during AutoDir pass
#define AUT_VERBOSE 0

QDupFind::QDupFind(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    ui.dirs->set_buddy(ui.prio);

    ui.errors_box->hide();
//    ui.files_box->hide();

    scanner = new ScanThread(this);

    connect(scanner, &ScanThread::new_dir, this, &QDupFind::scan_new_dir, Qt::BlockingQueuedConnection);
    connect(scanner, &ScanThread::new_dup, this, &QDupFind::scan_new_dup, Qt::QueuedConnection);
    connect(scanner, &ScanThread::stat_update, this, &QDupFind::scan_stat_update, Qt::QueuedConnection);
    connect(scanner, &ScanThread::error, this, &QDupFind::scan_error, Qt::QueuedConnection);

//    new QShortcut(Qt::Key_Space, ui.files, [this]() {on_btn_invert_pressed();}, Qt::WidgetShortcut);
//    new QShortcut(Qt::Key_Delete, ui.files, [this]() {on_btn_remove_pressed();}, Qt::WidgetShortcut);
//    new QShortcut(Qt::Key_Insert, ui.files, [this]() {on_btn_keep_me_pressed();}, Qt::WidgetShortcut);

    progress_bar = new QProgressBar();
    progress_bar->hide();
    statusBar()->addPermanentWidget(progress_bar);

    scanner->start();
}

QDupFind::~QDupFind()
{
    scanner->terminate();
}

QIcon QDupFind::get_icon(FileNodeModes mode)
{
    FileNodeModes normilized = FNM_None;
    if (mode & FNM_KeepDup) normilized = FNM_KeepDup; else
    if (mode & FNM_Keep) normilized = FNM_Keep; else
    if (mode & FNM_Delete) normilized = FNM_Delete;
    if (mode & FNM_AddFileIcon) normilized |= FNM_AddFileIcon;

    if (!normilized) return {};

    if (icons.contains(normilized)) return icons[normilized];

    QIcon result;
    if (normilized & FNM_KeepDup) result = QIcon(":/QDupFind/ok2"); else
    if (normilized & FNM_Keep) result = style()->standardIcon(QStyle::SP_DialogApplyButton); else
    if (normilized & FNM_Delete) result = style()->standardIcon(QStyle::SP_DialogCancelButton); else
    if (normilized & FNM_AddFileIcon) result = style()->standardIcon(QStyle::SP_FileIcon);
/*
    if ((normilized & FNM_AddFileIcon) && normilized != FNM_AddFileIcon)
    {
        QIcon file_icon = style()->standardIcon(QStyle::SP_FileIcon);
        QIcon new_icon;

        QPixmap result_pixmap(128*2 + 5, 128);
        QPainter painter(&result_pixmap);
        file_icon.paint(&painter, 0, 0, 128, 128);
        result.paint(&painter, 128+5, 0, 128, 128);
        new_icon.addPixmap(result_pixmap);

        result = new_icon;
    }
*/
    icons[normilized] = result;
    return result;
}


void QDupFind::add_error(QString msg)
{
    ui.errors->append(msg);
    ui.errors_box->show();
}

void QDupFind::sb_message(QString msg)
{
    QStatusTipEvent tip(msg);
    QCoreApplication::sendEvent(this, &tip);
}

void QDupFind::scan_stat_update(ScanState event)
{
    QString msg = QString("File dups: %1/%3").arg(event.total_dups).arg(event.total_files);
    if (event.total_false_dups) msg += QString(" | False dups: %1").arg(event.total_false_dups);
    msg += QString(" | Dirs: %1/%2").arg(event.total_dirs - event.dirs_to_proceed).arg(event.total_dirs);
    sb_message(msg);

    if (event.total_dirs || event.dirs_to_proceed)
    {
        progress_bar->setMaximum(event.total_dirs);
        progress_bar->setValue(event.total_dirs - event.dirs_to_proceed);
        progress_bar->show();
    }
}

void QDupFind::on_actionAdd_directory_triggered(bool)
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select directory to scan");
    if (!dir.isEmpty()) scanner->scan_dir(dir);
}

static QTreeWidgetItem* find_in_children(QTreeWidgetItem* root, QString text)
{
    for (int i = 0; i < root->childCount(); ++i)
    {
        auto child = root->child(i);
        if (child->text(0) == text) return child;
    }
    return NULL;
}

QTreeWidgetItem* QDupFind::add_dir(QString path)
{
    QTreeWidgetItem* root = ui.dirs->invisibleRootItem();
    auto path_list = path.split("/", Qt::SkipEmptyParts);
    size_t idx = 0;
    for(const auto& ent: path_list)
    {
        QTreeWidgetItem* item = find_in_children(root, ent);
        if (!item)
        {
            item = new QTreeWidgetItem(QStringList(ent));
            if (idx + 1 != path_list.size()) item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon)); 
            else item->setIcon(0, get_icon(all_files[path].file_mode | FNM_AddFileIcon));
            root->insertChild(0, item);
        }
        root = item;
        ++idx;
    }
    return root;
}

void QDupFind::scan_new_dup(QString fname, QByteArray hash)
{
   auto wg = add_dir(fname);

   auto ptr = all_files.insert(fname, FileInfo{
        .hash = hash,
        .file_mode = FNM_None,
        .item = wg
   });
   files_by_hash.insert(hash, ptr);
}

void QDupFind::set_file_mode(QString fname, FileNodeModes mode)
{
    assert(all_files.contains(fname));
    auto& ent = all_files[fname];

    if (ent.file_mode & FNM_Hide) return;

    auto total = classify(ent.hash, {FNM_Delete | FNM_KeepDup}, fname);

    if (mode & FNM_DeleteManual && !ui.actionEnable_full_delete->isChecked()) // Verify that we do not delete all alternatives
    {
        if (total[2] + 1 == total[0]) return; // We delete all (Intended Dups not taken in account) - reject
    }

    ent.file_mode = mode;
    ent.item->setIcon(0, get_icon(ent.file_mode | FNM_AddFileIcon));

    int idx=0;
    while(auto wg = ui.files->item(idx++))
    {
        if (wg->text() == fname)
        {
            wg->setIcon(get_icon(ent.file_mode));
            break;
        }
    }

    if (ui.actionAuto_complete->isChecked() && mode & (FNM_DeleteManual | FNM_KeepManual | FNM_KeepDup))
    {
        auto total = classify(ent.hash, {FNM_Delete | FNM_KeepDup});
        if (total[1] == 1 && total[2] + 1 == total[0]) // We delete (or make intended Dup) all but 1 unassigned entry - make it Keep
        {
            auto range = files_by_hash.equal_range(ent.hash);
            for (auto iter = range.first; iter != range.second; ++iter)
            {
                if (!iter.value()->file_mode)
                {
                    set_file_mode(iter.value().key(), FNM_KeepManual);
                    break;
                }
            }
        }
    }
}

void QDupFind::hide_file(QString fname)
{
    assert(all_files.contains(fname));
    auto& ent = all_files[fname];

    ent.file_mode |= FNM_Hide;
    if (ui.actionShow_processed_entries->isChecked()) return;

    hide_dir_item(ent.item);
}

void QDupFind::hide_dir_item(QTreeWidgetItem* root)
{
    root->setHidden(true);
    root = root->parent();
    for (;root; root = root->parent())
    {
        for(int ch_idx=0; ch_idx < root->childCount(); ++ch_idx)
        {
            if (!root->child(ch_idx)->isHidden()) return;
        }
        root->setHidden(true);
    }
}

void QDupFind::hide_dir_tree()
{
    for(const auto& ent: all_files)
    {
        if (ent.file_mode & FNM_Hide) hide_dir_item(ent.item);
    }
}

void QDupFind::show_dir_tree()
{
    for (const auto& ent : all_files)
    {
        if (ent.file_mode & FNM_Hide)
        {
            for(auto root = ent.item; root; root=root->parent())
            {
                if (!root->isHidden()) break;
                root->setHidden(false);
            }
        }
    }
}

void QDupFind::set_file_mode_all(QByteArray hash, FileNodeModes new_mode)
{
    auto range = files_by_hash.equal_range(hash);
    for(auto iter=range.first; iter!=range.second; ++iter)
    {
        if (!iter.value()->file_mode) set_file_mode(iter->key(), new_mode);
    }
}

QString QDupFind::get_current_file_name()
{
    QString file;
    if (ui.dirs->hasFocus()) file = tree_item_to_path(ui.dirs->currentItem()); else
    if (ui.files->hasFocus()) 
    {
        if (auto p = ui.files->currentItem()) file = p->text(); else
        if (auto p = ui.files->selectedItems(); !p.isEmpty()) file = p[0]->text();
        else return {};
    }
    else return {};
    if (!all_files.contains(file)) return {};
    return file;
}

void QDupFind::set_current_file_mode(FileNodeModes new_mode)
{
    QString file = get_current_file_name();
    if (!file.isEmpty()) set_file_mode(file, new_mode);
}

void QDupFind::on_actionKeep_me_triggered(bool)
{
    QString file = get_current_file_name();
    if (file.isEmpty()) return;
    set_file_mode(file, FNM_KeepManual);
    set_file_mode_all(all_files[file].hash, FNM_DeleteManual);
}

void QDupFind::on_actionKeep_other_triggered(bool)
{
    QString file = get_current_file_name();
    if (file.isEmpty()) return;
    set_file_mode(file, FNM_DeleteManual);

    auto range = files_by_hash.equal_range(all_files[file].hash);
    QString fname;
    int count = 0;

    for (auto iter = range.first; iter != range.second && count < 2; ++iter)
    {
        if (!iter.value()->file_mode) {fname = iter.value().key(); ++count;}
    }
    if (count == 1) set_file_mode(fname, FNM_KeepManual);
}

void QDupFind::on_actionInvert_triggered(bool)
{
    QString file = get_current_file_name();
    if (file.isEmpty()) return;
    auto file_mode = all_files[file].file_mode;
    
    if (file_mode & FNM_Hide) return;

    if (!file_mode) file_mode = FNM_KeepManual; else
    if (file_mode & (FNM_Keep | FNM_KeepDup)) file_mode = FNM_DeleteManual; else
    if (file_mode & FNM_Delete) file_mode = FNM_None;
    
    set_file_mode(file, file_mode);
}

void QDupFind::on_actionKeep_as_intended_duplicate_triggered(bool)
{
    QString file = get_current_file_name();
    if (!file.isEmpty()) {set_file_mode(file, FNM_KeepDup); return;}
    if (!ui.dirs->hasFocus()) return;
    set_file_mode_rec(ui.dirs->currentItem(), FNM_KeepDup);
}

void QDupFind::set_file_mode_rec(QTreeWidgetItem* root, FileNodeModes mode)
{
    QString file = tree_item_to_path(root);
    if (all_files.contains(file)) { set_file_mode(file, mode); return; }
    for(int idx=0; idx<root->childCount(); ++idx)
    {
        set_file_mode_rec(root->child(idx), mode);
    }
}

void QDupFind::on_dirs_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem*)
{
    QSignalBlocker b(ui.dirs);
    QSignalBlocker b2(ui.files);

    ui.files->clear();
    if (!current || current->isHidden()) return;
    auto path = tree_item_to_path(current);
    auto org_ptr = all_files.find(path);
    if (org_ptr == all_files.end()) return;
    auto range = files_by_hash.equal_range(org_ptr->hash);
    for (auto iter = range.first; iter != range.second; ++iter)
    {
        auto file_mode = iter.value()->file_mode;
        if (!ui.actionShow_processed_entries->isChecked())
        {
            if (file_mode & FNM_Hide) continue;
        }
        auto icon = get_icon(file_mode);
        auto wg = new QListWidgetItem(icon, iter->key(), ui.files);
        if (*iter == org_ptr) wg->setSelected(true);
    }
}

void QDupFind::on_files_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    QSignalBlocker b(ui.dirs);
    QSignalBlocker b2(ui.files);

    if (!current) return;
    auto path = current->text();
    if (!all_files.contains(path)) return;
    ui.dirs->setCurrentItem(all_files[path].item);
}

bool QDupFind::do_delete(QString fname)
{
#if DEL_DRYRUN
    add_error("Delete " + fname);
    return true;
#else
    return QDir().remove(fname) && QFileInfo(fname).absoluteDir().rmpath(".");
#endif
}

void QDupFind::on_actionRun_triggered(bool)
{
    WaitCursor wc;
    for (const auto& [file_name, ent] : all_files.asKeyValueRange())
    {
        if (ent.file_mode & FNM_Hide) continue;
        if (ent.file_mode & (FNM_Keep | FNM_KeepDup))
        {
            if (is_all_assigned(ent.hash)) hide_file(file_name); 
        }
        else if ((ent.file_mode & FNM_Delete) && do_delete(file_name))
        {
            hide_file(file_name);
            scanner->remove_file(file_name, ent.hash);
        }
    }
    ui.files->clear();
    on_dirs_currentItemChanged(ui.dirs->currentItem(), NULL);
}

void QDupFind::on_actionShow_processed_entries_triggered(bool show)
{
    if (show) show_dir_tree();
    else hide_dir_tree();
    ui.files->clear();
    on_dirs_currentItemChanged(ui.dirs->currentItem(), NULL);
}
//////////////////////

void PrioDirTree::add_dir(DirNode& root, const QStringList& path, int index, int priority)
{
    auto ptr = root.children.insert(path[index], {});
    if (index + 1 < path.size()) add_dir(ptr.value(), path, index+1, priority);
    else ptr->priority = priority;
}

int PrioDirTree::get_dir(DirNode& root, const QStringList& path, int index, int parent_priority)
{
    auto ptr = root.children.find(path[index]);
    if (ptr == root.children.end()) return parent_priority;
    if (ptr->priority != NoPrio) parent_priority = ptr->priority;
    if (index + 1 >= path.size()) return parent_priority;
    return get_dir(ptr.value(), path, index+1, parent_priority);
}
///////////////////////

void QDupFind::on_actionAuto_by_Dirs_triggered(bool)
{
    WaitCursor wc;
    PrioDirTree prio;

    int cur_prio = ui.prio->count();
    for (int i = 0; i < ui.prio->count(); ++i)
    {
        QString text = ui.prio->item(i)->text();
        if (text == "<default>") {cur_prio = -1; continue;}
        prio.add_dir(text, cur_prio--);
    }
    if (prio.empty() || files_by_hash.empty()) {sb_message("AutoDir: No priorities or files - nothing to process"); return;}

    QByteArray cur_hash = files_by_hash.begin().key();
    for (;;)
    {
        auto range = files_by_hash.equal_range(cur_hash);
        if (range.first == range.second) break;
        process_prio_range(prio, range.first, range.second);
        if (range.second == files_by_hash.end()) break;
        cur_hash = range.second.key();
    }
    sb_message(QString("AutoDir: Processed %1 file(s) in %2 bundles").arg(prio.total_files).arg(prio.total_hashes));
}

void QDupFind::process_prio_range(PrioDirTree& prio_tree, HashPtr begin, HashPtr end)
{
    int max_keep_priority = std::numeric_limits<int>::min();
    QMultiMap<int, FilePtr> prio_list;

    for (auto iter = begin; iter != end; ++iter)
    {
        const auto ptr = iter.value();
        if (ptr->file_mode & FNM_KeepManual)
        {
            max_keep_priority = std::max(max_keep_priority, prio_tree.get_dir(iter.value().key()));
        }
        if (ptr->file_mode & (FNM_KeepManual|FNM_KeepDup|FNM_DeleteManual|FNM_Hide)) continue;
        int prio = prio_tree.get_dir(iter.value().key());
        prio_list.insert(prio, ptr);
    }

    if (prio_list.isEmpty()) return;
    int active_prio = prio_list.lastKey();
    int total_last_prio = prio_list.count(active_prio);

    ++prio_tree.total_hashes;

    enum Action {
        Keep = FNM_KeepAuto,
        Discard = FNM_DeleteAuto,
        Terminate = 10000,
    } action;

    if (max_keep_priority == active_prio) action = Discard; else // If we have at least one already 'Keep' file at maximum priority - use it and discard all other
    if (total_last_prio == 1) action = Keep; // If we have exactly one file at maximum priority - keep it
    else action = Terminate; // More than one - let's user decide (just stop processing)

    for (const auto& e : prio_list.asKeyValueRange())
    {
        if (e.first < active_prio) 
        {
            set_file_mode(e.second.key(), FNM_DeleteAuto); 
#if AUT_VERBOSE
            add_error(QString("File %1 deleted").arg(e.second.key()));
#endif
        }
        else if (action == Terminate) break;
        else 
        {
            set_file_mode(e.second.key(), FileNodeMode(action)); 
#if AUT_VERBOSE
            add_error(QString("File %1 %2").arg(e.second.key()).arg(action == Keep ? "saved" : "deleted"));
#endif
        }
        ++prio_tree.total_files;
    }
}

void QDupFind::on_actionScan_for_Empty_dirs_triggered(bool)
{
    EmptyDirsDialog dlg;

    dlg.fill(scanner);

    if (dlg.exec() == QDialog::Accepted)
    {
        sb_message("Removing files ...");
        dlg.do_remove(progress_bar);
        sb_message("");
    }
}
