










#ifndef TREEVIEWCONTROLLER_H
#define TREEVIEWCONTROLLER_H

#include <QObject>
#include <QTreeWidgetItem>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QStringList>

class QTreeWidget;
class EmberConnection;

class TreeViewController : public QObject
{
    Q_OBJECT

public:
    explicit TreeViewController(QTreeWidget *treeWidget, EmberConnection *connection, QObject *parent = nullptr);
    ~TreeViewController();

    
    QTreeWidgetItem* findTreeItem(const QString &path) const;
    QStringList getAllTreeItemPaths() const;
    bool hasPathBeenFetched(const QString &path) const;
    void markPathAsFetched(const QString &path);
    void clear();

signals:
    
    void matrixItemCreated(const QString &path, QTreeWidgetItem *item);
    void functionItemCreated(const QString &path, QTreeWidgetItem *item);

public slots:
    
    void onNodeReceived(const QString &path, const QString &identifier, const QString &description, bool isOnline);
    void onParameterReceived(const QString &path, int number, const QString &identifier, const QString &description, const QString &value, 
                            int access, int type, const QVariant &minimum, const QVariant &maximum,
                            const QStringList &enumOptions, const QList<int> &enumValues, bool isOnline, int streamIdentifier,
                                const QString &format, const QString &referenceLevel, const QString &formula, int factor);
    void onMatrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                         int type, int targetCount, int sourceCount);
    void onFunctionReceived(const QString &path, const QString &identifier, const QString &description,
                           const QStringList &argNames, const QList<int> &argTypes,
                           const QStringList &resultNames, const QList<int> &resultTypes);

    
    void onItemExpanded(QTreeWidgetItem *item);

private:
    QTreeWidgetItem* findOrCreateTreeItem(const QString &path);
    void setItemDisplayName(QTreeWidgetItem *item, const QString &baseName);

    QTreeWidget *m_treeWidget;
    EmberConnection *m_connection;
    
    
    QMap<QString, QTreeWidgetItem*> m_pathToItem;
    
    
    QSet<QString> m_fetchedPaths;
    
    
    int m_itemsAddedSinceUpdate;
    
    static constexpr int UPDATE_BATCH_SIZE = 100;
    static constexpr int MATRIX_LABEL_PATH_MARKER = 666999666;
};

#endif 
