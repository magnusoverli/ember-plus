









#ifndef FUNCTIONINVOKER_H
#define FUNCTIONINVOKER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>

class EmberConnection;


struct FunctionInfo {
    QString identifier;
    QString description;
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
};

class FunctionInvoker : public QObject
{
    Q_OBJECT

public:
    explicit FunctionInvoker(EmberConnection* connection, QObject* parent = nullptr);
    ~FunctionInvoker();

    
    void registerFunction(const QString& path, const QString& identifier, const QString& description,
                         const QStringList& argNames, const QList<int>& argTypes,
                         const QStringList& resultNames, const QList<int>& resultTypes);
    
    
    FunctionInfo getFunctionInfo(const QString& path) const;
    bool hasFunction(const QString& path) const;
    
    
    const QMap<QString, FunctionInfo>& getAllFunctions() const { return m_functions; }
    
    
    void invokeFunction(const QString& path, const QList<QVariant>& arguments);
    
    
    void clear();

signals:
    void functionInvoked(const QString& path, int invocationId);
    void invocationResultReceived(int invocationId, bool success, const QString& message);

private slots:
    void onInvocationResult(int invocationId, bool success, const QList<QVariant>& results);

private:
    EmberConnection* m_connection;
    QMap<QString, FunctionInfo> m_functions;       
    QMap<int, QString> m_pendingInvocations;       
};

#endif 
