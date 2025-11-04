







#include "GlowParser.h"
#include <ember/glow/GlowNodeFactory.hpp>
#include <ember/glow/GlowRootElementCollection.hpp>
#include <ember/glow/GlowNode.hpp>
#include <ember/glow/GlowQualifiedNode.hpp>
#include <ember/glow/GlowParameter.hpp>
#include <ember/glow/GlowQualifiedParameter.hpp>
#include <ember/glow/GlowMatrix.hpp>
#include <ember/glow/GlowQualifiedMatrix.hpp>
#include <ember/glow/GlowFunction.hpp>
#include <ember/glow/GlowQualifiedFunction.hpp>
#include <ember/glow/GlowConnection.hpp>
#include <ember/glow/GlowTarget.hpp>
#include <ember/glow/GlowSource.hpp>
#include <ember/glow/GlowLabel.hpp>
#include <ember/glow/GlowStreamCollection.hpp>
#include <ember/glow/GlowStreamEntry.hpp>
#include <ember/glow/GlowInvocationResult.hpp>
#include <QVariant>
#include <QDebug>

GlowParser::GlowParser(QObject *parent)
    : QObject(parent)
    , m_domReader(new StreamingDomReader(libember::glow::GlowNodeFactory::getFactory()))
{
    // Set up streaming callback to process items as they arrive
    m_domReader->setItemReadyCallback([this](libember::dom::Node* node) {
        if (node) {
            this->onItemReady(node);
        }
    });
}

GlowParser::~GlowParser()
{
    delete m_domReader;
}

void GlowParser::parseEmberData(const QByteArray& data)
{
    try {
        
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
        m_domReader->read(bytes, bytes + data.size());
        
        
        if (m_domReader->isRootReady()) {
            auto root = m_domReader->detachRoot();
            qDebug() << "[GlowParser] Processing root with" << data.size() << "bytes of EmBER data";
            processRoot(root);
        } else {
            qDebug() << "[GlowParser] Root not ready after reading" << data.size() << "bytes (accumulating)";
        }
    } catch (const std::exception& e) {
        emit parsingError(QString("Ember+ parsing error: %1").arg(e.what()));
    }
}

void GlowParser::processRoot(libember::dom::Node* root)
{
    if (!root) {
        return;
    }
    
    try {
        auto glowRoot = dynamic_cast<libember::glow::GlowRootElementCollection*>(root);
        if (glowRoot) {
            qDebug() << "[GlowParser] Root is GlowRootElementCollection with" << glowRoot->size() << "elements";
            processElementCollection(glowRoot, "");
        } else {
            
            auto streamColl = dynamic_cast<libember::glow::GlowStreamCollection*>(root);
            if (streamColl) {
                qDebug() << "[GlowParser] Root is standalone GlowStreamCollection";
                processStreamCollection(streamColl);
            } else {
                qDebug() << "[GlowParser] WARNING: Root is unknown type!";
            }
        }
    } catch (const std::exception& e) {
        emit parsingError(QString("Error processing root: %1").arg(e.what()));
    }
    
    delete root;
}

void GlowParser::processElementCollection(libember::glow::GlowContainer* container, const QString& parentPath)
{
    for (auto it = container->begin(); it != container->end(); ++it) {
        auto element = &(*it);
        
        
        if (auto qnode = dynamic_cast<libember::glow::GlowQualifiedNode*>(element)) {
            processQualifiedNode(qnode);
        }
        else if (auto node = dynamic_cast<libember::glow::GlowNode*>(element)) {
            processNode(node, parentPath);
        }
        else if (auto qparam = dynamic_cast<libember::glow::GlowQualifiedParameter*>(element)) {
            processQualifiedParameter(qparam);
        }
        else if (auto param = dynamic_cast<libember::glow::GlowParameter*>(element)) {
            processParameter(param, parentPath);
        }
        else if (auto qmatrix = dynamic_cast<libember::glow::GlowQualifiedMatrix*>(element)) {
            processQualifiedMatrix(qmatrix);
        }
        else if (auto matrix = dynamic_cast<libember::glow::GlowMatrix*>(element)) {
            processMatrix(matrix, parentPath);
        }
        else if (auto qfunction = dynamic_cast<libember::glow::GlowQualifiedFunction*>(element)) {
            processQualifiedFunction(qfunction);
        }
        else if (auto function = dynamic_cast<libember::glow::GlowFunction*>(element)) {
            processFunction(function, parentPath);
        }
        else if (auto invResult = dynamic_cast<libember::glow::GlowInvocationResult*>(element)) {
            processInvocationResult(invResult);
        }
        else if (auto streamColl = dynamic_cast<libember::glow::GlowStreamCollection*>(element)) {
            processStreamCollection(streamColl);
        }
        else {
            qDebug() << "[GlowParser] WARNING: Unknown element type received, might be stream data";
        }
    }
}

void GlowParser::processQualifiedNode(libember::glow::GlowQualifiedNode* node, bool)
{
    EmberData::NodeInfo info;
    
    
    auto path = node->path();
    for (auto num : path) {
        info.path += QString::number(num) + ".";
    }
    info.path.chop(1);  
    
    
    info.hasIdentifier = node->contains(libember::glow::NodeProperty::Identifier);
    info.identifier = info.hasIdentifier
        ? QString::fromStdString(node->identifier()) 
        : QString("Node %1").arg(path.back());
    
    info.hasDescription = node->contains(libember::glow::NodeProperty::Description);
    info.description = info.hasDescription
        ? QString::fromStdString(node->description()) 
        : "";
    
    info.isOnline = node->contains(libember::glow::NodeProperty::IsOnline)
        ? node->isOnline()
        : true;
    
    
    
    bool hadIdentifier = m_nodesWithIdentifier.value(info.path, false);
    bool shouldEmit = true;
    
    if (hadIdentifier && !info.hasIdentifier) {
        qDebug() << "[GlowParser] SKIPPING QualifiedNode:" << info.path 
                 << "- would replace valid identifier with stub";
        shouldEmit = false;
    }
    
    
    if (info.hasIdentifier) {
        m_nodesWithIdentifier[info.path] = true;
    }
    
    if (shouldEmit) {
        qDebug() << "[GlowParser] EMITTING QualifiedNode:" << info.path 
                 << "identifier=" << info.identifier 
                 << "hasIdentifier=" << info.hasIdentifier;
        emit nodeReceived(info);
    }
    
    
    if (node->children()) {
        processElementCollection(node->children(), info.path);
    }
}

void GlowParser::processNode(libember::glow::GlowNode* node, const QString& parentPath, bool)
{
    EmberData::NodeInfo info;
    
    int number = node->number();
    info.path = parentPath.isEmpty() 
        ? QString::number(number) 
        : QString("%1.%2").arg(parentPath).arg(number);
    
    info.hasIdentifier = node->contains(libember::glow::NodeProperty::Identifier);
    info.identifier = info.hasIdentifier
        ? QString::fromStdString(node->identifier())
        : QString("Node %1").arg(number);
    
    info.hasDescription = node->contains(libember::glow::NodeProperty::Description);
    info.description = info.hasDescription
        ? QString::fromStdString(node->description())
        : "";
    
    info.isOnline = node->contains(libember::glow::NodeProperty::IsOnline)
        ? node->isOnline()
        : true;
    
    
    
    bool hadIdentifier = m_nodesWithIdentifier.value(info.path, false);
    bool shouldEmit = true;
    
    if (hadIdentifier && !info.hasIdentifier) {
        qDebug() << "[GlowParser] SKIPPING Node:" << info.path 
                 << "- would replace valid identifier with stub";
        shouldEmit = false;
    }
    
    
    if (info.hasIdentifier) {
        m_nodesWithIdentifier[info.path] = true;
    }
    
    if (shouldEmit) {
        qDebug() << "[GlowParser] EMITTING Node:" << info.path 
                 << "identifier=" << info.identifier 
                 << "hasIdentifier=" << info.hasIdentifier;
        emit nodeReceived(info);
    }
    
    
    if (node->children()) {
        processElementCollection(node->children(), info.path);
    }
}

void GlowParser::processQualifiedParameter(libember::glow::GlowQualifiedParameter* param)
{
    EmberData::ParameterInfo info;
    
    
    auto path = param->path();
    for (auto num : path) {
        info.path += QString::number(num) + ".";
    }
    info.path.chop(1);
    
    info.number = path.back();
    
    
    
    
    
    static int debugCount = 0;
    if (debugCount < 5 && (info.path.contains(".1.0.1.") || info.path.contains(".1.1.1."))) {
        qDebug().noquote() << QString("DEBUG: Parameter path=%1, m_matrixLabelPaths.size=%2")
            .arg(info.path).arg(m_matrixLabelPaths.size());
        debugCount++;
    }
    
    for (auto it = m_matrixLabelPaths.begin(); it != m_matrixLabelPaths.end(); ++it) {
        const QString& matrixPath = it.key();
        const MatrixLabelPaths& labelPaths = it.value();
        
        
        for (auto labelIt = labelPaths.labelBasePaths.begin(); labelIt != labelPaths.labelBasePaths.end(); ++labelIt) {
            const QString& basePath = labelIt.key();
            
            if (info.path.startsWith(basePath + ".")) {
                qDebug().noquote() << QString("MATCHED label path! basePath=%1, fullPath=%2")
                    .arg(basePath).arg(info.path);
                
                
                
                QString remaining = info.path.mid(basePath.length() + 1);
                QStringList parts = remaining.split('.');
                
                
                if (parts.size() >= 2) {
                    int nodeNumber = parts[0].toInt();
                    int signalNumber = parts[1].toInt();
                    
                    if (param->contains(libember::glow::ParameterProperty::Value)) {
                        auto value = param->value();
                        if (value.type().value() == libember::glow::ParameterType::String) {
                            QString labelValue = QString::fromStdString(value.toString());
                            
                            
                            bool isTargetLabel = (nodeNumber == 1);
                            
                            if (isTargetLabel) {
                                
                                EmberData::MatrixTargetInfo targetInfo;
                                targetInfo.matrixPath = matrixPath;
                                targetInfo.targetNumber = signalNumber;
                                targetInfo.label = labelValue;
                                
                                qDebug().noquote() << QString("EMBER+ TARGET LABEL: Matrix %1, target %2, label '%3'")
                                    .arg(matrixPath).arg(signalNumber).arg(labelValue);
                                
                                emit matrixTargetReceived(targetInfo);
                            } else {
                                
                                EmberData::MatrixSourceInfo sourceInfo;
                                sourceInfo.matrixPath = matrixPath;
                                sourceInfo.sourceNumber = signalNumber;
                                sourceInfo.label = labelValue;
                                
                                qDebug().noquote() << QString("EMBER+ SOURCE LABEL: Matrix %1, source %2, label '%3'")
                                    .arg(matrixPath).arg(signalNumber).arg(labelValue);
                                
                                 emit matrixSourceReceived(sourceInfo);
                            }
                        }
                    }
                }
                
                
                return;
            }
        }
    }
    
    
    bool hasIdentifier = param->contains(libember::glow::ParameterProperty::Identifier);
    bool hadIdentifier = m_parametersWithIdentifier.value(info.path, false);
    
    info.identifier = hasIdentifier
        ? QString::fromStdString(param->identifier())
        : QString("Parameter %1").arg(info.number);
    
    
    if (hasIdentifier) {
        m_parametersWithIdentifier[info.path] = true;
    }
    
    
    if (hadIdentifier && !hasIdentifier) {
        qDebug() << "[GlowParser] SKIPPING Parameter:" << info.path
                 << "- would replace valid identifier with stub";
        return;
    }
    
    info.description = param->contains(libember::glow::ParameterProperty::Description)
        ? QString::fromStdString(param->description())
        : QString();
    
    
    if (param->contains(libember::glow::ParameterProperty::Value)) {
        auto value = param->value();
        if (value.type().value() == libember::glow::ParameterType::Integer) {
            info.value = QString::number(value.toInteger());
        } else if (value.type().value() == libember::glow::ParameterType::Real) {
            info.value = QString::number(value.toReal());
        } else if (value.type().value() == libember::glow::ParameterType::String) {
            info.value = QString::fromStdString(value.toString());
        } else if (value.type().value() == libember::glow::ParameterType::Boolean) {
            info.value = value.toBoolean() ? "true" : "false";
        }
    }
    
    
    info.access = param->contains(libember::glow::ParameterProperty::Access)
        ? param->access().value()
        : 3;
    
    info.type = param->contains(libember::glow::ParameterProperty::Type)
        ? param->type().value()
        : 0;
    
    if (param->contains(libember::glow::ParameterProperty::Minimum)) {
        auto min = param->minimum();
        if (min.type().value() == libember::glow::ParameterType::Integer) {
            info.minimum = QVariant(static_cast<int>(min.toInteger()));
        } else if (min.type().value() == libember::glow::ParameterType::Real) {
            info.minimum = QVariant(min.toReal());
        }
    }
    
    if (param->contains(libember::glow::ParameterProperty::Maximum)) {
        auto max = param->maximum();
        if (max.type().value() == libember::glow::ParameterType::Integer) {
            info.maximum = QVariant(static_cast<int>(max.toInteger()));
        } else if (max.type().value() == libember::glow::ParameterType::Real) {
            info.maximum = QVariant(max.toReal());
        }
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::EnumMap)) {
        auto enumMap = param->enumerationMap();
        for (auto it = enumMap.begin(); it != enumMap.end(); ++it) {
            info.enumOptions.append(QString::fromStdString((*it).first));
            info.enumValues.append((*it).second);
        }
    }
    else if (param->contains(libember::glow::ParameterProperty::Enumeration)) {
        auto enumeration = param->enumeration();
        for (auto it = enumeration.begin(); it != enumeration.end(); ++it) {
            info.enumOptions.append(QString::fromStdString((*it).first));
            info.enumValues.append((*it).second);
        }
    }
    
    info.isOnline = param->contains(libember::glow::ParameterProperty::IsOnline)
        ? param->isOnline()
        : true;
    
    info.streamIdentifier = param->streamIdentifier();
    
    
    if (param->contains(libember::glow::ParameterProperty::Format)) {
        info.format = QString::fromStdString(param->format());
        info.referenceLevel = detectReferenceLevel(info.format);
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::Formula)) {
        auto formula = param->formula();
        
        info.formula = QString::fromStdString(formula.providerToConsumer());
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::Factor)) {
        info.factor = param->factor();
    } else {
        info.factor = 1;  
    }
    
    
    if (info.streamIdentifier > 0 && info.factor > 0) {
        m_streamFactors[info.streamIdentifier] = info.factor;
        qDebug() << "[GlowParser] Stored factor" << info.factor << "for stream ID" << info.streamIdentifier;
    }
    
    
    if (info.streamIdentifier > 0) {
        qDebug() << "[GlowParser] PPM Parameter (qualified):" << info.path 
                 << "identifier=" << info.identifier 
                 << "streamId=" << info.streamIdentifier
                 << "type=" << info.type
                 << "value=" << info.value
                 << "min=" << info.minimum
                 << "max=" << info.maximum
                 << "format=" << info.format
                 << "reference=" << info.referenceLevel
                 << "formula=" << info.formula
                 << "factor=" << info.factor;
    }
    
    emit parameterReceived(info);
    
    
    if (param->children()) {
        processElementCollection(param->children(), info.path);
    }
}

void GlowParser::processParameter(libember::glow::GlowParameter* param, const QString& parentPath)
{
    EmberData::ParameterInfo info;
    
    info.number = param->number();
    info.path = parentPath.isEmpty() 
        ? QString::number(info.number) 
        : QString("%1.%2").arg(parentPath).arg(info.number);
    
    
    for (auto it = m_matrixLabelPaths.begin(); it != m_matrixLabelPaths.end(); ++it) {
        const QString& matrixPath = it.key();
        const MatrixLabelPaths& labelPaths = it.value();
        
        for (auto labelIt = labelPaths.labelBasePaths.begin(); labelIt != labelPaths.labelBasePaths.end(); ++labelIt) {
            const QString& basePath = labelIt.key();
            
            if (info.path.startsWith(basePath + ".")) {
                QString remaining = info.path.mid(basePath.length() + 1);
                QStringList parts = remaining.split('.');
                
                static int debugLabelCount = 0;
                if (debugLabelCount < 10) {
                    qDebug().noquote() << QString("LABEL DEBUG: path=%1, basePath=%2, remaining=%3, parts=%4")
                        .arg(info.path).arg(basePath).arg(remaining).arg(parts.join(","));
                    debugLabelCount++;
                }
                
                if (parts.size() >= 2 && param->contains(libember::glow::ParameterProperty::Value)) {
                    int nodeNumber = parts[0].toInt();
                    int signalNumber = parts[1].toInt();
                    
                    auto value = param->value();
                    if (value.type().value() == libember::glow::ParameterType::String) {
                        QString labelValue = QString::fromStdString(value.toString());
                        
                        static int valueDebugCount = 0;
                        if (valueDebugCount < 10) {
                            qDebug().noquote() << QString("VALUE DEBUG: paramNumber=%1, nodeNum=%2, signalNum=%3, value='%4'")
                                .arg(param->number()).arg(nodeNumber).arg(signalNumber).arg(labelValue);
                            valueDebugCount++;
                        }
                        
                        bool isTargetLabel = (nodeNumber == 1);
                        
                        if (isTargetLabel) {
                            EmberData::MatrixTargetInfo targetInfo;
                            targetInfo.matrixPath = matrixPath;
                            targetInfo.targetNumber = signalNumber;
                            targetInfo.label = labelValue;
                            qDebug().noquote() << QString("EMBER+ TARGET LABEL: Matrix %1, target %2, label '%3'")
                                .arg(matrixPath).arg(signalNumber).arg(labelValue);
                            emit matrixTargetReceived(targetInfo);
                        } else {
                            EmberData::MatrixSourceInfo sourceInfo;
                            sourceInfo.matrixPath = matrixPath;
                            sourceInfo.sourceNumber = signalNumber;
                            sourceInfo.label = labelValue;
                            qDebug().noquote() << QString("EMBER+ SOURCE LABEL: Matrix %1, source %2, label '%3'")
                                .arg(matrixPath).arg(signalNumber).arg(labelValue);
                            emit matrixSourceReceived(sourceInfo);
                        }
                    }
                }
                return;
            }
        }
    }
    
    
    bool hasIdentifier = param->contains(libember::glow::ParameterProperty::Identifier);
    bool hadIdentifier = m_parametersWithIdentifier.value(info.path, false);
    
    info.identifier = hasIdentifier
        ? QString::fromStdString(param->identifier())
        : QString("Parameter %1").arg(info.number);
    
    
    if (hasIdentifier) {
        m_parametersWithIdentifier[info.path] = true;
    }
    
    if (hadIdentifier && !hasIdentifier) {
        qDebug() << "[GlowParser] SKIPPING Parameter:" << info.path
                 << "- would replace valid identifier with stub";
        return;
    }
    
    info.description = param->contains(libember::glow::ParameterProperty::Description)
        ? QString::fromStdString(param->description())
        : QString();
    
    if (param->contains(libember::glow::ParameterProperty::Value)) {
        auto value = param->value();
        if (value.type().value() == libember::glow::ParameterType::Integer) {
            info.value = QString::number(value.toInteger());
        } else if (value.type().value() == libember::glow::ParameterType::Real) {
            info.value = QString::number(value.toReal());
        } else if (value.type().value() == libember::glow::ParameterType::String) {
            info.value = QString::fromStdString(value.toString());
        } else if (value.type().value() == libember::glow::ParameterType::Boolean) {
            info.value = value.toBoolean() ? "true" : "false";
        }
    }
    
    
    info.access = param->contains(libember::glow::ParameterProperty::Access)
        ? param->access().value()
        : 3;
    
    info.type = param->contains(libember::glow::ParameterProperty::Type)
        ? param->type().value()
        : 0;
    
    if (param->contains(libember::glow::ParameterProperty::Minimum)) {
        auto min = param->minimum();
        if (min.type().value() == libember::glow::ParameterType::Integer) {
            info.minimum = QVariant(static_cast<int>(min.toInteger()));
        } else if (min.type().value() == libember::glow::ParameterType::Real) {
            info.minimum = QVariant(min.toReal());
        }
    }
    
    if (param->contains(libember::glow::ParameterProperty::Maximum)) {
        auto max = param->maximum();
        if (max.type().value() == libember::glow::ParameterType::Integer) {
            info.maximum = QVariant(static_cast<int>(max.toInteger()));
        } else if (max.type().value() == libember::glow::ParameterType::Real) {
            info.maximum = QVariant(max.toReal());
        }
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::EnumMap)) {
        auto enumMap = param->enumerationMap();
        for (auto it = enumMap.begin(); it != enumMap.end(); ++it) {
            info.enumOptions.append(QString::fromStdString((*it).first));
            info.enumValues.append((*it).second);
        }
    }
    else if (param->contains(libember::glow::ParameterProperty::Enumeration)) {
        auto enumeration = param->enumeration();
        for (auto it = enumeration.begin(); it != enumeration.end(); ++it) {
            info.enumOptions.append(QString::fromStdString((*it).first));
            info.enumValues.append((*it).second);
        }
    }
    
    info.isOnline = param->contains(libember::glow::ParameterProperty::IsOnline)
        ? param->isOnline()
        : true;
    
    info.streamIdentifier = param->streamIdentifier();
    
    
    if (param->contains(libember::glow::ParameterProperty::Format)) {
        info.format = QString::fromStdString(param->format());
        info.referenceLevel = detectReferenceLevel(info.format);
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::Formula)) {
        auto formula = param->formula();
        
        info.formula = QString::fromStdString(formula.providerToConsumer());
    }
    
    
    if (param->contains(libember::glow::ParameterProperty::Factor)) {
        info.factor = param->factor();
    } else {
        info.factor = 1;  
    }
    
    
    if (info.streamIdentifier > 0 && info.factor > 0) {
        m_streamFactors[info.streamIdentifier] = info.factor;
        qDebug() << "[GlowParser] Stored factor" << info.factor << "for stream ID" << info.streamIdentifier;
    }
    
    
    if (info.streamIdentifier > 0) {
        qDebug() << "[GlowParser] PPM Parameter (unqualified):" << info.path 
                 << "identifier=" << info.identifier 
                 << "streamId=" << info.streamIdentifier
                 << "type=" << info.type
                 << "value=" << info.value
                 << "min=" << info.minimum
                 << "max=" << info.maximum
                 << "format=" << info.format
                 << "reference=" << info.referenceLevel
                 << "formula=" << info.formula
                 << "factor=" << info.factor;
    }
    
    emit parameterReceived(info);
    
    
    if (param->children()) {
        processElementCollection(param->children(), info.path);
    }
}

void GlowParser::processQualifiedMatrix(libember::glow::GlowQualifiedMatrix* matrix)
{
    
    auto path = matrix->path();
    QString pathStr;
    for (auto num : path) {
        pathStr += QString::number(num) + ".";
    }
    pathStr.chop(1);
    
    int number = path.back();
    
    
    bool hasMetadata = matrix->contains(libember::glow::MatrixProperty::Identifier) ||
                       matrix->contains(libember::glow::MatrixProperty::Description) ||
                       matrix->contains(libember::glow::MatrixProperty::Type) ||
                       matrix->contains(libember::glow::MatrixProperty::TargetCount) ||
                       matrix->contains(libember::glow::MatrixProperty::SourceCount);
    
    if (hasMetadata) {
        EmberData::MatrixInfo info;
        info.path = pathStr;
        info.number = number;
        
        info.identifier = matrix->contains(libember::glow::MatrixProperty::Identifier)
            ? QString::fromStdString(matrix->identifier())
            : QString("Matrix %1").arg(number);
        
        info.description = matrix->contains(libember::glow::MatrixProperty::Description)
            ? QString::fromStdString(matrix->description())
            : "";
        
        info.type = matrix->contains(libember::glow::MatrixProperty::Type)
            ? matrix->type().value()
            : 2;  
        
        info.targetCount = matrix->contains(libember::glow::MatrixProperty::TargetCount)
            ? matrix->targetCount()
            : 0;
        
        info.sourceCount = matrix->contains(libember::glow::MatrixProperty::SourceCount)
            ? matrix->sourceCount()
            : 0;
        
        emit matrixReceived(info);
    }
    
    
    if (matrix->labels()) {
        MatrixLabelPaths labelPaths;
        labelPaths.matrixPath = pathStr;
        
        
        std::vector<libember::glow::GlowLabel const*> labels;
        matrix->typedLabels(std::back_inserter(labels));
        
        for (auto label : labels) {
            
            auto basePath = label->basePath();
            QString basePathStr;
            for (auto num : basePath) {
                basePathStr += QString::number(num) + ".";
            }
            basePathStr.chop(1);  
            
            
            QString description = QString::fromStdString(label->description());
            
            
            labelPaths.labelBasePaths[basePathStr] = description;
            labelPaths.labelOrder.append(basePathStr);
            
            
            QString labelType = (labelPaths.labelOrder.size() == 1) ? "targets" : "sources";
            qDebug().noquote() << QString("Matrix %1: Found label layer '%2' (%3) at basePath %4")
                .arg(pathStr).arg(description).arg(labelType).arg(basePathStr);
        }
        
        
        if (!labelPaths.labelBasePaths.isEmpty()) {
            m_matrixLabelPaths[pathStr] = labelPaths;
            
            qDebug().noquote() << QString("STORED %1 label basePaths for matrix %2 (total matrices: %3)")
                .arg(labelPaths.labelBasePaths.size()).arg(pathStr).arg(m_matrixLabelPaths.size());
            
            
            qDebug().noquote() << QString("Emitting signal to request label parameters for matrix %1").arg(pathStr);
            emit matrixLabelPathsDiscovered(pathStr, labelPaths.labelOrder);
        }
    }
    
    
    if (matrix->targets()) {
        for (auto it = matrix->targets()->begin(); it != matrix->targets()->end(); ++it) {
            if (auto target = dynamic_cast<libember::glow::GlowTarget*>(&(*it))) {
                EmberData::MatrixTargetInfo targetInfo;
                targetInfo.matrixPath = pathStr;
                targetInfo.targetNumber = target->number();
                targetInfo.label = QString("Target %1").arg(targetInfo.targetNumber);
                
                emit matrixTargetReceived(targetInfo);
            }
        }
    }
    
    
    if (matrix->sources()) {
        for (auto it = matrix->sources()->begin(); it != matrix->sources()->end(); ++it) {
            if (auto source = dynamic_cast<libember::glow::GlowSource*>(&(*it))) {
                EmberData::MatrixSourceInfo sourceInfo;
                sourceInfo.matrixPath = pathStr;
                sourceInfo.sourceNumber = source->number();
                sourceInfo.label = QString("Source %1").arg(sourceInfo.sourceNumber);
                
                emit matrixSourceReceived(sourceInfo);
            }
        }
    }
    
    
    if (matrix->connections()) {
        for (auto it = matrix->connections()->begin(); it != matrix->connections()->end(); ++it) {
            if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*it))) {
                int targetNumber = connection->target();
                
                
                emit matrixTargetConnectionsCleared(pathStr, targetNumber);
                
                
                libember::ber::ObjectIdentifier sources = connection->sources();
                
                if (!sources.empty()) {
                    int disposition = connection->disposition().value();
                    
                    for (auto sourceIt = sources.begin(); sourceIt != sources.end(); ++sourceIt) {
                        EmberData::MatrixConnectionInfo connInfo;
                        connInfo.matrixPath = pathStr;
                        connInfo.targetNumber = targetNumber;
                        connInfo.sourceNumber = *sourceIt;
                        connInfo.connected = true;
                        connInfo.disposition = disposition;
                        
                        emit matrixConnectionReceived(connInfo);
                    }
                }
            }
        }
    }
    
    
    if (matrix->children()) {
        processElementCollection(matrix->children(), pathStr);
    }
}

void GlowParser::processMatrix(libember::glow::GlowMatrix* matrix, const QString& parentPath)
{
    int number = matrix->number();
    QString pathStr = parentPath.isEmpty() 
        ? QString::number(number) 
        : QString("%1.%2").arg(parentPath).arg(number);
    
    
    bool hasMetadata = matrix->contains(libember::glow::MatrixProperty::Identifier) ||
                       matrix->contains(libember::glow::MatrixProperty::Description) ||
                       matrix->contains(libember::glow::MatrixProperty::Type) ||
                       matrix->contains(libember::glow::MatrixProperty::TargetCount) ||
                       matrix->contains(libember::glow::MatrixProperty::SourceCount);
    
    if (hasMetadata) {
        EmberData::MatrixInfo info;
        info.path = pathStr;
        info.number = number;
        
        info.identifier = matrix->contains(libember::glow::MatrixProperty::Identifier)
            ? QString::fromStdString(matrix->identifier())
            : QString("Matrix %1").arg(number);
        
        info.description = matrix->contains(libember::glow::MatrixProperty::Description)
            ? QString::fromStdString(matrix->description())
            : "";
        
        info.type = matrix->contains(libember::glow::MatrixProperty::Type)
            ? matrix->type().value()
            : 2;
        
        info.targetCount = matrix->contains(libember::glow::MatrixProperty::TargetCount)
            ? matrix->targetCount()
            : 0;
        
        info.sourceCount = matrix->contains(libember::glow::MatrixProperty::SourceCount)
            ? matrix->sourceCount()
            : 0;
        
        emit matrixReceived(info);
    }
    
    
    if (matrix->targets()) {
        for (auto it = matrix->targets()->begin(); it != matrix->targets()->end(); ++it) {
            if (auto target = dynamic_cast<libember::glow::GlowTarget*>(&(*it))) {
                EmberData::MatrixTargetInfo targetInfo;
                targetInfo.matrixPath = pathStr;
                targetInfo.targetNumber = target->number();
                targetInfo.label = QString("Target %1").arg(targetInfo.targetNumber);
                
                emit matrixTargetReceived(targetInfo);
            }
        }
    }
    
    
    if (matrix->sources()) {
        for (auto it = matrix->sources()->begin(); it != matrix->sources()->end(); ++it) {
            if (auto source = dynamic_cast<libember::glow::GlowSource*>(&(*it))) {
                EmberData::MatrixSourceInfo sourceInfo;
                sourceInfo.matrixPath = pathStr;
                sourceInfo.sourceNumber = source->number();
                sourceInfo.label = QString("Source %1").arg(sourceInfo.sourceNumber);
                
                emit matrixSourceReceived(sourceInfo);
            }
        }
    }
    
    
    if (matrix->connections()) {
        for (auto it = matrix->connections()->begin(); it != matrix->connections()->end(); ++it) {
            if (auto connection = dynamic_cast<libember::glow::GlowConnection*>(&(*it))) {
                int targetNumber = connection->target();
                
                
                emit matrixTargetConnectionsCleared(pathStr, targetNumber);
                
                
                libember::ber::ObjectIdentifier sources = connection->sources();
                
                if (!sources.empty()) {
                    int disposition = connection->disposition().value();
                    
                    for (auto sourceIt = sources.begin(); sourceIt != sources.end(); ++sourceIt) {
                        EmberData::MatrixConnectionInfo connInfo;
                        connInfo.matrixPath = pathStr;
                        connInfo.targetNumber = targetNumber;
                        connInfo.sourceNumber = *sourceIt;
                        connInfo.connected = true;
                        connInfo.disposition = disposition;
                        
                        emit matrixConnectionReceived(connInfo);
                    }
                }
            }
        }
    }
    
    
    if (matrix->children()) {
        processElementCollection(matrix->children(), pathStr);
    }
}

void GlowParser::processQualifiedFunction(libember::glow::GlowQualifiedFunction* function)
{
    EmberData::FunctionInfo info;
    
    
    auto path = function->path();
    for (auto num : path) {
        info.path += QString::number(num) + ".";
    }
    info.path.chop(1);
    
    info.identifier = function->contains(libember::glow::FunctionProperty::Identifier)
        ? QString::fromStdString(function->identifier())
        : QString("Function");
    
    info.description = function->contains(libember::glow::FunctionProperty::Description)
        ? QString::fromStdString(function->description())
        : "";
    
    
    if (function->contains(libember::glow::FunctionProperty::Arguments)) {
        auto args = function->arguments();
        for (auto it = args->begin(); it != args->end(); ++it) {
            if (auto arg = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                info.argNames.append(QString::fromStdString(arg->name()));
                info.argTypes.append(arg->type().value());
            }
        }
    }
    
    if (function->contains(libember::glow::FunctionProperty::Result)) {
        auto results = function->result();
        for (auto it = results->begin(); it != results->end(); ++it) {
            if (auto res = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                info.resultNames.append(QString::fromStdString(res->name()));
                info.resultTypes.append(res->type().value());
            }
        }
    }
    
    emit functionReceived(info);
    
    
    if (function->children()) {
        processElementCollection(function->children(), info.path);
    }
}

void GlowParser::processFunction(libember::glow::GlowFunction* function, const QString& parentPath)
{
    EmberData::FunctionInfo info;
    
    int number = function->number();
    info.path = parentPath.isEmpty() 
        ? QString::number(number) 
        : QString("%1.%2").arg(parentPath).arg(number);
    
    info.identifier = function->contains(libember::glow::FunctionProperty::Identifier)
        ? QString::fromStdString(function->identifier())
        : QString("Function");
    
    info.description = function->contains(libember::glow::FunctionProperty::Description)
        ? QString::fromStdString(function->description())
        : "";
    
    
    if (function->contains(libember::glow::FunctionProperty::Arguments)) {
        auto args = function->arguments();
        for (auto it = args->begin(); it != args->end(); ++it) {
            if (auto arg = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                info.argNames.append(QString::fromStdString(arg->name()));
                info.argTypes.append(arg->type().value());
            }
        }
    }
    
    if (function->contains(libember::glow::FunctionProperty::Result)) {
        auto results = function->result();
        for (auto it = results->begin(); it != results->end(); ++it) {
            if (auto res = dynamic_cast<const libember::glow::GlowTupleItemDescription*>(&(*it))) {
                info.resultNames.append(QString::fromStdString(res->name()));
                info.resultTypes.append(res->type().value());
            }
        }
    }
    
    emit functionReceived(info);
    
    
    if (function->children()) {
        processElementCollection(function->children(), info.path);
    }
}

void GlowParser::processInvocationResult(libember::dom::Node* result)
{
    if (auto invResult = dynamic_cast<libember::glow::GlowInvocationResult*>(result)) {
        EmberData::InvocationResult info;
        info.invocationId = invResult->invocationId();
        info.success = invResult->success();
        
        
        if (invResult->result()) {
            std::vector<libember::glow::Value> values;
            invResult->typedResult(std::back_inserter(values));
            
            for (const auto& val : values) {
                switch (val.type().value()) {
                    case libember::glow::ParameterType::Integer:
                        info.results.append(QVariant::fromValue(val.toInteger()));
                        break;
                    case libember::glow::ParameterType::Real:
                        info.results.append(QVariant::fromValue(val.toReal()));
                        break;
                    case libember::glow::ParameterType::String:
                        info.results.append(QString::fromStdString(val.toString()));
                        break;
                    case libember::glow::ParameterType::Boolean:
                        info.results.append(val.toBoolean());
                        break;
                    default:
                        info.results.append(QVariant());
                        break;
                }
            }
        }
        
        emit invocationResultReceived(info);
    }
}

void GlowParser::processStreamCollection(libember::glow::GlowContainer* streamCollection)
{
    qDebug() << "[GlowParser] Processing StreamCollection with" << streamCollection->size() << "entries";
    
    for (auto it = streamCollection->begin(); it != streamCollection->end(); ++it) {
        if (auto streamEntry = dynamic_cast<libember::glow::GlowStreamEntry*>(&(*it))) {
            EmberData::StreamValue info;
            info.streamIdentifier = streamEntry->streamIdentifier();
            
            
            
            
            auto value = streamEntry->value();
            double rawValue = 0.0;
            
            switch (value.type().value()) {
                case libember::glow::ParameterType::Integer:
                    rawValue = static_cast<double>(value.toInteger());
                    break;
                case libember::glow::ParameterType::Real:
                    rawValue = value.toReal();
                    break;
                default:
                    rawValue = 0.0;
                    break;
            }
            
            
            int factor = m_streamFactors.value(info.streamIdentifier, 1);
            
            
            if (factor > 0) {
                info.value = rawValue / static_cast<double>(factor);
            } else {
                info.value = rawValue;  
            }
            
            qDebug() << "[GlowParser] StreamEntry: streamId=" << info.streamIdentifier 
                     << "rawValue=" << rawValue << "factor=" << factor << "dB=" << info.value;
            
            emit streamValueReceived(info);
        }
    }
}

void GlowParser::onItemReady(libember::dom::Node* node)
{
    // This is called for each item as it's decoded in the stream
    // Process top-level items immediately without waiting for complete messages
    
    if (!node) {
        return;
    }
    
    try {
        // Check if this is a qualified parameter (label value)
        if (auto qparam = dynamic_cast<libember::glow::GlowQualifiedParameter*>(node)) {
            processQualifiedParameter(qparam);
        }
        // Check if this is a qualified node
        else if (auto qnode = dynamic_cast<libember::glow::GlowQualifiedNode*>(node)) {
            processQualifiedNode(qnode);
        }
        // Check if this is a qualified matrix
        else if (auto qmatrix = dynamic_cast<libember::glow::GlowQualifiedMatrix*>(node)) {
            processQualifiedMatrix(qmatrix);
        }
        // Check if this is a qualified function
        else if (auto qfunction = dynamic_cast<libember::glow::GlowQualifiedFunction*>(node)) {
            processQualifiedFunction(qfunction);
        }
        // For other types, we'll still process them in the traditional way when root is ready
    } catch (const std::exception& e) {
        qDebug() << "[GlowParser] Error in streaming item processing:" << e.what();
    }
}

QString GlowParser::detectReferenceLevel(const QString& formatString) const
{
    if (formatString.isEmpty()) {
        return QString();
    }
    
    
    QString upper = formatString.toUpper();
    
    if (upper.contains("DBFS")) {
        return "dBFS";  
    }
    else if (upper.contains("DBTP") || upper.contains("DB TP")) {
        return "dBTP";  
    }
    else if (upper.contains("DBR")) {
        return "dBr";   
    }
    else if (upper.contains("DBU")) {
        return "dBu";   
    }
    else if (upper.contains("DBV")) {
        return "dBV";   
    }
    else if (upper.contains("VU")) {
        return "VU";    
    }
    else if (upper.contains("PPM")) {
        return "PPM";   
    }
    else if (upper.contains("LUFS") || upper.contains("LU")) {
        return "LUFS";  
    }
    
    else if (upper.contains("DB")) {
        return "dB";    
    }
    
    return QString();
}
