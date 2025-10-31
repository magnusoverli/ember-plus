/*
    EmberViewer - Data Transfer Objects for Ember+ protocol
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef EMBERDATATYPES_H
#define EMBERDATATYPES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>

namespace EmberData {

// Node information
struct NodeInfo {
    QString path;
    QString identifier;
    QString description;
    bool isOnline;
    bool hasIdentifier;
    bool hasDescription;
};

// Parameter information
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
};

// Matrix information
struct MatrixInfo {
    QString path;
    int number;
    QString identifier;
    QString description;
    int type;
    int targetCount;
    int sourceCount;
};

// Matrix target information
struct MatrixTargetInfo {
    QString matrixPath;
    int targetNumber;
    QString label;
};

// Matrix source information
struct MatrixSourceInfo {
    QString matrixPath;
    int sourceNumber;
    QString label;
};

// Matrix connection information
struct MatrixConnectionInfo {
    QString matrixPath;
    int targetNumber;
    int sourceNumber;
    bool connected;
    int disposition;
};

// Function information
struct FunctionInfo {
    QString path;
    QString identifier;
    QString description;
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
};

// Invocation result
struct InvocationResult {
    int invocationId;
    bool success;
    QList<QVariant> results;
};

// Stream value update
struct StreamValue {
    int streamIdentifier;
    double value;
};

} // namespace EmberData

#endif // EMBERDATATYPES_H
