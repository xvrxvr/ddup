#include "stdafx.h"

#include "find.h"

void FindDialog::on_btn_find_pressed()
{
    WaitCursor wc;

    std::function<QString(QString)> file_extractor;
    std::function<bool(QString)> match_functor;
    QRegularExpressionMatch match;
    QString fname = ui.find_name->text();
    QString aka = ui.aka->text().trimmed();

    if (!aka_callback) aka = {};

    switch (ui.find_mode->currentIndex())
    {
        case 0: file_extractor = [](QString f) {return f;}; break;
        case 1: file_extractor = [](QString f) {return QFileInfo(f).path();}; break;
        default: file_extractor = [](QString f) {return QFileInfo(f).fileName(); }; break;
    }
    switch (ui.find_type->currentIndex())
    {
        case 0: match_functor = [this, fname](QString f) {return fname == f;}; break;
        default: 
        {
            QRegularExpression re(fname);
            if (!re.isValid()) {QMessageBox::critical(NULL, "Error", "Invalid Regular Expression"); return;}
            match_functor = [this, &match, re](QString f) {match = re.match(f); return match.hasMatch();}; 
            break;
        }
    }

    for (const auto& f : files)
    {
        QString ff = file_extractor(f); // Make if offline because QRegularExpressionMatch car refer to it
        if (!match_functor(ff)) continue;
        if (!aka.isEmpty())
        {
            static QRegularExpression dlm("\\$(.)");
            QString processed_aka;
            auto iter = dlm.globalMatch(aka);
            if (!iter.hasNext()) processed_aka = aka; else
            {
                int last_pos = 0;
                while (iter.hasNext())
                {
                    auto m = iter.next();
                    if (m.capturedStart() > last_pos) processed_aka += aka.mid(last_pos, match.capturedStart() - last_pos);
                    last_pos = m.capturedEnd();
                    auto sym = m.captured(1);
                    if (sym[0].isNumber()) processed_aka += match.captured(sym.toInt()); else processed_aka += sym;
                }
                if (last_pos < aka.size()) processed_aka += aka.mid(last_pos);
            }
            if (!aka_callback(f, processed_aka)) continue;
        }
        auto item = new QListWidgetItem(f, ui.files);
        item->setCheckState(Qt::Checked);
    }
    ui.pages->setCurrentIndex(1);
}

void FindDialog::on_btn_exec_pressed()
{
    static const FileNodeMode modes[] = {FNM_DeleteManual, FNM_KeepManual, FNM_KeepMe, FNM_KeepOther, FNM_KeepDup};
    action_callback(get_list(ui.files), modes[ui.action_sel->currentIndex()]);
}
