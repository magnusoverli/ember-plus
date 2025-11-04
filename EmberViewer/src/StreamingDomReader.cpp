#include "StreamingDomReader.h"
#include <ember/dom/Node.hpp>

StreamingDomReader::StreamingDomReader(libember::dom::NodeFactory const& factory)
    : libember::dom::AsyncDomReader(factory)
    , m_itemReadyCallback(nullptr)
    , m_containerReadyCallback(nullptr)
{
}

StreamingDomReader::~StreamingDomReader()
{
}

void StreamingDomReader::setItemReadyCallback(ItemCallback callback)
{
    m_itemReadyCallback = callback;
}

void StreamingDomReader::setContainerReadyCallback(ItemCallback callback)
{
    m_containerReadyCallback = callback;
}

void StreamingDomReader::itemReady(libember::dom::Node* node)
{
    // Call the base implementation first
    AsyncDomReader::itemReady(node);
    
    // If we have a callback, invoke it with the decoded item
    // This allows processing items immediately as they arrive
    if (m_itemReadyCallback && node) {
        m_itemReadyCallback(node);
    }
}

void StreamingDomReader::containerReady(libember::dom::Node* node)
{
    // Call the base implementation first
    AsyncDomReader::containerReady(node);
    
    // If we have a callback, invoke it
    if (m_containerReadyCallback && node) {
        m_containerReadyCallback(node);
    }
}
