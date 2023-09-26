#pragma once

#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QAtomicInteger>
#include <QFutureWatcher>
#include <QFuture>
#include <QVector>

// Size for initial Scan of file
static constexpr size_t START_SCAN_SIZE = 4*1024;

struct ScanState {
    size_t  total_files;
    size_t  total_dups;
    size_t  total_false_dups;
    size_t  total_dirs;
    size_t  dirs_to_proceed;
};

class ScanThread : public QThread {
    Q_OBJECT;

    enum CmdCode {
        CC_Dir,
        CC_Exit,
        CC_ResetReported,
        CC_RemoveFile
    };
    struct Cmd {
        CmdCode command;
        QString file;
        QByteArray hash;
    };

    class Queue {
        QMutex queue_mutex;
        QSemaphore queue_counter;
        QSet<QString> dirs;
        QVector<QString> dirs_to_proceed;
        QVector<Cmd> oob_commands;
    public:
        void push(QString d) {push(Cmd{CC_Dir, d}); }
        void push(const Cmd& cmd)
        {
            QMutexLocker<QMutex> l(&queue_mutex);
            if (cmd.command == CC_Dir)
            {
                if (dirs.contains(cmd.file)) return;
                dirs_to_proceed.push_back(cmd.file);
                dirs.insert(cmd.file);
            }
            else
            {
                oob_commands.push_back(cmd);
            }
            queue_counter.release();
        }
        Cmd pop()
        {
            for(;;)
            {
                queue_counter.acquire();
                QMutexLocker<QMutex> l(&queue_mutex);
                if (!oob_commands.empty())
                {
                    auto result = oob_commands.front();
                    oob_commands.pop_front();
                    return result;
                }
                if (dirs_to_proceed.empty()) continue;
                QString result = dirs_to_proceed.back();
                dirs_to_proceed.pop_back();
                return Cmd{CC_Dir, result};
            }
        }
        std::pair<size_t, size_t> stat() // Returns <total_dirs, dirs_to_proceed>
        {
            QMutexLocker<QMutex> l(&queue_mutex);
            return { dirs.size(), dirs_to_proceed.size() };
        }
    } queue;
    QMutex suspend_mutex;
    bool expect_suspend = false;

    QMap<QByteArray, QSet<QString>> short_files_store;
    struct DupFilesEntry {
        QSet<QString> files;
        bool reported = false;
    };
    QMap<QByteArray, DupFilesEntry> dups_files_store;
    ScanState counters{};

    // Scan dir & send StatUpdate signal
    void do_scan_dir(QString);

    // Check file. Return true if dups found
    bool try_file_short(QString);
    bool try_file_full(QString, int fake_dups_weight);
    bool try_file_full(QString, const void*, size_t, int fake_dups_weight);

    // Handle bg 'suspend' request
    QFutureWatcher<void> suspend_watcher;
    QAction* suspend_action = NULL;

    virtual void run() override
    {
        for (;;)
        {
            auto cmd = queue.pop();
            switch(cmd.command)
            {
                case CC_Dir:
                {
                    if (expect_suspend) QThread::msleep(100);
                    QMutexLocker<QMutex> l(&suspend_mutex);
                    do_scan_dir(cmd.file);
                    break;
                }
                case CC_Exit: return;
                case CC_RemoveFile:
                {
                    if (dups_files_store.contains(cmd.hash))
                    {
                        dups_files_store[cmd.hash].files.remove(cmd.file);
                    }
                    break;
                }
                case CC_ResetReported:
                {
                    for(auto& ff: dups_files_store) ff.reported = false;
                    break;
                }
            }
        }
    }

public:
    ScanThread(QObject* parent) : QThread(parent) 
    {
        QObject::connect(&suspend_watcher, &QFutureWatcher<void>::finished, this, [this]() {suspend_action->setDisabled(false);});
    }

    void scan_dir(QString d) 
    {
        QFileInfo fi(d);
        if (fi.isDir() && !fi.isSymLink()) queue.push(fi.absoluteFilePath());
        // Stat update event is not sent here - it will be sent from processing thread later.
    }

    void force_exit() {queue.push(Cmd{CC_Exit});}
    void reset_reported() {queue.push(Cmd{CC_ResetReported});}
    void remove_file(QString file, QByteArray hash) {queue.push(Cmd{ CC_RemoveFile, file, hash});}

    void suspend_resume(QAction*, bool checked);

signals:
    void new_dup(QString fname, QByteArray hash);
    void new_dir(QString);
    void stat_update(ScanState event);
    void error(QString);
};
