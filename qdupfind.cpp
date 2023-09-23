#include "stdafx.h"

#include <QFileDialog>

#include "qdupfind.h"

QDupFind::QDupFind(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    ui.errors->hide();
    ui.priority_spin->setDisabled(true);

    scanner = new ScanThread(this);

    connect(scanner, &ScanThread::new_dir, this, &QDupFind::scan_new_dir, Qt::BlockingQueuedConnection);
    connect(scanner, &ScanThread::new_dup, this, &QDupFind::scan_new_dup, Qt::QueuedConnection);
    connect(scanner, &ScanThread::stat_update, this, &QDupFind::scan_stat_update, Qt::QueuedConnection);
    connect(scanner, &ScanThread::error, this, &QDupFind::scan_error, Qt::QueuedConnection);

    new QShortcut(Qt::Key_Space, ui.files, [this]() {on_btn_invert_pressed();}, Qt::WidgetShortcut);
    new QShortcut(Qt::Key_Delete, ui.files, [this]() {on_btn_remove_pressed();}, Qt::WidgetShortcut);
    new QShortcut(Qt::Key_Insert, ui.files, [this]() {on_btn_keep_me_pressed();}, Qt::WidgetShortcut);

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
    ui.errors->show();
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

QString DirTreeNode::get_visual_name(int parent_priority, QString raw_name)
{
    if (children.isEmpty()) return raw_name; // This is File - no priority marks on Files
    if (priority != -1) return QString("=%1 %2").arg(priority).arg(raw_name);
    if (parent_priority != -1) return QString("[%1] %2").arg(parent_priority).arg(raw_name);
    return raw_name;
}

std::pair<DirTreeNode*, int> QDupFind::add_dir(QStringList path, int trim, bool set_icons)
{
    DirTreeNode* root = &dir_tree_root;
    int priority = dir_tree_root.priority;
    size_t max = path.size() - trim;
    for (size_t i=0; i<max; ++i)
    {
        const auto& tok = path[i];
        if (!root->children.contains(tok))
        {
            auto iter = root->children.insert(tok, {});
            auto item = new QTreeWidgetItem(QStringList(iter->get_visual_name(priority, tok)));
            item->setData(0, Qt::UserRole, tok);
            if (set_icons) item->setIcon(0, style()->standardIcon(i+1 == max ? QStyle::SP_FileIcon : QStyle::SP_DirIcon));
            iter->item = item;
            if (iter == root->children.begin()) // Insert at top
            {
                if (!root->item) // top level
                {
                    ui.dirs->insertTopLevelItem(0, item);
                }
                else
                {
                    root->item->insertChild(0, item);
                }
            }
            else // insert after some other item
            {
                auto prev = iter;
                --prev;
                if (!root->item) // top level
                {
                    ui.dirs->insertTopLevelItem(ui.dirs->indexOfTopLevelItem(prev->item)+1, item);
                }
                else
                {
                    root->item->insertChild(root->item->indexOfChild(prev->item)+1, item);
                }
            }
        }
        root = &root->children[tok];
        if (root->priority != -1) priority = root->priority;
    }
    return {root, priority};
}

void QDupFind::scan_new_dup(QString fname, QByteArray hash)
{
   add_dir(fname);
   dups_backrefs[fname] = hash;

   if (!dups_hash_toc.contains(hash)) // New File entry
   {
        QString file_name = QFileInfo(fname).fileName();
        QString first_entry_key = QString("%1 %2").arg(file_name).arg(hash.toHex());
        dups_hash_toc[hash] = first_entry_key;

        FileNode fn;
        fn.file_name = file_name;
        fn.item = new QTreeWidgetItem(QStringList(file_name));
        fn.item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        auto iter = dups_visual.insert(first_entry_key, fn);
        if (iter == dups_visual.begin()) ui.files->insertTopLevelItem(0, fn.item); else
        {
            auto prev = iter;
            --prev;
            ui.files->insertTopLevelItem(ui.files->indexOfTopLevelItem(prev->item)+1, fn.item);
        }
        iter->add_dir_node(fname, ui.files);
   }
   else
   {
       dups_visual[dups_hash_toc[hash]].add_dir_node(fname, ui.files);
   }
}

void FileNode::add_dir_node(QString fname, QTreeWidget* wg)
{
    QFileInfo fi(fname);
    if (!mixed_file_names && file_name != fi.fileName()) to_mixed_file_name();
    QString dir_entry_name = mixed_file_names ? fname : fi.path();
    auto wg_item = new QTreeWidgetItem(QStringList(dir_entry_name));
    wg_item->setData(0, Qt::UserRole, fname);
    auto iter = dirs.insert(fname, DirNode{ wg_item});
    if (iter == dirs.begin()) item->insertChild(0, wg_item); else
    {
        auto prev = iter; --prev;
        item->insertChild(item->indexOfChild(prev->item)+1, wg_item);
    }
    if (mixed_file_names) update_file_name_in_widget();
}

void FileNode::to_mixed_file_name()
{
    mixed_file_names = true;
    update_file_name_in_widget();
    for(auto ent: dirs.asKeyValueRange()) ent.second.item->setText(0, ent.first);
}

void FileNode::update_file_name_in_widget()
{
    QSet<QString> acc;
    for(auto ent : dirs.asKeyValueRange()) acc.insert(QFileInfo(ent.first).fileName());
    QStringList acc2(acc.begin(), acc.end());
    acc2.sort();
    item->setText(0, acc2.join(" | "));
}

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

void QDupFind::move_to_next_file()
{
    auto parent = ui.files->currentItem()->parent();
    if (!parent) return;
    int idx = ui.files->indexOfTopLevelItem(parent);
    if (ui.files->topLevelItemCount() <= idx) return;
    auto next_parent = ui.files->topLevelItem(idx+1);
    next_parent->setExpanded(true);
    ui.files->setCurrentItem(next_parent->child(0));
}

QString QDupFind::tree_item_to_path(QTreeWidgetItem* item)
{
    QStringList acc;
    while (item)
    {
        acc << item->data(0, Qt::UserRole).toString();
        item = item->parent();
    }
    std::reverse(acc.begin(), acc.end());
    return acc.join("/");
}

FileNode* QDupFind::path_to_fnode(QString path)
{
    if (!dups_backrefs.contains(path)) return NULL;
    auto & p1 = dups_backrefs[path];
    assert(dups_hash_toc.contains(p1));
    auto & p2 = dups_hash_toc[p1];
    assert(dups_visual.contains(p2));
    return &dups_visual[p2];
}

void QDupFind::on_dirs_itemDoubleClicked(QTreeWidgetItem* item, int column)
{
    auto path = tree_item_to_path(item);
    const FileNode* fnode = path_to_fnode(path);
    if (!fnode) return;
    auto wg_item = fnode->dirs[path].item;
    if (auto p = wg_item->parent()) p->setExpanded(true);
    ui.files->setCurrentItem(wg_item);
    ui.tabWidget->setCurrentIndex(1);
}

void QDupFind::on_files_itemDoubleClicked(QTreeWidgetItem* item, int column)
{
    auto fname = item->data(0, Qt::UserRole).toString();
    if (fname.isEmpty()) return;
    auto wg_item = add_dir(fname).first->item;
    if (auto p = wg_item->parent()) p->setExpanded(true);
    ui.dirs->setCurrentItem(wg_item);
    ui.tabWidget->setCurrentIndex(0);
}

void QDupFind::on_dirs_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem*)
{
    auto path = tree_item_to_path(current);
    ui.dir_to_process->setText(path);
    if (!current->childCount()) { cur_dir_selected = ""; ui.priority_spin->setDisabled(true); return;}
    cur_dir_selected = path; 
    ui.priority_spin->setDisabled(false);
    int priority = add_dir(cur_dir_selected).second;
    suppress_new_prio_value = true;
    ui.priority_spin->setValue(priority == -1 ? 500 : priority);
    suppress_new_prio_value = false;
}

void QDupFind::on_priority_spin_valueChanged(int priority)
{
    if (suppress_new_prio_value || cur_dir_selected.isEmpty()) return;
    auto item = add_dir(cur_dir_selected).first;
    if (item->priority == priority) return;
    item->priority = priority;
    item->update_visual_widget(-1);
}

void DirTreeNode::update_visual_widget(int parent_priority)
{
    auto visual = get_visual_name(parent_priority, item->data(0, Qt::UserRole).toString());
    item->setText(0, visual);
    int pp = priority != -1 ? priority : parent_priority;
    for(auto& ent: children) ent.update_visual_widget(pp);
}

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

bool QDupFind::do_delete(QString fname)
{
    return QDir().remove(fname);
}
