#ifndef STREAMINGDOMREADER_H
#define STREAMINGDOMREADER_H

#include <QObject>
#include <ember/dom/AsyncDomReader.hpp>
#include <functional>

/**
 * Custom AsyncDomReader that processes items as they arrive (streaming mode)
 * instead of waiting for the complete root to be ready.
 * 
 * This significantly improves performance when receiving large Ember+ messages
 * by allowing UI updates to happen incrementally rather than all at once.
 */
class StreamingDomReader : public libember::dom::AsyncDomReader
{
public:
    using ItemCallback = std::function<void(libember::dom::Node*)>;
    
    explicit StreamingDomReader(libember::dom::NodeFactory const& factory);
    virtual ~StreamingDomReader();
    
    /**
     * Set callback to be invoked when an item is decoded.
     * This allows processing items immediately without waiting for root.
     */
    void setItemReadyCallback(ItemCallback callback);
    
    /**
     * Set callback to be invoked when a container is decoded.
     */
    void setContainerReadyCallback(ItemCallback callback);
    
protected:
    /**
     * Called when a new item has been decoded.
     * We override this to process items immediately.
     */
    virtual void itemReady(libember::dom::Node* node) override;
    
    /**
     * Called when a new container has been decoded.
     */
    virtual void containerReady(libember::dom::Node* node) override;
    
private:
    ItemCallback m_itemReadyCallback;
    ItemCallback m_containerReadyCallback;
};

#endif // STREAMINGDOMREADER_H
