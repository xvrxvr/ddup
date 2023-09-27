#include "stdafx.h"

#include "empty_dirs.h"

EmptyDirsDialog::EmptyDirsDialog()
{
    ui.setupUi(this);
}

void EmptyDirsDialog::fill(ScanThread* sthread)
{
    WaitCursor wc;
    auto list = sthread->get_empty_dirs();
    for (const auto& f : list)
    {
        auto item = new QListWidgetItem(f, ui.dirs_list);
        item->setCheckState(Qt::Checked);
    }
}

void EmptyDirsDialog::do_remove(QProgressBar* pb)
{
    QStringList lst = get_list(ui.dirs_list);
    pb->setMaximum(lst.size());
    pb->setValue(0);
    int idx = 0;
    for (auto const& f : lst)
    {
        pb->setValue(idx++);
        QDir(f).removeRecursively();
    }
}
