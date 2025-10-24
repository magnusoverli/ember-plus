/*
 * MatrixWidget.cpp - Widget for displaying Ember+ matrix crosspoints
 */

#include "MatrixWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFontMetrics>
#include <QPair>
#include <QDebug>
#include <QEvent>
#include <QPalette>
#include <QColor>
#include <QScrollBar>
#include <QSplitter>
#include <QIcon>
#include <algorithm>

// Define static constants
const int MatrixWidget::BUTTON_SIZE;
const int MatrixWidget::GRID_SPACING;
const int MatrixWidget::LABEL_HEIGHT;
const int MatrixWidget::MAX_LABEL_WIDTH;

// RotatedLabel implementation
// RotatedLabel implementation
RotatedLabel::RotatedLabel(const QString &text, int buttonWidth, QWidget *parent)
    : QWidget(parent), m_fullText(text), m_displayText(text)
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    setFont(font);
    
    // Set fixed width (matches button width), but allow height to vary
    setFixedWidth(buttonWidth);
    setMinimumHeight(50);  // Minimum reasonable height
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    
    // Initial display text will be updated when widget is sized
    updateDisplayText();
}

void RotatedLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayText();
}

void RotatedLabel::updateDisplayText()
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    QFontMetrics fm(font);
    
    int textWidth = fm.horizontalAdvance(m_fullText);
    int availableHeight = height();  // After rotation, height becomes horizontal space
    
    // Truncate only if text exceeds available height
    if (textWidth > availableHeight) {
        m_displayText = fm.elidedText(m_fullText, Qt::ElideRight, availableHeight);
        setToolTip(m_fullText);  // Show full text on hover
    } else {
        m_displayText = m_fullText;
        setToolTip("");  // No tooltip needed if full text fits
    }
    
    update();  // Trigger repaint
}

void RotatedLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    painter.setFont(font);
    
    // Rotate around center of widget
    painter.save();
    painter.translate(width() / 2, height() / 2);
    painter.rotate(-90);
    
    // Draw text bottom-aligned (AlignLeft in rotated space = bottom on screen)
    QRect boundingRect(-height() / 2, -width() / 2, height(), width());
    painter.drawText(boundingRect, Qt::AlignLeft | Qt::AlignVCenter, m_displayText);
    
    painter.restore();
}



// SourceLabel implementation
SourceLabel::SourceLabel(const QString &text, int buttonHeight, QWidget *parent)
    : QWidget(parent), m_fullText(text), m_displayText(text)
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    setFont(font);
    
    // Set fixed height (matches button height), but allow width to vary
    setFixedHeight(buttonHeight);
    setMinimumWidth(50);  // Minimum reasonable width
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // Initial display text will be updated when widget is sized
    updateDisplayText();
}

void SourceLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateDisplayText();
}

void SourceLabel::updateDisplayText()
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    QFontMetrics fm(font);
    
    int textWidth = fm.horizontalAdvance(m_fullText);
    int availableWidth = width() - 4;  // Leave small padding
    
    // Truncate only if text exceeds available width
    if (textWidth > availableWidth) {
        m_displayText = fm.elidedText(m_fullText, Qt::ElideRight, availableWidth);
        setToolTip(m_fullText);  // Show full text on hover
    } else {
        m_displayText = m_fullText;
        setToolTip("");  // No tooltip needed if full text fits
    }
    
    update();  // Trigger repaint
}

void SourceLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    painter.setFont(font);
    
    // Draw text right-aligned, vertically centered
    QRect boundingRect(0, 0, width(), height());
    painter.drawText(boundingRect, Qt::AlignRight | Qt::AlignVCenter, m_displayText);
}
// MatrixWidget implementation
MatrixWidget::MatrixWidget(QWidget *parent)
    : QWidget(parent)
    , m_matrixType(2)  // Default to NToN
    , m_targetCount(0)
    , m_sourceCount(0)
    , m_headerLabel(nullptr)
    , m_outerVerticalSplitter(nullptr)
    , m_topHorizontalSplitter(nullptr)
    , m_bottomHorizontalSplitter(nullptr)
    , m_cornerWidget(nullptr)
    , m_targetHeaderScrollArea(nullptr)
    , m_sourcesSidebarScrollArea(nullptr)
    , m_buttonGridScrollArea(nullptr)
    , m_targetHeaderContainer(nullptr)
    , m_sourcesSidebarContainer(nullptr)
    , m_buttonGridContainer(nullptr)
    , m_targetHeaderLayout(nullptr)
    , m_sourcesSidebarLayout(nullptr)
    , m_buttonGridLayout(nullptr)
    , m_hoverTargetNumber(-1)
    , m_hoverSourceNumber(-1)
    , m_crosspointsEnabled(false)
    , m_userAdjustedHandles(false)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(10, 10, 10, 10);
    outerLayout->setSpacing(5);
    
    // Header label (above the frozen pane)
    m_headerLabel = new QLabel("Matrix", this);
    m_headerLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    outerLayout->addWidget(m_headerLabel);
    
    // Create outer vertical splitter (top row vs bottom row)
    m_outerVerticalSplitter = new QSplitter(Qt::Vertical, this);
    m_outerVerticalSplitter->setChildrenCollapsible(false);
    m_outerVerticalSplitter->setHandleWidth(6);
    m_outerVerticalSplitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    
    // === TOP ROW: Corner + Target Headers ===
    m_topHorizontalSplitter = new QSplitter(Qt::Horizontal);
    m_topHorizontalSplitter->setChildrenCollapsible(false);
    m_topHorizontalSplitter->setHandleWidth(6);
    m_topHorizontalSplitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    
    // [Top-Left] Corner widget - button to toggle crosspoint editing
    m_cornerWidget = new QPushButton();
    m_cornerWidget->setMinimumWidth(50);
    m_cornerWidget->setMaximumWidth(200);
    m_cornerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_cornerWidget->setStyleSheet("QPushButton { background-color: transparent; border: none; }");
    m_cornerWidget->setCursor(Qt::PointingHandCursor);
    m_cornerWidget->setToolTip("Toggle crosspoint editing mode");
    QIcon initialIcon(":/lock-closed.png");
    qDebug() << "Initial corner icon isNull:" << initialIcon.isNull() << "availableSizes:" << initialIcon.availableSizes();
    m_cornerWidget->setIcon(initialIcon);
    m_cornerWidget->setIconSize(QSize(32, 32));
    
    connect(m_cornerWidget, &QPushButton::clicked, this, &MatrixWidget::crosspointToggleRequested);
    
    // [Top-Right] Target header (horizontal scroll only)
    m_targetHeaderScrollArea = new QScrollArea();
    m_targetHeaderScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_targetHeaderScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_targetHeaderScrollArea->setWidgetResizable(true);
    m_targetHeaderScrollArea->setFrameShape(QFrame::NoFrame);
    m_targetHeaderScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_targetHeaderContainer = new QWidget();
    m_targetHeaderLayout = new QHBoxLayout(m_targetHeaderContainer);
    m_targetHeaderLayout->setSpacing(GRID_SPACING);
    m_targetHeaderLayout->setContentsMargins(0, 0, 0, 0);
    m_targetHeaderScrollArea->setWidget(m_targetHeaderContainer);
    
    m_topHorizontalSplitter->addWidget(m_cornerWidget);
    m_topHorizontalSplitter->addWidget(m_targetHeaderScrollArea);
    m_topHorizontalSplitter->setStretchFactor(0, 0);  // Corner doesn't stretch
    m_topHorizontalSplitter->setStretchFactor(1, 1);  // Headers stretch
    
    // === BOTTOM ROW: Source Sidebar + Button Grid ===
    m_bottomHorizontalSplitter = new QSplitter(Qt::Horizontal);
    m_bottomHorizontalSplitter->setChildrenCollapsible(false);
    m_bottomHorizontalSplitter->setHandleWidth(6);
    m_bottomHorizontalSplitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    
    // [Bottom-Left] Source sidebar (vertical scroll only)
    m_sourcesSidebarScrollArea = new QScrollArea();
    m_sourcesSidebarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sourcesSidebarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sourcesSidebarScrollArea->setWidgetResizable(true);
    m_sourcesSidebarScrollArea->setFrameShape(QFrame::NoFrame);
    m_sourcesSidebarScrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");
    m_sourcesSidebarScrollArea->setMinimumWidth(50);
    m_sourcesSidebarScrollArea->setMaximumWidth(200);
    m_sourcesSidebarScrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_sourcesSidebarContainer = new QWidget();
    m_sourcesSidebarLayout = new QVBoxLayout(m_sourcesSidebarContainer);
    m_sourcesSidebarLayout->setSpacing(GRID_SPACING);
    m_sourcesSidebarLayout->setContentsMargins(0, 0, 0, 0);
    m_sourcesSidebarScrollArea->setWidget(m_sourcesSidebarContainer);
    
    // [Bottom-Right] Button grid (both scrollbars)
    m_buttonGridScrollArea = new QScrollArea();
    m_buttonGridScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_buttonGridScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_buttonGridScrollArea->setWidgetResizable(false);
    m_buttonGridScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_buttonGridContainer = new QWidget();
    m_buttonGridLayout = new QGridLayout(m_buttonGridContainer);
    m_buttonGridLayout->setSpacing(GRID_SPACING);
    m_buttonGridLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonGridScrollArea->setWidget(m_buttonGridContainer);
    
    m_bottomHorizontalSplitter->addWidget(m_sourcesSidebarScrollArea);
    m_bottomHorizontalSplitter->addWidget(m_buttonGridScrollArea);
    m_bottomHorizontalSplitter->setStretchFactor(0, 0);  // Sidebar doesn't stretch
    m_bottomHorizontalSplitter->setStretchFactor(1, 1);  // Grid stretches
    
    // Add both rows to outer vertical splitter
    m_outerVerticalSplitter->addWidget(m_topHorizontalSplitter);
    m_outerVerticalSplitter->addWidget(m_bottomHorizontalSplitter);
    
    // Set initial sizes for vertical splitter (target headers smaller than button grid)
    m_outerVerticalSplitter->setMinimumHeight(200);
    m_outerVerticalSplitter->setStretchFactor(0, 0);  // Top row doesn't stretch much
    m_outerVerticalSplitter->setStretchFactor(1, 1);  // Bottom row stretches
    
    // Note: Don't call setSizes() here - it will be set in resizeEvent() 
    // to maintain static handle positions regardless of initial widget size
    
    // Add the splitter to outer layout
    outerLayout->addWidget(m_outerVerticalSplitter);
    
    // Set all child widgets to transparent background so parent background shows through
    m_outerVerticalSplitter->setStyleSheet(m_outerVerticalSplitter->styleSheet() + " QSplitter { background: transparent; }");
    m_topHorizontalSplitter->setStyleSheet(m_topHorizontalSplitter->styleSheet() + " QSplitter { background: transparent; }");
    m_bottomHorizontalSplitter->setStyleSheet(m_bottomHorizontalSplitter->styleSheet() + " QSplitter { background: transparent; }");
    m_targetHeaderScrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");
    m_targetHeaderContainer->setStyleSheet("QWidget { background-color: transparent; }");
    m_buttonGridScrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");
    m_buttonGridContainer->setStyleSheet("QWidget { background-color: transparent; }");
    m_sourcesSidebarContainer->setStyleSheet("QWidget { background-color: transparent; }");
    
    // Connect splitter synchronization
    connect(m_topHorizontalSplitter, &QSplitter::splitterMoved,
            this, &MatrixWidget::onTopSplitterMoved);
    connect(m_bottomHorizontalSplitter, &QSplitter::splitterMoved,
            this, &MatrixWidget::onBottomSplitterMoved);
    connect(m_outerVerticalSplitter, &QSplitter::splitterMoved,
            this, &MatrixWidget::onVerticalSplitterMoved);
    
    // Connect scroll synchronization
    connectScrollSync();
}

MatrixWidget::~MatrixWidget()
{
}

void MatrixWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // Only enforce static positions if user hasn't manually adjusted handles
    if (!m_userAdjustedHandles) {
        enforceStaticHandlePositions();
    }
}

void MatrixWidget::enforceStaticHandlePositions()
{
    // Get current widget dimensions
    int totalHeight = m_outerVerticalSplitter->height();
    int totalWidth = m_outerVerticalSplitter->width();
    
    // Calculate sizes to keep labels anchored to bottom with minimum spacing from top
    const int minLabelHeight = 50;
    const int minTopMargin = 10;  // Minimum space above labels
    
    // Default: give labels reasonable space
    int topSectionHeight = LABEL_HEIGHT + 2;  // Use default label height
    int bottomSectionHeight = totalHeight - topSectionHeight - m_outerVerticalSplitter->handleWidth();
    
    // Ensure bottom section has minimum size
    if (bottomSectionHeight < 100) {
        bottomSectionHeight = 100;
        topSectionHeight = totalHeight - bottomSectionHeight - m_outerVerticalSplitter->handleWidth();
    }
    
    // Ensure top section has minimum size
    if (topSectionHeight < minLabelHeight + minTopMargin) {
        topSectionHeight = minLabelHeight + minTopMargin;
    }
    
    QList<int> verticalSizes;
    verticalSizes << topSectionHeight << bottomSectionHeight;
    m_outerVerticalSplitter->setSizes(verticalSizes);
    
    // Calculate horizontal splits (same for both top and bottom)
    int leftSectionWidth = MAX_LABEL_WIDTH;  // Static distance from left
    int rightSectionWidth = totalWidth - leftSectionWidth - m_topHorizontalSplitter->handleWidth();
    
    // Ensure right section doesn't go negative
    if (rightSectionWidth < 100) {
        rightSectionWidth = 100;
        leftSectionWidth = totalWidth - rightSectionWidth - m_topHorizontalSplitter->handleWidth();
    }
    
    QList<int> horizontalSizes;
    horizontalSizes << leftSectionWidth << rightSectionWidth;
    
    // Set both horizontal splitters to same width split
    m_topHorizontalSplitter->setSizes(horizontalSizes);
    m_bottomHorizontalSplitter->setSizes(horizontalSizes);
}


void MatrixWidget::connectScrollSync()
{
    // Sync horizontal scrolling: button grid → target header
    connect(m_buttonGridScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        m_targetHeaderScrollArea->horizontalScrollBar()->setValue(value);
    });
    
    // Sync vertical scrolling: button grid → source sidebar
    connect(m_buttonGridScrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        m_sourcesSidebarScrollArea->verticalScrollBar()->setValue(value);
    });
}

void MatrixWidget::onTopSplitterMoved(int pos, int index)
{
    // User manually moved a handle - remember this
    m_userAdjustedHandles = true;
    
    // When top splitter moves (corner vs target headers), sync bottom splitter
    // to maintain same width for corner and source sidebar
    if (index == 1) {  // Handle between corner and target headers
        QList<int> topSizes = m_topHorizontalSplitter->sizes();
        QList<int> bottomSizes = m_bottomHorizontalSplitter->sizes();
        
        // Update bottom splitter to match top splitter's left width
        if (topSizes.size() >= 2 && bottomSizes.size() >= 2) {
            bottomSizes[0] = topSizes[0];  // Match corner width to sidebar width
            
            // Temporarily block signals to avoid infinite recursion
            m_bottomHorizontalSplitter->blockSignals(true);
            m_bottomHorizontalSplitter->setSizes(bottomSizes);
            m_bottomHorizontalSplitter->blockSignals(false);
        }
    }
}

void MatrixWidget::onBottomSplitterMoved(int pos, int index)
{
    // User manually moved a handle - remember this
    m_userAdjustedHandles = true;
    
    // When bottom splitter moves (sidebar vs button grid), sync top splitter
    // to maintain same width for corner and source sidebar
    if (index == 1) {  // Handle between sidebar and button grid
        QList<int> topSizes = m_topHorizontalSplitter->sizes();
        QList<int> bottomSizes = m_bottomHorizontalSplitter->sizes();
        
        // Update top splitter to match bottom splitter's left width
        if (topSizes.size() >= 2 && bottomSizes.size() >= 2) {
            topSizes[0] = bottomSizes[0];  // Match corner width to sidebar width
            
            // Temporarily block signals to avoid infinite recursion
            m_topHorizontalSplitter->blockSignals(true);
            m_topHorizontalSplitter->setSizes(topSizes);
            m_topHorizontalSplitter->blockSignals(false);
        }
    }
}

void MatrixWidget::onVerticalSplitterMoved(int pos, int index)
{
    // User manually moved the vertical handle - remember this
    m_userAdjustedHandles = true;
    
    // The vertical splitter controls the height of the target label area
    // When it moves, the labels automatically resize due to their Expanding size policy
    // and the updateDisplayText() in resizeEvent() will recalculate truncation
    
    // No additional synchronization needed - labels are in the top section
    // and will naturally resize to fill the available height
}

void MatrixWidget::clearLayoutAndWidgets(QLayout *layout)
{
    if (!layout) return;
    
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void MatrixWidget::setMatrixPath(const QString &path)
{
    m_matrixPath = path;
}

void MatrixWidget::setMatrixInfo(const QString &identifier, const QString &description, 
                                  int type, int targetCount, int sourceCount)
{
    m_identifier = identifier;
    m_description = description;
    m_matrixType = type;
    m_targetCount = targetCount;
    m_sourceCount = sourceCount;
    
    // Update header - show only type and dimensions
    QString typeStr;
    switch (type) {
        case 0: typeStr = "1:N"; break;
        case 1: typeStr = "1:1"; break;
        case 2: typeStr = "N:N"; break;
        default: typeStr = QString::number(type); break;
    }
    
    m_headerLabel->setText(QString("<b>%1</b>  •  %2×%3")
                           .arg(typeStr).arg(sourceCount).arg(targetCount));
}

void MatrixWidget::setTargetLabel(int targetNumber, const QString &label)
{
    m_targetLabels[targetNumber] = label;
    
    // Track this target number if not already present
    if (!m_targetNumbers.contains(targetNumber)) {
        m_targetNumbers.append(targetNumber);
        std::sort(m_targetNumbers.begin(), m_targetNumbers.end());  // Keep sorted
    }
}

void MatrixWidget::setSourceLabel(int sourceNumber, const QString &label)
{
    m_sourceLabels[sourceNumber] = label;
    
    // Track this source number if not already present
    if (!m_sourceNumbers.contains(sourceNumber)) {
        m_sourceNumbers.append(sourceNumber);
        std::sort(m_sourceNumbers.begin(), m_sourceNumbers.end());  // Keep sorted
    }
}

void MatrixWidget::setConnection(int targetNumber, int sourceNumber, bool connected, int disposition)
{
    QPair<int, int> key(targetNumber, sourceNumber);
    Q_ASSERT(targetNumber >= 0 && sourceNumber >= 0);
    
    if (connected) {
        ConnectionState state;
        state.connected = true;
        state.disposition = disposition;
        m_connections[key] = state;
    } else {
        m_connections.remove(key);
    }
    
    updateConnectionButton(targetNumber, sourceNumber);
}

void MatrixWidget::clearConnections()
{
    m_connections.clear();
}

void MatrixWidget::rebuild()
{
    buildGrid();
}

void MatrixWidget::buildGrid()
{
    // If grid already exists, just refresh button states (avoid expensive rebuild)
    if (!m_buttons.isEmpty()) {
        qDebug() << "Grid already built, refreshing button states only";
        for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
            updateConnectionButton(it.key().first, it.key().second);
        }
        return;
    }
    
    // Clear existing widgets from all sections
    clearLayoutAndWidgets(m_targetHeaderLayout);
    clearLayoutAndWidgets(m_sourcesSidebarLayout);
    clearLayoutAndWidgets(m_buttonGridLayout);
    m_buttons.clear();
    
    // Check if we have data
    if (m_targetNumbers.isEmpty() || m_sourceNumbers.isEmpty()) {
        auto *emptyLabel = new QLabel("No matrix data", m_buttonGridContainer);
        emptyLabel->setStyleSheet("color: #888;");
        m_buttonGridLayout->addWidget(emptyLabel, 0, 0);
        return;
    }
    
    // Build target header (rotated labels)
    for (int col = 0; col < m_targetNumbers.size(); col++) {
        int tgtNum = m_targetNumbers[col];
        QString label = m_targetLabels.value(tgtNum, QString("T%1").arg(tgtNum));
        auto *rotatedLabel = new RotatedLabel(label, BUTTON_SIZE, 
                                              m_targetHeaderContainer);
        m_targetHeaderLayout->addWidget(rotatedLabel);
    }
    
    // Build source sidebar (using SourceLabel)
    for (int row = 0; row < m_sourceNumbers.size(); row++) {
        int srcNum = m_sourceNumbers[row];
        QString label = m_sourceLabels.value(srcNum, QString("S%1").arg(srcNum));
        
        auto *srcLabel = new SourceLabel(label, BUTTON_SIZE, m_sourcesSidebarContainer);
        m_sourcesSidebarLayout->addWidget(srcLabel);
    }
    
    // Build button grid (NO labels, just buttons)
    for (int row = 0; row < m_sourceNumbers.size(); row++) {
        int srcNum = m_sourceNumbers[row];
        
        for (int col = 0; col < m_targetNumbers.size(); col++) {
            int tgtNum = m_targetNumbers[col];
            
            auto *btn = new QPushButton(m_buttonGridContainer);
            btn->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
            btn->setEnabled(m_crosspointsEnabled);
            btn->installEventFilter(this);
            
            QPair<int, int> key(tgtNum, srcNum);
            m_buttons[key] = btn;
            
            // Connect button click to emit signal
            connect(btn, &QPushButton::clicked, this, [this, tgtNum, srcNum]() {
                emit crosspointClicked(m_matrixPath, tgtNum, srcNum);
            });
            
            updateConnectionButton(tgtNum, srcNum);
            
            m_buttonGridLayout->addWidget(btn, row, col);  // Note: no +1 offset
        }
    }
    
    // Resize containers to fit content
    
    // Set container width to match button grid columns exactly
    int targetHeaderWidth = m_targetNumbers.size() * BUTTON_SIZE;
    // Set container height to match button grid rows exactly
    int sourcesSidebarHeight = m_sourceNumbers.size() * BUTTON_SIZE;
    m_sourcesSidebarContainer->setFixedHeight(sourcesSidebarHeight);
    m_targetHeaderContainer->setFixedWidth(targetHeaderWidth);
    m_targetHeaderContainer->adjustSize();
    m_sourcesSidebarContainer->adjustSize();
    m_buttonGridContainer->adjustSize();
}

void MatrixWidget::updateConnectionButton(int targetNumber, int sourceNumber)
{
    QPair<int, int> key(targetNumber, sourceNumber);
    Q_ASSERT(targetNumber >= 0 && sourceNumber >= 0);
    
    if (!m_buttons.contains(key)) {
        qDebug() << "MatrixWidget::updateConnectionButton - Button not found for Target" << targetNumber << "Source" << sourceNumber << "- Grid has" << m_buttons.size() << "buttons";
        return;
    }
    
    QPushButton *btn = m_buttons[key];
    
    if (!m_connections.contains(key)) {
        btn->setStyleSheet(
            "QPushButton { "
            "  background-color: #f5f5f5; "
            "  border: 1px solid #ccc; "
            "}"
        );
        btn->setText("");
        return;
    }
    
    const ConnectionState &state = m_connections[key];
    
    switch (state.disposition) {
        case 0:
            btn->setStyleSheet(
                "QPushButton { "
                "  background-color: #4CAF50; "
                "  border: 1px solid #45a049; "
                "  font-weight: bold; "
                "  color: white; "
                "  font-size: 8pt; "
                "}"
            );
            btn->setText("✓");
            btn->setToolTip("Connected (Tally)");
            break;
        
        case 1:
            btn->setStyleSheet(
                "QPushButton { "
                "  background-color: #FF9800; "
                "  border: 1px solid #F57C00; "
                "  font-weight: bold; "
                "  color: white; "
                "  font-size: 8pt; "
                "}"
            );
            btn->setText("~");
            btn->setToolTip("Modified - Change pending confirmation");
            break;
        
        case 2:
            btn->setStyleSheet(
                "QPushButton { "
                "  background-color: #FFC107; "
                "  border: 1px solid #FFA000; "
                "  font-weight: bold; "
                "  color: #333; "
                "  font-size: 8pt; "
                "}"
            );
            btn->setText("⏳");
            btn->setToolTip("Pending - Waiting for device");
            break;
        
        case 3:
            btn->setStyleSheet(
                "QPushButton { "
                "  background-color: #4CAF50; "
                "  border: 2px solid #F44336; "
                "  font-weight: bold; "
                "  color: white; "
                "  font-size: 8pt; "
                "}"
            );
            btn->setText("🔒");
            btn->setToolTip("Locked - Cannot be changed");
            btn->setEnabled(false);
            break;
        
        default:
            btn->setStyleSheet(
                "QPushButton { "
                "  background-color: #4CAF50; "
                "  border: 1px solid #45a049; "
                "  font-weight: bold; "
                "  color: white; "
                "  font-size: 8pt; "
                "}"
            );
            btn->setText("✓");
            btn->setToolTip(QString("Connected (Unknown disposition: %1)").arg(state.disposition));
            break;
    }
}

bool MatrixWidget::eventFilter(QObject *watched, QEvent *event)
{
    QPushButton *btn = qobject_cast<QPushButton*>(watched);
    if (!btn) {
        return QWidget::eventFilter(watched, event);
    }
    
    // Find which target/source this button belongs to
    QPair<int, int> key(-1, -1);
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
        if (it.value() == btn) {
            key = it.key();
            break;
        }
    }
    
    if (key.first == -1) {
        return QWidget::eventFilter(watched, event);
    }
    
    if (event->type() == QEvent::Enter) {
        // Mouse entered button - highlight row and column
        updateHoverHighlight(key.first, key.second);
    } else if (event->type() == QEvent::Leave) {
        // Mouse left button - clear highlight
        updateHoverHighlight(-1, -1);
    }
    
    return QWidget::eventFilter(watched, event);
}

void MatrixWidget::updateHoverHighlight(int targetNumber, int sourceNumber)
{
    // Clear previous highlight
    if (m_hoverTargetNumber != -1 && m_hoverSourceNumber != -1) {
        // Find the column index of the previously hovered target
        int prevColIdx = m_targetNumbers.indexOf(m_hoverTargetNumber);
        if (prevColIdx != -1) {
            // Update all buttons to the LEFT of and including the previous column (same row)
            for (int col = 0; col <= prevColIdx; col++) {
                int tgtNum = m_targetNumbers[col];
                QPair<int, int> key(tgtNum, m_hoverSourceNumber);
                if (m_buttons.contains(key)) {
                    updateConnectionButton(tgtNum, m_hoverSourceNumber);
                }
            }
        }
        
        // Find the row index of the previously hovered source
        int prevRowIdx = m_sourceNumbers.indexOf(m_hoverSourceNumber);
        if (prevRowIdx != -1) {
            // Update all buttons ABOVE and including the previous row (same column)
            for (int row = 0; row <= prevRowIdx; row++) {
                int srcNum = m_sourceNumbers[row];
                QPair<int, int> key(m_hoverTargetNumber, srcNum);
                if (m_buttons.contains(key)) {
                    updateConnectionButton(m_hoverTargetNumber, srcNum);
                }
            }
        }
    }
    
    // Set new hover position
    m_hoverTargetNumber = targetNumber;
    m_hoverSourceNumber = sourceNumber;
    
    // Apply new highlight
    if (m_hoverTargetNumber != -1 && m_hoverSourceNumber != -1) {
        // Find the column index of the current hovered target
        int colIdx = m_targetNumbers.indexOf(m_hoverTargetNumber);
        if (colIdx != -1) {
            // Highlight all buttons to the LEFT of and including this column (same row)
            for (int col = 0; col <= colIdx; col++) {
                int tgtNum = m_targetNumbers[col];
                QPair<int, int> key(tgtNum, m_hoverSourceNumber);
                QPushButton *btn = m_buttons.value(key, nullptr);
                if (btn) {
                    bool connected = m_connections.contains(key);
                    if (connected) {
                        btn->setStyleSheet(
                            "QPushButton { "
                            "  background-color: #66BB6A; "  // Lighter green for hover
                            "  border: 2px solid #2E7D32; "
                            "  font-weight: bold; "
                            "  color: white; "
                            "  font-size: 8pt; "
                            "}"
                        );
                    } else {
                        btn->setStyleSheet(
                            "QPushButton { "
                            "  background-color: #e0e0e0; "  // Darker gray for hover
                            "  border: 2px solid #999; "
                            "}"
                        );
                    }
                }
            }
        }
        
        // Find the row index of the current hovered source
        int rowIdx = m_sourceNumbers.indexOf(m_hoverSourceNumber);
        if (rowIdx != -1) {
            // Highlight all buttons ABOVE and including this row (same column)
            for (int row = 0; row <= rowIdx; row++) {
                int srcNum = m_sourceNumbers[row];
                QPair<int, int> key(m_hoverTargetNumber, srcNum);
                QPushButton *btn = m_buttons.value(key, nullptr);
                if (btn) {
                    bool connected = m_connections.contains(key);
                    if (connected) {
                        btn->setStyleSheet(
                            "QPushButton { "
                            "  background-color: #66BB6A; "  // Lighter green for hover
                            "  border: 2px solid #2E7D32; "
                            "  font-weight: bold; "
                            "  color: white; "
                            "  font-size: 8pt; "
                            "}"
                        );
                    } else {
                        btn->setStyleSheet(
                            "QPushButton { "
                            "  background-color: #e0e0e0; "  // Darker gray for hover
                            "  border: 2px solid #999; "
                            "}"
                        );
                    }
                }
            }
        }
    }
}

bool MatrixWidget::isConnected(int targetNumber, int sourceNumber) const
{
    QPair<int, int> key(targetNumber, sourceNumber);
    Q_ASSERT(targetNumber >= 0 && sourceNumber >= 0);
    return m_connections.contains(key) && m_connections[key].connected;
}

QString MatrixWidget::getTargetLabel(int targetNumber) const
{
    return m_targetLabels.value(targetNumber, QString("Target %1").arg(targetNumber));
}

QString MatrixWidget::getSourceLabel(int sourceNumber) const
{
    return m_sourceLabels.value(sourceNumber, QString("Source %1").arg(sourceNumber));
}

void MatrixWidget::setCrosspointsEnabled(bool enabled)
{
    m_crosspointsEnabled = enabled;
    
    // Update corner button icon based on state
    QIcon icon;
    if (enabled) {
        icon = QIcon(":/lock-open.png");
        qDebug() << "Setting lock-open icon, isNull:" << icon.isNull() << "sizes:" << icon.availableSizes();
    } else {
        icon = QIcon(":/lock-closed.png");
        qDebug() << "Setting lock-closed icon, isNull:" << icon.isNull() << "sizes:" << icon.availableSizes();
    }
    m_cornerWidget->setIcon(icon);
    m_cornerWidget->setIconSize(QSize(32, 32));
    
    // Update background color based on state
    if (enabled) {
        // Pure red with 15% opacity to indicate editing is enabled
        // Use QPalette to set background color reliably
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(255, 0, 0, 38));  // Pure red with 15% alpha (0.15 * 255 = 38)
        setAutoFillBackground(true);
        setPalette(pal);
    } else {
        // Normal background
        setAutoFillBackground(false);
        setPalette(QPalette());
    }
    
    // Enable/disable all buttons
    for (QPushButton *btn : m_buttons.values()) {
        btn->setEnabled(enabled);
        if (enabled) {
            btn->setCursor(Qt::PointingHandCursor);
        } else {
            btn->setCursor(Qt::ArrowCursor);
        }
    }
}

