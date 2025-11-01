#ifndef DEVICESNAPSHOT_H
#define DEVICESNAPSHOT_H

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <utility>
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
    int streamIdentifier = -1;
    
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
    QList<int> targetNumbers;
    QList<int> sourceNumbers;
    QMap<int, QString> targetLabels;
    QMap<int, QString> sourceLabels;
    QMap<std::pair<int,int>, bool> connections;
    
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
    
    QString deviceName;
    QDateTime captureTime;
    QString hostAddress;
    int port;
    int formatVersion;
    
    QStringList rootPaths;
    QMap<QString, NodeData> nodes;
    QMap<QString, ParameterData> parameters;
    QMap<QString, MatrixData> matrices;
    QMap<QString, FunctionData> functions;
    
    int nodeCount() const { return nodes.size(); }
    int parameterCount() const { return parameters.size(); }
    int matrixCount() const { return matrices.size(); }
    int functionCount() const { return functions.size(); }
    
    QJsonDocument toJson() const;
    bool saveToFile(const QString& filePath) const;
    
    static DeviceSnapshot fromJson(const QJsonDocument& doc);
    static DeviceSnapshot loadFromFile(const QString& filePath);
    
private:
    static constexpr int CURRENT_FORMAT_VERSION = 1;
};

#endif
