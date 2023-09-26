#include "stdafx.h"

#include <QCryptographicHash>
#include <QtConcurrent>

#include "scan_thread.h"

void ScanThread::do_scan_dir(QString dir)
{
    emit new_dir(dir);

    QStringList empty_dir_template(QFileInfo(dir).absoluteFilePath());
    bool is_empty = true;

    for (const auto& ent : QDir(dir).entryInfoList())
    {
        if (ent.isSymLink()) continue;
        if (ent.isFile()) {try_file_short(ent.absoluteFilePath()); is_empty = false;} else
        if (ent.isDir() && ent.fileName() != "." && ent.fileName() != "..") {queue.push(ent.absoluteFilePath()); empty_dir_template << ent.fileName();}
        else continue;

        auto s = queue.stat();
        counters.total_dirs = s.first;
        counters.dirs_to_proceed = s.second;
        emit stat_update(counters);
    }
    if (is_empty)
    {
        QMutexLocker<QMutex> l(&empty_dirs_mutex);
        empty_dirs << empty_dir_template;
    }
}

QByteArray eval_hash(const void* data, uint64_t size, bool partial)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QByteArray((const char*)&size, sizeof(size)));
    md5.addData(QByteArrayView((const char*)data, partial ? std::min<uint64_t>(START_SCAN_SIZE, size) : size));
    return md5.result();
}

bool ScanThread::try_file_short(QString file)
{
    QFile f(file);
    if (!f.open(QIODeviceBase::ReadOnly))
    {
        emit error("Can't open file '" + file + "'");
        return false;
    }
    ++counters.total_files;
    qint64 size = f.size();
    if (!size) return false;
    uchar* data = f.map(0, size);
    if (!data)
    {
        emit error("Can't map file '" + file + "' to memory");
        return false;
    }
    QByteArray hash = eval_hash(data, size, true);
    if (short_files_store.contains(hash))
    {
        QSet<QString>& files = short_files_store[hash];
        if (files.contains(file)) return false;
        QString old_file = *files.begin();
        files.insert(file);
        switch(files.size())
        {
            case 0: case 1: return false; // Unique file
            case 2: // Switch from unique to full - Fill both files
            {
                bool result = try_file_full(old_file, 0);
                return try_file_full(file, data, size, 2) || result;
            }
            default: // Already not unique - just add me
                return try_file_full(file, data, size, 1);
        }
    }
    else
    {
        short_files_store[hash].insert(file);
        return false;
    }
}

// Add new full Hash.
bool ScanThread::try_file_full(QString file, int fake_dups_weight)
{
    QFile f(file);
    if (!f.open(QIODeviceBase::ReadOnly))
    {
        emit error("Can't reopen file '" + file + "'");
        return false;
    }
    qint64 size = f.size();
    uchar* data = f.map(0, size);
    if (!data)
    {
        emit error("Can't remap file '" + file + "' to memory");
        return false;
    }
    return try_file_full(file, data, size, fake_dups_weight);
}

bool ScanThread::try_file_full(QString file, const void* file_image, size_t file_size, int fake_dups_weight)
{
    bool result = false;
    QByteArray hash = eval_hash(file_image, file_size, false);
    if (dups_files_store.contains(hash))
    {
        if (fake_dups_weight) result = true;
        auto& ent = dups_files_store[hash];
        assert(!ent.files.contains(file));
        if (!ent.reported) // Switch from 'fake' to real dups - emit both entries
        {
            for(const auto f: ent.files)  emit new_dup(f, hash);
            ++counters.total_dups;
            ent.reported = true;
        }

        emit new_dup(file, hash);
        ent.files.insert(file);
        ++counters.total_dups;
        fake_dups_weight = 0; // Real duplicate - reset 'fake' dup weight, it will not updated
    }
    dups_files_store[hash].files.insert(file);
    counters.total_false_dups += fake_dups_weight;
    return result;
}

void ScanThread::suspend_resume(QAction* action, bool checked)
{
    if (checked)
    {
        expect_suspend = true;
        action->setDisabled(true);
        suspend_action = action;
        suspend_watcher.setFuture(QtConcurrent::run([this]() {suspend_mutex.lock();}));
    }
    else
    {
        expect_suspend = false; 
        suspend_mutex.unlock();
    }
}

QStringList ScanThread::get_empty_dirs()
{
    struct EmptyDirProto {
        bool me_empty = true;
        bool parent_empty = false;
    };
    QMap<QString, EmptyDirProto> accumulated_empty_dirs;
    {
        QMutexLocker<QMutex> l(&empty_dirs_mutex);
        for (size_t idx = empty_dirs.size(); idx--;)
        {
            const auto& ent = empty_dirs[idx];
            EmptyDirProto data;
            for (size_t i = 1; i < ent.size(); ++i)
            {
                QString path = ent[0] + "/" + ent[i];
                if (!accumulated_empty_dirs.contains(path) || !accumulated_empty_dirs[path].me_empty)
                {
                    data.me_empty = false;
                    break;
                }
            }
            accumulated_empty_dirs[ent[0]] = data;
            if (data.me_empty)
            {
                for (size_t i = 0; i < ent.size(); ++i)
                {
                    accumulated_empty_dirs[ent[0] + "/" + ent[i]].parent_empty = true;
                }
            }
        }
    }
    QStringList result;
    for (const auto& [fname, data] : accumulated_empty_dirs.asKeyValueRange())
    {
        if (data.me_empty && !data.parent_empty) result << fname;
    }
    return result;
}
