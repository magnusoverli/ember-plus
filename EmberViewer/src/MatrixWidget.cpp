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
    , m_grid(nullptr)
    , m_gridWidget(nullptr)
    , m_headerLabel(nullptr)
    , m_hoverTargetNumber(-1)
    , m_hoverSourceNumber(-1)
    , m_crosspointsEnabled(false)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Header label
    m_headerLabel = new QLabel("Matrix", this);
    m_headerLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    mainLayout->addWidget(m_headerLabel);
    
    // Grid container (will be populated when matrix data is set)
    m_gridWidget = new QWidget(this);
    m_grid = new QGridLayout(m_gridWidget);
    m_grid->setSpacing(GRID_SPACING);
    m_grid->setContentsMargins(0, 0, 0, 0);
    
    mainLayout->addWidget(m_gridWidget);
    mainLayout->addStretch();
}

MatrixWidget::~MatrixWidget()
{
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

void MatrixWidget::setConnection(int targetNumber, int sourceNumber, bool connected)
{
    QPair<int, int> key(targetNumber, sourceNumber);
    
    if (connected) {
        m_connections.insert(key);
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
    // Clear existing grid
    QLayoutItem *item;
    while ((item = m_grid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_buttons.clear();
    
    // Use actual target/source numbers instead of counts
    if (m_targetNumbers.isEmpty() || m_sourceNumbers.isEmpty()) {
        auto *emptyLabel = new QLabel("No matrix data", this);
        emptyLabel->setStyleSheet("color: #888;");
        m_grid->addWidget(emptyLabel, 0, 0);
        return;
    }
    
    // Corner cell
    auto *corner = new QLabel("SRC \\ TGT", this);
    corner->setStyleSheet("font-weight: bold; color: #888; font-size: 8pt; padding: 2px;");
    corner->setAlignment(Qt::AlignCenter);
    m_grid->addWidget(corner, 0, 0);
    
    // Target labels (rotated, on top) - use actual target numbers
    for (int col = 0; col < m_targetNumbers.size(); col++) {
        int tgtNum = m_targetNumbers[col];
        QString label = m_targetLabels.value(tgtNum, QString("T%1").arg(tgtNum));
        auto *rotatedLabel = new RotatedLabel(label, BUTTON_SIZE, LABEL_HEIGHT, 120, this);
        m_grid->addWidget(rotatedLabel, 0, col + 1, Qt::AlignHCenter | Qt::AlignTop);
    }
    
    // Source labels (left) and buttons - use actual source numbers
    for (int row = 0; row < m_sourceNumbers.size(); row++) {
        int srcNum = m_sourceNumbers[row];
        
        // Source label
        QString label = m_sourceLabels.value(srcNum, QString("S%1").arg(srcNum));
        auto *srcLabel = new QLabel(label, this);
        QFont labelFont = srcLabel->font();
        labelFont.setBold(true);
        srcLabel->setFont(labelFont);
        srcLabel->setStyleSheet("padding: 2px;");
        srcLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        srcLabel->setFixedHeight(BUTTON_SIZE);
        
        // Truncate if too long
        QFontMetrics fm(labelFont);
        QString elidedText = fm.elidedText(label, Qt::ElideRight, MAX_LABEL_WIDTH);
        srcLabel->setText(elidedText);
        if (elidedText != label) {
            srcLabel->setToolTip(label);
        }
        
        m_grid->addWidget(srcLabel, row + 1, 0);
        
        // Crosspoint buttons - use actual target numbers
        for (int col = 0; col < m_targetNumbers.size(); col++) {
            int tgtNum = m_targetNumbers[col];
            
            auto *btn = new QPushButton(this);
            btn->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
            btn->setEnabled(m_crosspointsEnabled);  // Enable based on crosspoints state
            btn->installEventFilter(this);  // Install event filter for hover detection
            
            QPair<int, int> key(tgtNum, srcNum);
            m_buttons[key] = btn;
            
            // Connect button click to emit signal
            connect(btn, &QPushButton::clicked, this, [this, tgtNum, srcNum]() {
                emit crosspointClicked(m_matrixPath, tgtNum, srcNum);
            });
            
            // Set initial state
            bool connected = m_connections.contains(key);
            if (connected) {
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
            } else {
                btn->setStyleSheet(
                    "QPushButton { "
                    "  background-color: #f5f5f5; "
                    "  border: 1px solid #ccc; "
                    "}"
                );
            }
            
            m_grid->addWidget(btn, row + 1, col + 1);
        }
    }
}

void MatrixWidget::updateConnectionButton(int targetNumber, int sourceNumber)
{
    QPair<int, int> key(targetNumber, sourceNumber);
    
    if (!m_buttons.contains(key)) {
        qDebug() << "MatrixWidget::updateConnectionButton - Button not found for Target" << targetNumber << "Source" << sourceNumber << "- Grid has" << m_buttons.size() << "buttons";
        return;
    }
    
    QPushButton *btn = m_buttons[key];
    bool connected = m_connections.contains(key);
    
    if (connected) {
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
    } else {
        btn->setStyleSheet(
            "QPushButton { "
            "  background-color: #f5f5f5; "
            "  border: 1px solid #ccc; "
            "}"
        );
        btn->setText("");
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
    return m_connections.contains(QPair<int, int>(targetNumber, sourceNumber));
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

