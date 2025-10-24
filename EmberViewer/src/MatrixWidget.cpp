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
#include <algorithm>

// RotatedLabel implementation
RotatedLabel::RotatedLabel(const QString &text, int buttonWidth, int labelHeight, int maxTextLength, QWidget *parent)
    : QWidget(parent), m_fullText(text), m_displayText(text)
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    setFont(font);
    
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text);
    
    // If text is too long, truncate with ellipsis
    if (textWidth > maxTextLength) {
        m_displayText = fm.elidedText(text, Qt::ElideRight, maxTextLength);
        setToolTip(text);
    }
    
    // Fixed size: width matches button, height provides space for rotated text
    setFixedSize(buttonWidth, labelHeight);
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
    
    // Draw text centered
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(m_displayText);
    int textHeight = fm.height();
    painter.drawText(-textWidth / 2, textHeight / 4, m_displayText);
    
    painter.restore();
}

// MatrixWidget implementation
MatrixWidget::MatrixWidget(QWidget *parent)
    : QWidget(parent)
    , m_matrixType(2)  // Default to NToN
    , m_targetCount(0)
    , m_sourceCount(0)
    , m_headerLabel(nullptr)
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
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(10, 10, 10, 10);
    outerLayout->setSpacing(5);
    
    // Header label (above the frozen pane grid)
    m_headerLabel = new QLabel("Matrix", this);
    m_headerLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    outerLayout->addWidget(m_headerLabel);
    
    // Main frozen pane grid: 2x2 layout
    auto *frozenPaneWidget = new QWidget(this);
    auto *frozenPaneLayout = new QGridLayout(frozenPaneWidget);
    frozenPaneLayout->setSpacing(0);
    frozenPaneLayout->setContentsMargins(0, 0, 0, 0);
    
    // [0,0] Corner widget - empty spacer
    m_cornerWidget = new QWidget(this);
    m_cornerWidget->setFixedSize(MAX_LABEL_WIDTH, LABEL_HEIGHT + 2);
    frozenPaneLayout->addWidget(m_cornerWidget, 0, 0);
    
    // [0,1] Target header (horizontal scroll only)
    m_targetHeaderScrollArea = new QScrollArea(this);
    m_targetHeaderScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_targetHeaderScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Synced from button grid
    m_targetHeaderScrollArea->setWidgetResizable(false);
    m_targetHeaderScrollArea->setFrameShape(QFrame::NoFrame);
    m_targetHeaderScrollArea->setFixedHeight(LABEL_HEIGHT + 2);
    m_targetHeaderContainer = new QWidget();
    m_targetHeaderLayout = new QHBoxLayout(m_targetHeaderContainer);
    m_targetHeaderLayout->setSpacing(GRID_SPACING);
    m_targetHeaderLayout->setContentsMargins(0, 0, 0, 0);
    m_targetHeaderScrollArea->setWidget(m_targetHeaderContainer);
    frozenPaneLayout->addWidget(m_targetHeaderScrollArea, 0, 1);
    
    // [1,0] Source sidebar (vertical scroll only)
    m_sourcesSidebarScrollArea = new QScrollArea(this);
    m_sourcesSidebarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sourcesSidebarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Synced from button grid
    m_sourcesSidebarScrollArea->setWidgetResizable(false);
    m_sourcesSidebarScrollArea->setFrameShape(QFrame::NoFrame);
    m_sourcesSidebarScrollArea->setFixedWidth(MAX_LABEL_WIDTH + 2);
    m_sourcesSidebarScrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");
    m_sourcesSidebarContainer = new QWidget();
    // No background color set - uses default/transparent
    m_sourcesSidebarLayout = new QVBoxLayout(m_sourcesSidebarContainer);
    m_sourcesSidebarLayout->setSpacing(GRID_SPACING);
    m_sourcesSidebarLayout->setContentsMargins(0, 0, 0, 0);
    m_sourcesSidebarScrollArea->setWidget(m_sourcesSidebarContainer);
    frozenPaneLayout->addWidget(m_sourcesSidebarScrollArea, 1, 0);
    
    // [1,1] Button grid (both scrollbars)
    m_buttonGridScrollArea = new QScrollArea(this);
    m_buttonGridScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_buttonGridScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_buttonGridScrollArea->setWidgetResizable(false);
    m_buttonGridContainer = new QWidget();
    m_buttonGridLayout = new QGridLayout(m_buttonGridContainer);
    m_buttonGridLayout->setSpacing(GRID_SPACING);
    m_buttonGridLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonGridScrollArea->setWidget(m_buttonGridContainer);
    frozenPaneLayout->addWidget(m_buttonGridScrollArea, 1, 1);
    
    // Add frozen pane grid to outer layout
    outerLayout->addWidget(frozenPaneWidget);
    
    // Connect scroll synchronization
    connectScrollSync();
}

MatrixWidget::~MatrixWidget()
{
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
    
    // Update header
    QString header = identifier;
    if (!description.isEmpty() && description != identifier) {
        header = description;
    }
    
    QString typeStr;
    switch (type) {
        case 0: typeStr = "1:N"; break;
        case 1: typeStr = "1:1"; break;
        case 2: typeStr = "N:N"; break;
        default: typeStr = QString::number(type); break;
    }
    
    m_headerLabel->setText(QString("<b>%1</b> (%2) - %3×%4")
                           .arg(header).arg(typeStr).arg(sourceCount).arg(targetCount));
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
        auto *rotatedLabel = new RotatedLabel(label, BUTTON_SIZE, LABEL_HEIGHT, 120, 
                                              m_targetHeaderContainer);
        m_targetHeaderLayout->addWidget(rotatedLabel);
    }
    
    // Build source sidebar (regular labels)
    for (int row = 0; row < m_sourceNumbers.size(); row++) {
        int srcNum = m_sourceNumbers[row];
        QString label = m_sourceLabels.value(srcNum, QString("S%1").arg(srcNum));
        
        auto *srcLabel = new QLabel(label, m_sourcesSidebarContainer);
        QFont labelFont = srcLabel->font();
        labelFont.setBold(true);
        srcLabel->setFont(labelFont);
        srcLabel->setStyleSheet("padding: 2px; background-color: transparent;");
        srcLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        srcLabel->setFixedHeight(BUTTON_SIZE);
        srcLabel->setFixedWidth(MAX_LABEL_WIDTH);
        srcLabel->setAutoFillBackground(false);
        
        // Truncate if too long
        QFontMetrics fm(labelFont);
        QString elidedText = fm.elidedText(label, Qt::ElideRight, MAX_LABEL_WIDTH - 4);
        srcLabel->setText(elidedText);
        if (elidedText != label) {
            srcLabel->setToolTip(label);
        }
        
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

