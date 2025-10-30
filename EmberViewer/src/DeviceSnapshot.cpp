/*
    EmberViewer - Device Snapshot Implementation
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "DeviceSnapshot.h"
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// NodeData implementation
QJsonObject NodeData::toJson() const {
    QJsonObject obj;
    obj["path"] = path;
    obj["identifier"] = identifier;
    obj["description"] = description;
    obj["isOnline"] = isOnline;
    
    QJsonArray childArray;
    for (const QString& child : childPaths) {
        childArray.append(child);
    }
    obj["children"] = childArray;
    
    return obj;
}

NodeData NodeData::fromJson(const QJsonObject& json) {
    NodeData data;
    data.path = json["path"].toString();
    data.identifier = json["identifier"].toString();
    data.description = json["description"].toString();
    data.isOnline = json["isOnline"].toBool(true);
    
    QJsonArray childArray = json["children"].toArray();
    for (const QJsonValue& val : childArray) {
        data.childPaths.append(val.toString());
    }
    
    return data;
}

// ParameterData implementation
QJsonObject ParameterData::toJson() const {
    QJsonObject obj;
    obj["path"] = path;
    obj["identifier"] = identifier;
    obj["value"] = value;
    obj["type"] = type;
    obj["access"] = access;
    obj["isOnline"] = isOnline;
    
    if (minimum.isValid()) {
        obj["minimum"] = QJsonValue::fromVariant(minimum);
    }
    if (maximum.isValid()) {
        obj["maximum"] = QJsonValue::fromVariant(maximum);
    }
    
    if (!enumOptions.isEmpty()) {
        QJsonArray optionsArray;
        for (const QString& opt : enumOptions) {
            optionsArray.append(opt);
        }
        obj["enumOptions"] = optionsArray;
        
        QJsonArray valuesArray;
        for (int val : enumValues) {
            valuesArray.append(val);
        }
        obj["enumValues"] = valuesArray;
    }
    
    if (streamIdentifier != -1) {
        obj["streamIdentifier"] = streamIdentifier;
    }
    
    return obj;
}

ParameterData ParameterData::fromJson(const QJsonObject& json) {
    ParameterData data;
    data.path = json["path"].toString();
    data.identifier = json["identifier"].toString();
    data.value = json["value"].toString();
    data.type = json["type"].toInt();
    data.access = json["access"].toInt();
    data.isOnline = json["isOnline"].toBool(true);
    
    if (json.contains("minimum")) {
        data.minimum = json["minimum"].toVariant();
    }
    if (json.contains("maximum")) {
        data.maximum = json["maximum"].toVariant();
    }
    
    if (json.contains("enumOptions")) {
        QJsonArray optionsArray = json["enumOptions"].toArray();
        for (const QJsonValue& val : optionsArray) {
            data.enumOptions.append(val.toString());
        }
        
        QJsonArray valuesArray = json["enumValues"].toArray();
        for (const QJsonValue& val : valuesArray) {
            data.enumValues.append(val.toInt());
        }
    }
    
    if (json.contains("streamIdentifier")) {
        data.streamIdentifier = json["streamIdentifier"].toInt();
    }
    
    return data;
}

// MatrixData implementation
QJsonObject MatrixData::toJson() const {
    QJsonObject obj;
    obj["path"] = path;
    obj["identifier"] = identifier;
    obj["description"] = description;
    obj["type"] = type;
    obj["targetCount"] = targetCount;
    obj["sourceCount"] = sourceCount;
    
    // Target numbers (actual indices)
    QJsonArray targetNumbersArray;
    for (int num : targetNumbers) {
        targetNumbersArray.append(num);
    }
    obj["targetNumbers"] = targetNumbersArray;
    
    // Source numbers (actual indices)
    QJsonArray sourceNumbersArray;
    for (int num : sourceNumbers) {
        sourceNumbersArray.append(num);
    }
    obj["sourceNumbers"] = sourceNumbersArray;
    
    // Target labels
    QJsonObject targetLabelsObj;
    for (auto it = targetLabels.begin(); it != targetLabels.end(); ++it) {
        targetLabelsObj[QString::number(it.key())] = it.value();
    }
    obj["targetLabels"] = targetLabelsObj;
    
    // Source labels
    QJsonObject sourceLabelsObj;
    for (auto it = sourceLabels.begin(); it != sourceLabels.end(); ++it) {
        sourceLabelsObj[QString::number(it.key())] = it.value();
    }
    obj["sourceLabels"] = sourceLabelsObj;
    
    // Connections
    QJsonArray connectionsArray;
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (it.value()) {  // Only save connected crosspoints
            QJsonObject conn;
            conn["target"] = it.key().first;
            conn["source"] = it.key().second;
            connectionsArray.append(conn);
        }
    }
    obj["connections"] = connectionsArray;
    
    return obj;
}

MatrixData MatrixData::fromJson(const QJsonObject& json) {
    MatrixData data;
    data.path = json["path"].toString();
    data.identifier = json["identifier"].toString();
    data.description = json["description"].toString();
    data.type = json["type"].toInt();
    data.targetCount = json["targetCount"].toInt();
    data.sourceCount = json["sourceCount"].toInt();
    
    // Target numbers (actual indices)
    if (json.contains("targetNumbers")) {
        QJsonArray targetNumbersArray = json["targetNumbers"].toArray();
        for (const QJsonValue& val : targetNumbersArray) {
            data.targetNumbers.append(val.toInt());
        }
    }
    
    // Source numbers (actual indices)
    if (json.contains("sourceNumbers")) {
        QJsonArray sourceNumbersArray = json["sourceNumbers"].toArray();
        for (const QJsonValue& val : sourceNumbersArray) {
            data.sourceNumbers.append(val.toInt());
        }
    }
    
    // Target labels
    QJsonObject targetLabelsObj = json["targetLabels"].toObject();
    for (auto it = targetLabelsObj.begin(); it != targetLabelsObj.end(); ++it) {
        data.targetLabels[it.key().toInt()] = it.value().toString();
    }
    
    // Source labels
    QJsonObject sourceLabelsObj = json["sourceLabels"].toObject();
    for (auto it = sourceLabelsObj.begin(); it != sourceLabelsObj.end(); ++it) {
        data.sourceLabels[it.key().toInt()] = it.value().toString();
    }
    
    // Connections
    QJsonArray connectionsArray = json["connections"].toArray();
    for (const QJsonValue& val : connectionsArray) {
        QJsonObject conn = val.toObject();
        int target = conn["target"].toInt();
        int source = conn["source"].toInt();
        data.connections[{target, source}] = true;
    }
    
    return data;
}

// FunctionData implementation
QJsonObject FunctionData::toJson() const {
    QJsonObject obj;
    obj["path"] = path;
    obj["identifier"] = identifier;
    obj["description"] = description;
    
    QJsonArray argNamesArray;
    for (const QString& name : argNames) {
        argNamesArray.append(name);
    }
    obj["argNames"] = argNamesArray;
    
    QJsonArray argTypesArray;
    for (int type : argTypes) {
        argTypesArray.append(type);
    }
    obj["argTypes"] = argTypesArray;
    
    QJsonArray resultNamesArray;
    for (const QString& name : resultNames) {
        resultNamesArray.append(name);
    }
    obj["resultNames"] = resultNamesArray;
    
    QJsonArray resultTypesArray;
    for (int type : resultTypes) {
        resultTypesArray.append(type);
    }
    obj["resultTypes"] = resultTypesArray;
    
    return obj;
}

FunctionData FunctionData::fromJson(const QJsonObject& json) {
    FunctionData data;
    data.path = json["path"].toString();
    data.identifier = json["identifier"].toString();
    data.description = json["description"].toString();
    
    QJsonArray argNamesArray = json["argNames"].toArray();
    for (const QJsonValue& val : argNamesArray) {
        data.argNames.append(val.toString());
    }
    
    QJsonArray argTypesArray = json["argTypes"].toArray();
    for (const QJsonValue& val : argTypesArray) {
        data.argTypes.append(val.toInt());
    }
    
    QJsonArray resultNamesArray = json["resultNames"].toArray();
    for (const QJsonValue& val : resultNamesArray) {
        data.resultNames.append(val.toString());
    }
    
    QJsonArray resultTypesArray = json["resultTypes"].toArray();
    for (const QJsonValue& val : resultTypesArray) {
        data.resultTypes.append(val.toInt());
    }
    
    return data;
}

// DeviceSnapshot implementation
DeviceSnapshot::DeviceSnapshot()
    : port(0)
    , formatVersion(CURRENT_FORMAT_VERSION)
{
    captureTime = QDateTime::currentDateTime();
}

QJsonDocument DeviceSnapshot::toJson() const {
    QJsonObject root;
    
    // Metadata
    root["formatVersion"] = formatVersion;
    root["deviceName"] = deviceName;
    root["captureTime"] = captureTime.toString(Qt::ISODate);
    root["hostAddress"] = hostAddress;
    root["port"] = port;
    
    // Statistics
    QJsonObject stats;
    stats["nodes"] = nodeCount();
    stats["parameters"] = parameterCount();
    stats["matrices"] = matrixCount();
    stats["functions"] = functionCount();
    root["statistics"] = stats;
    
    // Root paths order
    QJsonArray rootPathsArray;
    for (const QString& path : rootPaths) {
        rootPathsArray.append(path);
    }
    root["rootPaths"] = rootPathsArray;
    
    // Nodes
    QJsonArray nodesArray;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        nodesArray.append(it.value().toJson());
    }
    root["nodes"] = nodesArray;
    
    // Parameters
    QJsonArray parametersArray;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        parametersArray.append(it.value().toJson());
    }
    root["parameters"] = parametersArray;
    
    // Matrices
    QJsonArray matricesArray;
    for (auto it = matrices.begin(); it != matrices.end(); ++it) {
        matricesArray.append(it.value().toJson());
    }
    root["matrices"] = matricesArray;
    
    // Functions
    QJsonArray functionsArray;
    for (auto it = functions.begin(); it != functions.end(); ++it) {
        functionsArray.append(it.value().toJson());
    }
    root["functions"] = functionsArray;
    
    return QJsonDocument(root);
}

bool DeviceSnapshot::saveToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }
    
    QJsonDocument doc = toJson();
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

DeviceSnapshot DeviceSnapshot::fromJson(const QJsonDocument& doc) {
    DeviceSnapshot snapshot;
    QJsonObject root = doc.object();
    
    // Metadata
    snapshot.formatVersion = root["formatVersion"].toInt();
    snapshot.deviceName = root["deviceName"].toString();
    snapshot.captureTime = QDateTime::fromString(root["captureTime"].toString(), Qt::ISODate);
    snapshot.hostAddress = root["hostAddress"].toString();
    snapshot.port = root["port"].toInt();
    
    // Root paths order
    if (root.contains("rootPaths")) {
        QJsonArray rootPathsArray = root["rootPaths"].toArray();
        for (const QJsonValue& val : rootPathsArray) {
            snapshot.rootPaths.append(val.toString());
        }
    }
    
    // Nodes
    QJsonArray nodesArray = root["nodes"].toArray();
    for (const QJsonValue& val : nodesArray) {
        NodeData data = NodeData::fromJson(val.toObject());
        snapshot.nodes[data.path] = data;
    }
    
    // Parameters
    QJsonArray parametersArray = root["parameters"].toArray();
    for (const QJsonValue& val : parametersArray) {
        ParameterData data = ParameterData::fromJson(val.toObject());
        snapshot.parameters[data.path] = data;
    }
    
    // Matrices
    QJsonArray matricesArray = root["matrices"].toArray();
    for (const QJsonValue& val : matricesArray) {
        MatrixData data = MatrixData::fromJson(val.toObject());
        snapshot.matrices[data.path] = data;
    }
    
    // Functions
    QJsonArray functionsArray = root["functions"].toArray();
    for (const QJsonValue& val : functionsArray) {
        FunctionData data = FunctionData::fromJson(val.toObject());
        snapshot.functions[data.path] = data;
    }
    
    return snapshot;
}

DeviceSnapshot DeviceSnapshot::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for reading:" << filePath;
        return DeviceSnapshot();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return fromJson(doc);
}
