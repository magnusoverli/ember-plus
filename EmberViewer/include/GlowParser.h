/*
    EmberViewer - Glow Message Parser
    
    Parses Ember+ Glow DOM structures into data transfer objects.
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef GLOWPARSER_H
#define GLOWPARSER_H

#include <QObject>
#include "EmberDataTypes.h"
#include <ember/dom/AsyncDomReader.hpp>

// Forward declarations
namespace libember {
    namespace dom {
        class Node;
    }
    namespace glow {
        class GlowContainer;
        class GlowNode;
        class GlowQualifiedNode;
        class GlowParameter;
        class GlowQualifiedParameter;
        class GlowMatrix;
        class GlowQualifiedMatrix;
        class GlowFunction;
        class GlowQualifiedFunction;
    }
}

class GlowParser : public QObject
{
    Q_OBJECT

public:
    explicit GlowParser(QObject *parent = nullptr);
    ~GlowParser();

    // Parse Ember+ data into DOM and extract elements
    void parseEmberData(const QByteArray& data);

signals:
    // Parsed element signals
    void nodeReceived(const EmberData::NodeInfo& node);
    void parameterReceived(const EmberData::ParameterInfo& param);
    void matrixReceived(const EmberData::MatrixInfo& matrix);
    void matrixTargetReceived(const EmberData::MatrixTargetInfo& target);
    void matrixSourceReceived(const EmberData::MatrixSourceInfo& source);
    void matrixConnectionReceived(const EmberData::MatrixConnectionInfo& connection);
    void matrixTargetConnectionsCleared(const QString& matrixPath, int targetNumber);
    void functionReceived(const EmberData::FunctionInfo& function);
    void invocationResultReceived(const EmberData::InvocationResult& result);
    void streamValueReceived(const EmberData::StreamValue& streamValue);
    
    void parsingError(const QString& error);

private:
    
    void processRoot(libember::dom::Node* root);
    void processElementCollection(libember::glow::GlowContainer* container, const QString& parentPath);
    void processQualifiedNode(libember::glow::GlowQualifiedNode* node, bool emitNode = true);
    void processNode(libember::glow::GlowNode* node, const QString& parentPath, bool emitNode = true);
    void processQualifiedParameter(libember::glow::GlowQualifiedParameter* param);
    void processParameter(libember::glow::GlowParameter* param, const QString& parentPath);
    void processQualifiedMatrix(libember::glow::GlowQualifiedMatrix* matrix);
    void processMatrix(libember::glow::GlowMatrix* matrix, const QString& parentPath);
    void processQualifiedFunction(libember::glow::GlowQualifiedFunction* function);
    void processFunction(libember::glow::GlowFunction* function, const QString& parentPath);
    void processInvocationResult(libember::dom::Node* result);
    void processStreamCollection(libember::glow::GlowContainer* streamCollection);
    
    libember::dom::AsyncDomReader *m_domReader;
    
    // Track nodes that have been received with identifiers
    // to avoid emitting stub nodes that would overwrite good data
    QMap<QString, bool> m_nodesWithIdentifier;
};

#endif // GLOWPARSER_H
