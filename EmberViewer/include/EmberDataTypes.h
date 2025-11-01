







#ifndef EMBERDATATYPES_H
#define EMBERDATATYPES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>

namespace EmberData {


struct NodeInfo {
    QString path;
    QString identifier;
    QString description;
    bool isOnline;
    bool hasIdentifier;
    bool hasDescription;
};


struct ParameterInfo {
    QString path;
    int number;
    QString identifier;
    QString value;
    int access;
    int type;
    QVariant minimum;
    QVariant maximum;
    QStringList enumOptions;
    QList<int> enumValues;
    bool isOnline;
    int streamIdentifier;
    QString format;              // C-style format string (e.g., "%.1f dBFS")
    QString referenceLevel;      // Detected reference level (e.g., "dBFS", "dBu", "dBr")
};


struct MatrixInfo {
    QString path;
    int number;
    QString identifier;
    QString description;
    int type;
    int targetCount;
    int sourceCount;
};


struct MatrixTargetInfo {
    QString matrixPath;
    int targetNumber;
    QString label;
};


struct MatrixSourceInfo {
    QString matrixPath;
    int sourceNumber;
    QString label;
};


struct MatrixConnectionInfo {
    QString matrixPath;
    int targetNumber;
    int sourceNumber;
    bool connected;
    int disposition;
};


struct FunctionInfo {
    QString path;
    QString identifier;
    QString description;
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
};


struct InvocationResult {
    int invocationId;
    bool success;
    QList<QVariant> results;
};


struct StreamValue {
    int streamIdentifier;
    double value;
};

} 

#endif 
