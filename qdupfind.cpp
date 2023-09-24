#include "stdafx.h"

#include <QFileDialog>

#include "qdupfind.h"

QDupFind::QDupFind(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

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
            item->setIcon(0, style()->standardIcon(idx + 1 == path_list.size() ? QStyle::SP_FileIcon : QStyle::SP_DirIcon));
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

#if 0
void FileNode::set_file_mode(QString fname, FileNodeMode mode)
{
    assert(dirs.contains(fname));
    auto org_fill = check_filled();
    auto& ent = dirs[fname];
    if (mode & FNM_Hide) ent.entry_mode |= FNM_Hide;
    else ent.entry_mode = mode;
    if (ent.entry_mode & FNM_Hide) ent.item->setHidden(true); else
    if (ent.entry_mode& FNM_Keep) ent.item->setIcon(0, ent.item->treeWidget()->style()->standardIcon(QStyle::SP_DialogApplyButton)); else
    if (ent.entry_mode & FNM_Delete) ent.item->setIcon(0, ent.item->treeWidget()->style()->standardIcon(QStyle::SP_DialogCancelButton)); 
    else ent.item->setIcon(0, {});
    auto now_fill = check_filled();
    if (org_fill != now_fill)
    {
        static const QStyle::StandardPixmap pxm[] = {QStyle::SP_FileIcon, QStyle::SP_FileLinkIcon, QStyle::SP_FileDialogContentsView};
        item->setIcon(0, ent.item->treeWidget()->style()->standardIcon(pxm[now_fill]));
    }

    if (!(mode & FNM_Hide)) return;

    for (const auto& ent : dirs)
    {
        if (!(ent.entry_mode & FNM_Hide)) return;
    }
    item->setHidden(true);
}

FileNode::FilledContents FileNode::check_filled() const
{
    int filled = 0;
    for (const auto& d : dirs)
    {
        if (d.entry_mode & FNM_Hide) continue;
        filled |= d.entry_mode ? 1 : 2;
    }
    static const FilledContents res[] = { FC_None, FC_All, FC_None, FC_Some };
    return res[filled];
}

void QDupFind::set_file_mode(std::function<int(int)> mode_functor)
{
    auto path = ui.files->currentItem()->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;
    FileNode* fnode = path_to_fnode(path);
    if (!fnode) return;
    auto& ent = fnode->dirs[path];
    if (ent.entry_mode & FNM_Hide) return;
    fnode->set_file_mode(path, FileNodeMode(mode_functor(ent.entry_mode)));
}

void QDupFind::set_file_mode_all(FileNodeMode new_mode)
{
    auto path = ui.files->currentItem()->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;
    FileNode* fnode = path_to_fnode(path);
    if (!fnode) return;
    for (const auto& ent : fnode->dirs.asKeyValueRange())
    {
        if (!ent.second.entry_mode) fnode->set_file_mode(ent.first, new_mode);
    }
}
#endif

QString QDupFind::tree_item_to_path(QTreeWidgetItem* item)
{
    QStringList acc;
    while (item)
    {
        acc << item->text(0);
        item = item->parent();
    }
    std::reverse(acc.begin(), acc.end());
    return acc.join("/");
}

void QDupFind::on_dirs_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem*)
{
    ui.files->clear();
    auto path = tree_item_to_path(current);
    auto org_ptr = all_files.find(path);
    if (org_ptr == all_files.end()) return;
    auto range = files_by_hash.equal_range(org_ptr->hash);
    for (auto iter = range.first; iter != range.second; ++iter)
    {
        auto wg = new QListWidgetItem(/*icon, */iter->key(), ui.files);
        if (*iter == org_ptr) wg->setSelected(true);
    }
}

void QDupFind::on_files_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current) return;
    auto path = current->text();
    if (!all_files.contains(path)) return;
    ui.dirs->setCurrentItem(all_files[path].item);
}


/*
void QDupFind::on_btn_auto_pressed()
{
    WaitCursor wc;
    for (auto& ent : dups_visual)
    {
        int max_keep_priority = -1;
        QMultiMap<int, QString> prio_list;

        for (auto iter = ent.dirs.begin(); iter != ent.dirs.end(); ++iter)
        {
            if (iter->entry_mode & FNM_Keep) 
            {
                int prio = add_dir(iter.key()).second;
                if (prio == -1) prio = 500;
                max_keep_priority = std::max(max_keep_priority, prio);
            }
            if (iter->entry_mode) continue;
            int prio = add_dir(iter.key()).second;
            if (prio == -1) prio = 500;
            prio_list.insert(prio, iter.key());
        }

        if (prio_list.isEmpty()) continue;
        int active_prio = prio_list.lastKey();
        int total_last_prio = prio_list.count(active_prio);

        enum Action {
            Keep = FNM_Keep,
            Discard = FNM_Delete,
            Terminate = 10,
        } action;

        if (max_keep_priority == active_prio) action = Discard; else // If we have at least one already 'Keep' file at maximum priority - use it and discard all other
        if (total_last_prio == 1) action = Keep; // If we have exactly one file at maximum priority - keep it
        else action = Terminate; // More than one - let's user decide (just stop processing)

        for (const auto& e : prio_list.asKeyValueRange())
        {
            if (e.first < active_prio) ent.set_file_mode(e.second, FNM_Delete); else
            if (action == Terminate) break;
            else ent.set_file_mode(e.second, FileNodeMode(action));
        }
    }
}

void QDupFind::on_btn_apply_pressed()
{
    WaitCursor wc;
    for (auto& ent : dups_visual)
    {
        int max_keep_priority = -1;
        QMultiMap<int, QString> prio_list;

        for (auto iter = ent.dirs.begin(); iter != ent.dirs.end(); ++iter)
        {
            if (iter->entry_mode & FNM_Hide) continue;
            QString file_name = iter.key();
            switch (iter->entry_mode)
            {
                case FNM_Keep: ent.set_file_mode(file_name, FNM_Hide); continue;
                case FNM_Delete: break;
                default: continue;
            }
            if (!do_delete(file_name)) continue;
            ent.set_file_mode(file_name, FNM_Hide);
            scanner->remove_file(file_name, dups_backrefs[file_name]);
            remove_from_dir_tree(file_name.split("/", Qt::SkipEmptyParts), 0, dir_tree_root);
        }
    }
}

// Hide TreeList entry and returns 'true' if no more items to show in parent
bool QDupFind::remove_from_dir_tree(const QStringList& file_name, int index, DirTreeNode& root)
{
    if (!root.children.contains(file_name[index])) return false;
    auto& ent = root.children[file_name[index]];
    if (index + 1 < file_name.size())
    {
        if (!remove_from_dir_tree(file_name, index+1, ent)) return false;
    }
    ent.item->setHidden(true);
    root.children.remove(file_name[index]);
    return root.children.isEmpty();
}
*/

bool QDupFind::do_delete(QString fname)
{
    //!!! Add removing empty dirs !!!
    return QDir().remove(fname);
}
