/*
    EmberViewer - Device Snapshot
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef DEVICESNAPSHOT_H
#define DEVICESNAPSHOT_H

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QJsonDocument>

struct NodeData {
    QString path;
    QString identifier;
    QString description;
    bool isOnline;
    QStringList childPaths;
    
    QJsonObject toJson() const;
    static NodeData fromJson(const QJsonObject& json);
};

struct ParameterData {
    QString path;
    QString identifier;
    QString value;
    int type;
    int access;
    QVariant minimum;
    QVariant maximum;
    QStringList enumOptions;
    QList<int> enumValues;
    bool isOnline;
    
    QJsonObject toJson() const;
    static ParameterData fromJson(const QJsonObject& json);
};

struct MatrixData {
    QString path;
    QString identifier;
    QString description;
    int type;
    int targetCount;
    int sourceCount;
    QMap<int, QString> targetLabels;
    QMap<int, QString> sourceLabels;
    QMap<QPair<int,int>, bool> connections; // (target,source) -> connected
    
    QJsonObject toJson() const;
    static MatrixData fromJson(const QJsonObject& json);
};

struct FunctionData {
    QString path;
    QString identifier;
    QString description;
    QStringList argNames;
    QList<int> argTypes;
    QStringList resultNames;
    QList<int> resultTypes;
    
    QJsonObject toJson() const;
    static FunctionData fromJson(const QJsonObject& json);
};

class DeviceSnapshot {
public:
    DeviceSnapshot();
    
    // Metadata
    QString deviceName;
    QDateTime captureTime;
    QString hostAddress;
    int port;
    int formatVersion;
    
    // Device structure
    QMap<QString, NodeData> nodes;
    QMap<QString, ParameterData> parameters;
    QMap<QString, MatrixData> matrices;
    QMap<QString, FunctionData> functions;
    
    // Statistics
    int nodeCount() const { return nodes.size(); }
    int parameterCount() const { return parameters.size(); }
    int matrixCount() const { return matrices.size(); }
    int functionCount() const { return functions.size(); }
    
    // Serialization
    QJsonDocument toJson() const;
    bool saveToFile(const QString& filePath) const;
    
    static DeviceSnapshot fromJson(const QJsonDocument& doc);
    static DeviceSnapshot loadFromFile(const QString& filePath);
    
private:
    static constexpr int CURRENT_FORMAT_VERSION = 1;
};

#endif // DEVICESNAPSHOT_H
