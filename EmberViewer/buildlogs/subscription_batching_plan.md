# Subscription Batching Implementation Plan

## Overview

Implement batched subscription commands to reduce network round trips when subscribing to multiple Ember+ elements. This follows the same proven pattern as the existing `sendBatchGetDirectory()` implementation.

**Expected Impact:**
- 5-20x reduction in subscription-related network packets
- 50-90% latency reduction for tree exploration
- Better perceived performance during user interaction

**Implementation Time:** 3-4 hours
**Risk Level:** Low (follows proven pattern)
**Complexity:** Medium

---

## Architecture

### Current Behavior
```
User expands node with 10 children
  ↓
10 individual subscribeToNode/Parameter/Matrix calls
  ↓
10 separate S101 frames sent
  ↓
10 × network RTT delay
```

### New Behavior
```
User expands node with 10 children
  ↓
Collect all subscription requests
  ↓
1 call to sendBatchSubscribe()
  ↓
1 S101 frame with multiple Subscribe commands
  ↓
1 × network RTT delay
```

---

## Implementation Steps

### Step 1: Define Data Structures ✅

**File:** `EmberViewer/include/EmberConnection.h`

Add new struct and method declaration:

```cpp
// After line 66 (in public section)
struct SubscriptionRequest {
    QString path;
    QString type;  // "Node", "Parameter", "Matrix", "Function"
};

// After line 91 (with other sendBatch methods)
void sendBatchSubscribe(const QList<SubscriptionRequest>& requests);
```

**Location in file:** Add to public interface section, near existing subscription methods.

---

### Step 2: Implement sendBatchSubscribe() ✅

**File:** `EmberViewer/src/EmberConnection.cpp`

**Insert after:** `unsubscribeFromMatrix()` method (around line 2240)

**Implementation:**

```cpp
void EmberConnection::sendBatchSubscribe(const QList<SubscriptionRequest>& requests)
{
    if (requests.isEmpty()) {
        return;
    }
    
    if (!m_socket || !m_connected) {
        log(LogLevel::Warning, "Cannot batch subscribe - not connected");
        return;
    }
    
    // Filter out already-subscribed paths
    QList<SubscriptionRequest> toSubscribe;
    for (const auto& req : requests) {
        if (!m_subscriptions.contains(req.path)) {
            toSubscribe.append(req);
        } else {
            log(LogLevel::Debug, QString("Skipping duplicate subscription for %1").arg(req.path));
        }
    }
    
    if (toSubscribe.isEmpty()) {
        log(LogLevel::Debug, "All paths already subscribed, skipping batch");
        return;
    }
    
    try {
        log(LogLevel::Debug, QString("Batch subscribing to %1 paths...").arg(toSubscribe.size()));
        
        // OPTIMIZATION: Batch multiple Subscribe commands in single S101 frame
        // This reduces protocol overhead and network round trips
        auto root = new libember::glow::GlowRootElementCollection();
        
        for (const auto& req : toSubscribe) {
            // Parse path to OID
            QStringList segments = req.path.split('.', Qt::SkipEmptyParts);
            libember::ber::ObjectIdentifier oid;
            for (const QString& seg : segments) {
                oid.push_back(seg.toInt());
            }
            
            // Create appropriate qualified element based on type
            if (req.type == "Node") {
                auto node = new libember::glow::GlowQualifiedNode(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                node->children()->insert(node->children()->end(), cmd);
                root->insert(root->end(), node);
            }
            else if (req.type == "Parameter") {
                auto param = new libember::glow::GlowQualifiedParameter(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                param->children()->insert(param->children()->end(), cmd);
                root->insert(root->end(), param);
            }
            else if (req.type == "Matrix") {
                auto matrix = new libember::glow::GlowQualifiedMatrix(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                matrix->children()->insert(matrix->children()->end(), cmd);
                root->insert(root->end(), matrix);
            }
            else if (req.type == "Function") {
                auto function = new libember::glow::GlowQualifiedFunction(oid);
                auto cmd = new libember::glow::GlowCommand(libember::glow::CommandType::Subscribe);
                function->children()->insert(function->children()->end(), cmd);
                root->insert(root->end(), function);
            }
            else {
                log(LogLevel::Warning, QString("Unknown subscription type: %1 for path %2")
                    .arg(req.type).arg(req.path));
                continue;
            }
            
            // Track subscription
            SubscriptionState state;
            state.subscribedAt = QDateTime::currentDateTime();
            state.autoSubscribed = true;
            m_subscriptions[req.path] = state;
        }
        
        // Encode to BER
        libember::util::OctetStream stream;
        root->encode(stream);
        
        // Encode to S101
        auto encoder = libs101::StreamEncoder<unsigned char>();
        encoder.encode(0x00);  // Slot
        encoder.encode(libs101::MessageType::EmBER);
        encoder.encode(libs101::CommandType::EmBER);
        encoder.encode(0x01);  // Version
        encoder.encode(libs101::PackageFlag::FirstPackage | libs101::PackageFlag::LastPackage);
        encoder.encode(0x01);  // DTD
        encoder.encode(0x00);  // AppBytes
        encoder.encode(stream.begin(), stream.end());
        encoder.finish();
        
        // Send
        std::vector<unsigned char> data(encoder.begin(), encoder.end());
        QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());
        m_socket->write(qdata);
        m_socket->flush();
        
        log(LogLevel::Debug, QString("Batch subscribed to %1 paths").arg(toSubscribe.size()));
        
        delete root;
    }
    catch (const std::exception &ex) {
        log(LogLevel::Error, QString("Error sending batch subscribe: %1").arg(ex.what()));
    }
}
```

**Key points:**
- Similar structure to `sendBatchGetDirectory()`
- Handles Node, Parameter, Matrix, and Function types
- Deduplicates already-subscribed paths
- Tracks subscriptions in `m_subscriptions` map
- Proper error handling and logging

---

### Step 3: Modify UI to Use Batch Subscription ✅

**File:** `EmberViewer/src/MainWindow.cpp`

**Method to modify:** `onItemExpanded()` (around line 1497)

**Current code:**
```cpp
// Subscription handling (existing logic) - lines 1559-1576
if (!m_subscribedPaths.contains(path)) {
    if (type == "Node") {
        m_connection->subscribeToNode(path, true);
    } else if (type == "Parameter") {
        m_connection->subscribeToParameter(path, true);
    } else if (type == "Matrix") {
        m_connection->subscribeToMatrix(path, true);
    }
    
    if (!type.isEmpty()) {
        m_subscribedPaths.insert(path);
        SubscriptionState state;
        state.subscribedAt = QDateTime::currentDateTime();
        state.autoSubscribed = true;
        m_subscriptionStates[path] = state;
    }
}
```

**New code:**
```cpp
// OPTIMIZATION: Batch subscription - collect all subscriptions needed
QList<EmberConnection::SubscriptionRequest> subscriptions;

// Subscribe to the expanded item itself
if (!m_subscribedPaths.contains(path) && !type.isEmpty()) {
    subscriptions.append({path, type});
    m_subscribedPaths.insert(path);
    SubscriptionState state;
    state.subscribedAt = QDateTime::currentDateTime();
    state.autoSubscribed = true;
    m_subscriptionStates[path] = state;
}

// Subscribe to all immediate children (they're now visible)
for (int i = 0; i < item->childCount(); i++) {
    QTreeWidgetItem *child = item->child(i);
    QString childPath = child->data(0, Qt::UserRole).toString();
    QString childType = child->text(1);
    
    if (!childPath.isEmpty() && !childType.isEmpty() && 
        !m_subscribedPaths.contains(childPath) &&
        childType != "Loading...") {  // Skip placeholder items
        
        subscriptions.append({childPath, childType});
        m_subscribedPaths.insert(childPath);
        SubscriptionState state;
        state.subscribedAt = QDateTime::currentDateTime();
        state.autoSubscribed = true;
        m_subscriptionStates[childPath] = state;
    }
}

// Send all subscriptions in one batch
if (!subscriptions.isEmpty()) {
    if (subscriptions.size() == 1) {
        // Single subscription - use existing methods for simplicity
        const auto& req = subscriptions.first();
        if (req.type == "Node") {
            m_connection->subscribeToNode(req.path, true);
        } else if (req.type == "Parameter") {
            m_connection->subscribeToParameter(req.path, true);
        } else if (req.type == "Matrix") {
            m_connection->subscribeToMatrix(req.path, true);
        }
    } else {
        // Multiple subscriptions - use batch API
        qDebug().noquote() << QString("Batch subscribing to %1 paths (expanded: %2)")
            .arg(subscriptions.size()).arg(path);
        m_connection->sendBatchSubscribe(subscriptions);
    }
}
```

**Rationale:**
- Collects subscriptions for parent + all visible children
- Uses batch API only when multiple subscriptions needed (>1)
- Maintains existing single-subscription code path for simplicity
- Tracks all subscriptions in local state before sending

---

### Step 4: Update subscribeToExpandedItems() ✅

**File:** `EmberViewer/src/MainWindow.cpp`

**Method:** `subscribeToExpandedItems()` (around line 1608)

This method is called after initial tree population. It should also use batching:

**Current code:**
```cpp
void MainWindow::subscribeToExpandedItems()
{
    // OPTIMIZATION: Tree updates no longer disabled with lazy loading
    // This function now only handles subscriptions for expanded items
    
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->isExpanded()) {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            QString type = (*it)->text(1);
            
            if (!path.isEmpty() && !m_subscribedPaths.contains(path)) {
                SubscriptionState state;
                state.subscribedAt = QDateTime::currentDateTime();
                state.autoSubscribed = true;
                
                if (type == "Node") {
                    m_connection->subscribeToNode(path, true);
                    m_subscribedPaths.insert(path);
                    m_subscriptionStates[path] = state;
                } else if (type == "Parameter") {
                    m_connection->subscribeToNode(path, true);
                    m_subscribedPaths.insert(path);
                    m_subscriptionStates[path] = state;
                } else if (type == "Matrix") {
                    m_connection->subscribeToMatrix(path, true);
                    m_subscribedPaths.insert(path);
                    m_subscriptionStates[path] = state;
                }
            }
        }
        ++it;
    }
}
```

**New code:**
```cpp
void MainWindow::subscribeToExpandedItems()
{
    // OPTIMIZATION: Tree updates no longer disabled with lazy loading
    // This function now only handles subscriptions for expanded items
    // Using batch subscription for better performance
    
    QList<EmberConnection::SubscriptionRequest> subscriptions;
    
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->isExpanded()) {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            QString type = (*it)->text(1);
            
            if (!path.isEmpty() && !type.isEmpty() && !m_subscribedPaths.contains(path)) {
                subscriptions.append({path, type});
                m_subscribedPaths.insert(path);
                
                SubscriptionState state;
                state.subscribedAt = QDateTime::currentDateTime();
                state.autoSubscribed = true;
                m_subscriptionStates[path] = state;
            }
        }
        ++it;
    }
    
    if (!subscriptions.isEmpty()) {
        qDebug().noquote() << QString("Batch subscribing to %1 expanded items after tree population")
            .arg(subscriptions.size());
        m_connection->sendBatchSubscribe(subscriptions);
    }
}
```

---

### Step 5: Integration with Sibling Prefetching (Optional Enhancement) 🌟

**File:** `EmberViewer/src/MainWindow.cpp`

**Enhancement:** When prefetching siblings, also batch-subscribe to them.

**In `onItemExpanded()`, after sibling prefetch logic (around line 1554):**

```cpp
// After sendBatchGetDirectory for sibling prefetching...

// OPTIONAL ENHANCEMENT: Also prefetch subscriptions for sibling nodes
// When we fetch siblings, we know they'll be visible soon, so subscribe proactively
if (pathsToPrefetch.size() > 1) {  // We have siblings to prefetch
    for (const QString& siblingPath : pathsToPrefetch) {
        if (siblingPath != path && !m_subscribedPaths.contains(siblingPath)) {
            // We don't know the type yet, but we can assume "Node" for safety
            // The actual subscription will happen when the item is created
            // This is optional - can be added later if desired
        }
    }
}
```

**Note:** This enhancement is optional because we don't know sibling types until they're received. Can be deferred to Phase 2.

---

## Testing Plan

### Unit Testing (Manual)

1. **Single Node Expansion**
   - Expand a node with 5 children
   - Verify 1 batch subscription message in console log
   - Verify all children are subscribed

2. **Multiple Node Expansion**
   - Rapidly expand 3 sibling nodes
   - Verify 3 batch subscription messages
   - Verify all paths subscribed correctly

3. **Already Subscribed**
   - Collapse and re-expand same node
   - Verify no duplicate subscriptions sent
   - Check `m_subscriptions` map correctly tracks state

4. **Mixed Types**
   - Expand node with mix of Node/Parameter/Matrix children
   - Verify batch contains all types
   - Verify correct GlowQualified* types used

5. **Large Tree**
   - Connect to device with 100+ nodes
   - Expand root
   - Verify batch contains all root children
   - Monitor network traffic reduction

### Integration Testing

1. **Compare Before/After**
   - Measure time to explore 10 nodes (before optimization)
   - Measure time to explore 10 nodes (after optimization)
   - Expect 50-80% improvement

2. **Network Packet Analysis**
   - Use Wireshark to count S101 frames
   - Before: N frames for N subscriptions
   - After: 1 frame for N subscriptions

3. **Provider Compatibility**
   - Test with multiple Ember+ providers
   - Verify all accept batched subscriptions
   - Check for proper subscription responses

### Edge Cases

1. **Empty tree** - No subscriptions sent
2. **Single child** - Falls back to individual subscription (code optimization)
3. **Disconnection during batch** - Proper error handling
4. **Invalid path in batch** - Logs warning, continues with valid paths
5. **Very large batch (100+ items)** - Verify S101 frame size limits (should be fine)

---

## Performance Expectations

### Before (Individual Subscriptions)
```
Expand node with 10 children:
  - 10 network packets
  - 10 × 20ms RTT = 200ms total
  - User perceives delay
```

### After (Batched Subscriptions)
```
Expand node with 10 children:
  - 1 network packet
  - 1 × 20ms RTT = 20ms total
  - 180ms saved (900% faster!)
  - User perceives instant response
```

### Combined with Sibling Prefetching
```
Expand first sibling (prefetches 3 others):
  - 1 GetDirectory batch (4 paths)
  - 1 Subscribe batch (4 paths + ~40 children)
  - Total: 2 network packets for entire exploration
  - Next 3 sibling expansions: INSTANT (0ms, already fetched & subscribed)
```

---

## Risks and Mitigations

### Risk 1: Provider doesn't support batch subscriptions
**Likelihood:** Very Low
**Mitigation:** Ember+ spec explicitly allows multiple elements in GlowRootElementCollection. All compliant providers must support this.
**Fallback:** Individual subscriptions still work if batch fails.

### Risk 2: S101 frame size limit exceeded
**Likelihood:** Very Low  
**Mitigation:** Typical subscription batch = 10-20 items = ~2KB payload, well under limits.
**Fallback:** Could add max batch size limit (e.g., 50 items) if needed.

### Risk 3: Tracking state becomes inconsistent
**Likelihood:** Low
**Mitigation:** Track subscriptions before sending (optimistic), same as current code.
**Fallback:** Re-subscription is idempotent, safe to retry.

### Risk 4: Performance regression for single items
**Likelihood:** None
**Mitigation:** Code explicitly uses individual subscription for single items (size == 1).

---

## Code Review Checklist

- [ ] `SubscriptionRequest` struct added to EmberConnection.h
- [ ] `sendBatchSubscribe()` declared in EmberConnection.h
- [ ] `sendBatchSubscribe()` implemented in EmberConnection.cpp
- [ ] Proper error handling and logging
- [ ] `onItemExpanded()` modified to use batching
- [ ] `subscribeToExpandedItems()` modified to use batching
- [ ] Subscription tracking maintains consistency
- [ ] No duplicate subscriptions sent
- [ ] All element types supported (Node, Parameter, Matrix, Function)
- [ ] Code follows existing patterns (similar to sendBatchGetDirectory)
- [ ] Comments explain optimization rationale
- [ ] Console logs provide visibility into batching behavior

---

## Documentation Updates

1. **network_optimizations.md** - Add section on subscription batching
2. **Code comments** - Explain batching logic in implementation
3. **Git commit message** - Describe optimization and benefits

---

## Implementation Phases

### Phase 1: Core Implementation (2-3 hours)
- Steps 1-4: Data structures, sendBatchSubscribe(), UI modifications
- Basic testing with manual device

### Phase 2: Polish and Testing (1 hour)
- Comprehensive testing with multiple devices
- Performance measurements
- Documentation updates

### Phase 3: Optional Enhancements (Future)
- Integration with sibling prefetching subscriptions
- Adaptive batch sizing based on network conditions
- Metrics/telemetry for batch effectiveness

---

## Success Criteria

✅ **Functional:**
- Batch subscriptions work correctly for all element types
- No duplicate subscriptions sent
- Subscription state tracked accurately
- Compatible with all tested Ember+ providers

✅ **Performance:**
- 50-80% reduction in subscription-related latency
- 5-20x reduction in subscription network packets
- No performance regression for single-item subscriptions

✅ **Code Quality:**
- Follows existing code patterns
- Well-commented and maintainable
- Proper error handling
- No linter errors

---

## Next Steps

1. Begin Phase 1 implementation
2. Test with real Ember+ device after each step
3. Iterate based on findings
4. Commit when all tests pass
5. Document results in network_optimizations.md

---

*Plan created: October 30, 2025*
*Estimated completion: 3-4 hours*
*Status: Ready to implement*

