#pragma once

#include <QTreeWidget>
#include <QMimeData>
#include <QListWidget>
#include <QDragEnterEvent>

class XDirTree : public QTreeWidget {
    QListWidget* buddy = NULL;

public:
    using QTreeWidget::QTreeWidget;

    void set_buddy(QListWidget* buddy_) {buddy = buddy_;}

    static QString tree_item_to_path(QTreeWidgetItem* item)
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

protected:
    virtual bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data, Qt::DropAction action) override {return true;}
    virtual QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override
    {
        if (items.size() != 1) return nullptr;
        QStringList types = mimeTypes();
        if (types.isEmpty()) return nullptr;
        QTreeWidgetItem* item = items[0];
        if (item->childCount() == 0) return nullptr; // This is a File, do not drop it

        QString path = tree_item_to_path(item);
        if (buddy && !buddy->findItems(path, Qt::MatchExactly).isEmpty()) return nullptr;

        QMimeData* data = new QMimeData();
        QString format = types.at(0);
        QByteArray encoded;
        QDataStream stream(&encoded, QDataStream::WriteOnly);

        stream << 0 << 0 << QMap<int, QVariant>{{Qt::DisplayRole, path}};

        data->setData(format, encoded);
        return data;
    }
    virtual void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->source() == this) event->ignore();
        else QTreeWidget::dragEnterEvent(event);
    }
    void startDrag(Qt::DropActions) override { QTreeWidget::startDrag(Qt::CopyAction);}
};

class XPrioWidget : public QListWidget {
public:
    using QListWidget::QListWidget;
protected:
    void startDrag(Qt::DropActions) override { QListWidget::startDrag(Qt::MoveAction); }
};