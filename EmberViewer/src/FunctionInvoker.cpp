







#include "FunctionInvoker.h"
#include "EmberConnection.h"
#include <QMessageBox>

FunctionInvoker::FunctionInvoker(EmberConnection* connection, QObject* parent)
    : QObject(parent)
    , m_connection(connection)
{
    
    connect(m_connection, &EmberConnection::invocationResultReceived,
            this, &FunctionInvoker::onInvocationResult);
}

FunctionInvoker::~FunctionInvoker()
{
}

void FunctionInvoker::registerFunction(const QString& path, const QString& identifier, 
                                      const QString& description,
                                      const QStringList& argNames, const QList<int>& argTypes,
                                      const QStringList& resultNames, const QList<int>& resultTypes)
{
    FunctionInfo info;
    info.identifier = identifier;
    info.description = description;
    info.argNames = argNames;
    info.argTypes = argTypes;
    info.resultNames = resultNames;
    info.resultTypes = resultTypes;
    
    m_functions[path] = info;
    
    qDebug() << "Registered function:" << identifier << "at path:" << path;
}

FunctionInfo FunctionInvoker::getFunctionInfo(const QString& path) const
{
    return m_functions.value(path);
}

bool FunctionInvoker::hasFunction(const QString& path) const
{
    return m_functions.contains(path);
}

void FunctionInvoker::invokeFunction(const QString& path, const QList<QVariant>& arguments)
{
    if (!m_functions.contains(path)) {
        qWarning() << "Attempted to invoke unknown function:" << path;
        return;
    }
    
    
    int invocationId = m_connection->property("nextInvocationId").toInt();
    
    
    m_pendingInvocations[invocationId] = path;
    
    
    m_connection->invokeFunction(path, arguments);
    
    qInfo() << "Invoked function:" << m_functions[path].identifier 
            << "with invocation ID:" << invocationId;
    
    emit functionInvoked(path, invocationId);
}

void FunctionInvoker::onInvocationResult(int invocationId, bool success, const QList<QVariant>& results)
{
    QString functionPath = m_pendingInvocations.value(invocationId, "Unknown");
    m_pendingInvocations.remove(invocationId);
    
    FunctionInfo funcInfo = m_functions.value(functionPath);
    
    QString resultText;
    if (success) {
        resultText = QString("✅ Function '%1' invoked successfully").arg(funcInfo.identifier);
        if (!results.isEmpty()) {
            resultText += "\n\nReturn values:";
            for (int i = 0; i < results.size() && i < funcInfo.resultNames.size(); i++) {
                QString valueName = funcInfo.resultNames[i];
                QVariant value = results[i];
                resultText += QString("\n  • %1: %2").arg(valueName).arg(value.toString());
            }
        }
    } else {
        resultText = QString("❌ Function '%1' invocation failed").arg(funcInfo.identifier);
    }
    
    
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    if (parentWidget) {
        QMessageBox msgBox(parentWidget);
        msgBox.setWindowTitle("Function Invocation Result");
        msgBox.setText(resultText);
        msgBox.setIcon(success ? QMessageBox::Information : QMessageBox::Warning);
        msgBox.exec();
    }
    
    qInfo().noquote() << QString("Invocation result - ID: %1, Success: %2, Results: %3")
        .arg(invocationId).arg(success ? "YES" : "NO").arg(results.size());
    
    emit invocationResultReceived(invocationId, success, resultText);
}

void FunctionInvoker::clear()
{
    m_functions.clear();
    m_pendingInvocations.clear();
    qDebug() << "Cleared all function data";
}
