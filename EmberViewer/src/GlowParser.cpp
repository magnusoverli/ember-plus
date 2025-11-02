







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
#include <ember/glow/GlowStreamCollection.hpp>
#include <ember/glow/GlowStreamEntry.hpp>
#include <ember/glow/GlowInvocationResult.hpp>
#include <QVariant>

GlowParser::GlowParser(QObject *parent)
    : QObject(parent)
    , m_domReader(new libember::dom::AsyncDomReader(libember::glow::GlowNodeFactory::getFactory()))
{
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
            // Check if it's a standalone StreamCollection (for subscribed parameters)
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
    
    
    info.identifier = param->contains(libember::glow::ParameterProperty::Identifier)
        ? QString::fromStdString(param->identifier())
        : QString("Parameter %1").arg(info.number);
    
    
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
    
    // Parse format string if available
    if (param->contains(libember::glow::ParameterProperty::Format)) {
        info.format = QString::fromStdString(param->format());
        info.referenceLevel = detectReferenceLevel(info.format);
    }
    
    // Parse formula if available
    if (param->contains(libember::glow::ParameterProperty::Formula)) {
        auto formula = param->formula();
        // We want the providerToConsumer formula (device value -> display value)
        info.formula = QString::fromStdString(formula.providerToConsumer());
    }
    
    // Parse factor if available
    if (param->contains(libember::glow::ParameterProperty::Factor)) {
        info.factor = param->factor();
    } else {
        info.factor = 1;  // Default factor
    }
    
    // Store factor for stream value conversion
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
    
    // Process children (e.g., StreamCollection for subscribed parameters)
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
    
    
    info.identifier = param->contains(libember::glow::ParameterProperty::Identifier)
        ? QString::fromStdString(param->identifier())
        : QString("Parameter %1").arg(info.number);
    
    
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
    
    // Parse format string if available
    if (param->contains(libember::glow::ParameterProperty::Format)) {
        info.format = QString::fromStdString(param->format());
        info.referenceLevel = detectReferenceLevel(info.format);
    }
    
    // Parse formula if available
    if (param->contains(libember::glow::ParameterProperty::Formula)) {
        auto formula = param->formula();
        // We want the providerToConsumer formula (device value -> display value)
        info.formula = QString::fromStdString(formula.providerToConsumer());
    }
    
    // Parse factor if available
    if (param->contains(libember::glow::ParameterProperty::Factor)) {
        info.factor = param->factor();
    } else {
        info.factor = 1;  // Default factor
    }
    
    // Store factor for stream value conversion
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
    
    // Process children (e.g., StreamCollection for subscribed parameters)
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
            
            // Look up factor for this stream, default to 1 if not found
            int factor = m_streamFactors.value(info.streamIdentifier, 1);
            
            // Convert raw value using factor: dB = rawValue / factor
            if (factor > 0) {
                info.value = rawValue / static_cast<double>(factor);
            } else {
                info.value = rawValue;  // No conversion if factor is invalid
            }
            
            qDebug() << "[GlowParser] StreamEntry: streamId=" << info.streamIdentifier 
                     << "rawValue=" << rawValue << "factor=" << factor << "dB=" << info.value;
            
            emit streamValueReceived(info);
        }
    }
}

QString GlowParser::detectReferenceLevel(const QString& formatString) const
{
    if (formatString.isEmpty()) {
        return QString();
    }
    
    // Check for common reference level indicators in the format string
    QString upper = formatString.toUpper();
    
    if (upper.contains("DBFS")) {
        return "dBFS";  // Digital Full Scale
    }
    else if (upper.contains("DBTP") || upper.contains("DB TP")) {
        return "dBTP";  // True Peak
    }
    else if (upper.contains("DBR")) {
        return "dBr";   // DIN PPM reference
    }
    else if (upper.contains("DBU")) {
        return "dBu";   // Professional line level
    }
    else if (upper.contains("DBV")) {
        return "dBV";   // Voltage reference
    }
    else if (upper.contains("VU")) {
        return "VU";    // VU meter
    }
    else if (upper.contains("PPM")) {
        return "PPM";   // Generic PPM (could be DIN or BBC)
    }
    else if (upper.contains("LUFS") || upper.contains("LU")) {
        return "LUFS";  // Loudness Units relative to Full Scale
    }
    
    else if (upper.contains("DB")) {
        return "dB";    // Generic dB (no specific reference)
    }
    // No reference level detected
    return QString();
}
