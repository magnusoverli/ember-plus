/*
    EmberViewer - Function Invoker
    
    Manages Ember+ function metadata, invocations, and results
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef FUNCTIONINVOKER_H
#define FUNCTIONINVOKER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>

class EmberConnection;

// Function metadata structure
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

    // Store function metadata
    void registerFunction(const QString& path, const QString& identifier, const QString& description,
                         const QStringList& argNames, const QList<int>& argTypes,
                         const QStringList& resultNames, const QList<int>& resultTypes);
    
    // Get function info for UI dialogs
    FunctionInfo getFunctionInfo(const QString& path) const;
    bool hasFunction(const QString& path) const;
    
    // Get all functions (for snapshot)
    const QMap<QString, FunctionInfo>& getAllFunctions() const { return m_functions; }
    
    // Invoke a function (tracks invocation ID)
    void invokeFunction(const QString& path, const QList<QVariant>& arguments);
    
    // Clear all functions (on disconnect)
    void clear();

signals:
    void functionInvoked(const QString& path, int invocationId);
    void invocationResultReceived(int invocationId, bool success, const QString& message);

private slots:
    void onInvocationResult(int invocationId, bool success, const QList<QVariant>& results);

private:
    EmberConnection* m_connection;
    QMap<QString, FunctionInfo> m_functions;       // path -> function info
    QMap<int, QString> m_pendingInvocations;       // invocationId -> function path
};

#endif // FUNCTIONINVOKER_H
