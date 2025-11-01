







#include "MatrixWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFontMetrics>
#include <utility>
#include <QDebug>
#include <QEvent>
#include <QPalette>
#include <QColor>
#include <QScrollBar>
#include <QSplitter>
#include <QIcon>
#include <algorithm>


const int MatrixWidget::BUTTON_SIZE;
const int MatrixWidget::GRID_SPACING;
const int MatrixWidget::LABEL_HEIGHT;
const int MatrixWidget::MAX_LABEL_WIDTH;



RotatedLabel::RotatedLabel(const QString &text, int buttonWidth, QWidget *parent)
    : QWidget(parent), m_fullText(text), m_displayText(text)
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    setFont(font);
    
    
    setFixedWidth(buttonWidth);
    setMinimumHeight(50);  
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    
    
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
    int availableHeight = height();  
    
    
    if (textWidth > availableHeight) {
        m_displayText = fm.elidedText(m_fullText, Qt::ElideRight, availableHeight);
        setToolTip(m_fullText);  
    } else {
        m_displayText = m_fullText;
        setToolTip("");  
    }
    
    update();  
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
    
    
    painter.save();
    painter.translate(width() / 2, height() / 2);
    painter.rotate(-90);
    
    
    QRect boundingRect(-height() / 2, -width() / 2, height(), width());
    painter.drawText(boundingRect, Qt::AlignLeft | Qt::AlignVCenter, m_displayText);
    
    painter.restore();
}




SourceLabel::SourceLabel(const QString &text, int buttonHeight, QWidget *parent)
    : QWidget(parent), m_fullText(text), m_displayText(text)
{
    QFont font;
    font.setBold(true);
    font.setPointSize(9);
    setFont(font);
    
    
    setFixedHeight(buttonHeight);
    setMinimumWidth(50);  
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    
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
    int availableWidth = width() - 4;  
    
    
    if (textWidth > availableWidth) {
        m_displayText = fm.elidedText(m_fullText, Qt::ElideRight, availableWidth);
        setToolTip(m_fullText);  
    } else {
        m_displayText = m_fullText;
        setToolTip("");  
    }
    
    update();  
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
    
    
    QRect boundingRect(0, 0, width(), height());
    painter.drawText(boundingRect, Qt::AlignRight | Qt::AlignVCenter, m_displayText);
}

MatrixWidget::MatrixWidget(QWidget *parent)
    : QWidget(parent)
    , m_matrixType(2)  
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
    
    
    m_headerLabel = new QLabel("Matrix", this);
    m_headerLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    outerLayout->addWidget(m_headerLabel);
    
    
    m_outerVerticalSplitter = new QSplitter(Qt::Vertical, this);
    m_outerVerticalSplitter->setChildrenCollapsible(false);
    m_outerVerticalSplitter->setHandleWidth(6);
    m_outerVerticalSplitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    
    
    m_topHorizontalSplitter = new QSplitter(Qt::Horizontal);
    m_topHorizontalSplitter->setChildrenCollapsible(false);
    m_topHorizontalSplitter->setHandleWidth(6);
    m_topHorizontalSplitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    
    
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
    m_topHorizontalSplitter->setStretchFactor(0, 0);  
    m_topHorizontalSplitter->setStretchFactor(1, 1);  
    
    
    m_bottomHorizontalSplitter = new QSplitter(Qt::Horizontal);
    m_bottomHorizontalSplitter->setChildrenCollapsible(false);
    m_bottomHorizontalSplitter->setHandleWidth(6);
    m_bottomHorizontalSplitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    
    
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
    m_bottomHorizontalSplitter->setStretchFactor(0, 0);  
    m_bottomHorizontalSplitter->setStretchFactor(1, 1);  
    
    
    m_outerVerticalSplitter->addWidget(m_topHorizontalSplitter);
    m_outerVerticalSplitter->addWidget(m_bottomHorizontalSplitter);
    
    
    m_outerVerticalSplitter->setMinimumHeight(200);
    m_outerVerticalSplitter->setStretchFactor(0, 0);  
    m_outerVerticalSplitter->setStretchFactor(1, 1);  
    
    
    
    
    
    outerLayout->addWidget(m_outerVerticalSplitter);
    
    
    m_outerVerticalSplitter->setStyleSheet(m_outerVerticalSplitter->styleSheet() + " QSplitter { background: transparent; }");
    m_topHorizontalSplitter->setStyleSheet(m_topHorizontalSplitter->styleSheet() + " QSplitter { background: transparent; }");
    m_bottomHorizontalSplitter->setStyleSheet(m_bottomHorizontalSplitter->styleSheet() + " QSplitter { background: transparent; }");
    m_targetHeaderScrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");
    m_targetHeaderContainer->setStyleSheet("QWidget { background-color: transparent; }");
    m_buttonGridScrollArea->setStyleSheet("QScrollArea { background-color: transparent; }");
    m_buttonGridContainer->setStyleSheet("QWidget { background-color: transparent; }");
    m_sourcesSidebarContainer->setStyleSheet("QWidget { background-color: transparent; }");
    
    
    connect(m_topHorizontalSplitter, &QSplitter::splitterMoved,
            this, &MatrixWidget::onTopSplitterMoved);
    connect(m_bottomHorizontalSplitter, &QSplitter::splitterMoved,
            this, &MatrixWidget::onBottomSplitterMoved);
    connect(m_outerVerticalSplitter, &QSplitter::splitterMoved,
            this, &MatrixWidget::onVerticalSplitterMoved);
    
    
    connectScrollSync();
}

MatrixWidget::~MatrixWidget()
{
}

void MatrixWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    
    if (!m_userAdjustedHandles) {
        enforceStaticHandlePositions();
    }
}

void MatrixWidget::enforceStaticHandlePositions()
{
    
    int totalHeight = m_outerVerticalSplitter->height();
    int totalWidth = m_outerVerticalSplitter->width();
    
    
    const int minLabelHeight = 50;
    const int minTopMargin = 10;  
    
    
    int topSectionHeight = LABEL_HEIGHT + 2;  
    int bottomSectionHeight = totalHeight - topSectionHeight - m_outerVerticalSplitter->handleWidth();
    
    
    if (bottomSectionHeight < 100) {
        bottomSectionHeight = 100;
        topSectionHeight = totalHeight - bottomSectionHeight - m_outerVerticalSplitter->handleWidth();
    }
    
    
    if (topSectionHeight < minLabelHeight + minTopMargin) {
        topSectionHeight = minLabelHeight + minTopMargin;
    }
    
    QList<int> verticalSizes;
    verticalSizes << topSectionHeight << bottomSectionHeight;
    m_outerVerticalSplitter->setSizes(verticalSizes);
    
    
    int leftSectionWidth = MAX_LABEL_WIDTH;  
    int rightSectionWidth = totalWidth - leftSectionWidth - m_topHorizontalSplitter->handleWidth();
    
    
    if (rightSectionWidth < 100) {
        rightSectionWidth = 100;
        leftSectionWidth = totalWidth - rightSectionWidth - m_topHorizontalSplitter->handleWidth();
    }
    
    QList<int> horizontalSizes;
    horizontalSizes << leftSectionWidth << rightSectionWidth;
    
    
    m_topHorizontalSplitter->setSizes(horizontalSizes);
    m_bottomHorizontalSplitter->setSizes(horizontalSizes);
}


void MatrixWidget::connectScrollSync()
{
    
    connect(m_buttonGridScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        m_targetHeaderScrollArea->horizontalScrollBar()->setValue(value);
    });
    
    
    connect(m_buttonGridScrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        m_sourcesSidebarScrollArea->verticalScrollBar()->setValue(value);
    });
}

void MatrixWidget::onTopSplitterMoved(int pos, int index)
{
    
    m_userAdjustedHandles = true;
    
    
    
    if (index == 1) {  
        QList<int> topSizes = m_topHorizontalSplitter->sizes();
        QList<int> bottomSizes = m_bottomHorizontalSplitter->sizes();
        
        
        if (topSizes.size() >= 2 && bottomSizes.size() >= 2) {
            bottomSizes[0] = topSizes[0];  
            
            
            m_bottomHorizontalSplitter->blockSignals(true);
            m_bottomHorizontalSplitter->setSizes(bottomSizes);
            m_bottomHorizontalSplitter->blockSignals(false);
        }
    }
}

void MatrixWidget::onBottomSplitterMoved(int pos, int index)
{
    
    m_userAdjustedHandles = true;
    
    
    
    if (index == 1) {  
        QList<int> topSizes = m_topHorizontalSplitter->sizes();
        QList<int> bottomSizes = m_bottomHorizontalSplitter->sizes();
        
        
        if (topSizes.size() >= 2 && bottomSizes.size() >= 2) {
            topSizes[0] = bottomSizes[0];  
            
            
            m_topHorizontalSplitter->blockSignals(true);
            m_topHorizontalSplitter->setSizes(topSizes);
            m_topHorizontalSplitter->blockSignals(false);
        }
    }
}

void MatrixWidget::onVerticalSplitterMoved(int pos, int index)
{
    
    m_userAdjustedHandles = true;
    
    
    
    
    
    
    
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
    
    
    if (!m_targetNumbers.contains(targetNumber)) {
        m_targetNumbers.append(targetNumber);
        std::sort(m_targetNumbers.begin(), m_targetNumbers.end());  
    }
}

void MatrixWidget::setSourceLabel(int sourceNumber, const QString &label)
{
    m_sourceLabels[sourceNumber] = label;
    
    
    if (!m_sourceNumbers.contains(sourceNumber)) {
        m_sourceNumbers.append(sourceNumber);
        std::sort(m_sourceNumbers.begin(), m_sourceNumbers.end());  
    }
}

void MatrixWidget::setConnection(int targetNumber, int sourceNumber, bool connected, int disposition)
{
    std::pair<int, int> key(targetNumber, sourceNumber);
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

void MatrixWidget::clearTargetConnections(int targetNumber)
{
    
    QList<std::pair<int, int>> keysToRemove;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.key().first == targetNumber) {
            keysToRemove.append(it.key());
        }
    }
    
    for (const auto &key : keysToRemove) {
        m_connections.remove(key);
    }
    
    
    for (int sourceNumber : m_sourceNumbers) {
        updateConnectionButton(targetNumber, sourceNumber);
    }
}

void MatrixWidget::rebuild()
{
    buildGrid();
}

void MatrixWidget::buildGrid()
{
    
    if (!m_buttons.isEmpty()) {
        qDebug() << "Grid already built, refreshing button states only";
        for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
            updateConnectionButton(it.key().first, it.key().second);
        }
        return;
    }
    
    
    clearLayoutAndWidgets(m_targetHeaderLayout);
    clearLayoutAndWidgets(m_sourcesSidebarLayout);
    clearLayoutAndWidgets(m_buttonGridLayout);
    m_buttons.clear();
    
    
    if (m_targetNumbers.isEmpty() || m_sourceNumbers.isEmpty()) {
        auto *emptyLabel = new QLabel("No matrix data", m_buttonGridContainer);
        emptyLabel->setStyleSheet("color: #888;");
        m_buttonGridLayout->addWidget(emptyLabel, 0, 0);
        return;
    }
    
    
    for (int col = 0; col < m_targetNumbers.size(); col++) {
        int tgtNum = m_targetNumbers[col];
        QString label = m_targetLabels.value(tgtNum, QString("T%1").arg(tgtNum));
        auto *rotatedLabel = new RotatedLabel(label, BUTTON_SIZE, 
                                              m_targetHeaderContainer);
        m_targetHeaderLayout->addWidget(rotatedLabel);
    }
    
    
    for (int row = 0; row < m_sourceNumbers.size(); row++) {
        int srcNum = m_sourceNumbers[row];
        QString label = m_sourceLabels.value(srcNum, QString("S%1").arg(srcNum));
        
        auto *srcLabel = new SourceLabel(label, BUTTON_SIZE, m_sourcesSidebarContainer);
        m_sourcesSidebarLayout->addWidget(srcLabel);
    }
    
    
    for (int row = 0; row < m_sourceNumbers.size(); row++) {
        int srcNum = m_sourceNumbers[row];
        
        for (int col = 0; col < m_targetNumbers.size(); col++) {
            int tgtNum = m_targetNumbers[col];
            
            auto *btn = new QPushButton(m_buttonGridContainer);
            btn->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
            btn->setEnabled(m_crosspointsEnabled);
            btn->installEventFilter(this);
            
            std::pair<int, int> key(tgtNum, srcNum);
            m_buttons[key] = btn;
            
            
            connect(btn, &QPushButton::clicked, this, [this, tgtNum, srcNum]() {
                emit crosspointClicked(m_matrixPath, tgtNum, srcNum);
            });
            
            updateConnectionButton(tgtNum, srcNum);
            
            m_buttonGridLayout->addWidget(btn, row, col);  
        }
    }
    
    
    
    
    int targetHeaderWidth = m_targetNumbers.size() * BUTTON_SIZE;
    
    int sourcesSidebarHeight = m_sourceNumbers.size() * BUTTON_SIZE;
    m_sourcesSidebarContainer->setFixedHeight(sourcesSidebarHeight);
    m_targetHeaderContainer->setFixedWidth(targetHeaderWidth);
    m_targetHeaderContainer->adjustSize();
    m_sourcesSidebarContainer->adjustSize();
    m_buttonGridContainer->adjustSize();
}

void MatrixWidget::updateConnectionButton(int targetNumber, int sourceNumber)
{
    std::pair<int, int> key(targetNumber, sourceNumber);
    Q_ASSERT(targetNumber >= 0 && sourceNumber >= 0);
    
    if (!m_buttons.contains(key)) {
        qDebug() << "MatrixWidget::updateConnectionButton - Button not found for Target" << targetNumber << "Source" << sourceNumber << "- Grid has" << m_buttons.size() << "buttons";
        return;
    }
    
    QPushButton *btn = m_buttons[key];
    
    
    
    btn->setStyleSheet(getButtonStyleSheet(targetNumber, sourceNumber));
    btn->setText(getButtonText(targetNumber, sourceNumber));
    btn->setToolTip(getButtonTooltip(targetNumber, sourceNumber));
    
    
    if (m_connections.contains(key) && m_connections[key].disposition == 3) {
        btn->setEnabled(false);
    } else {
        btn->setEnabled(m_crosspointsEnabled);
    }
}


bool MatrixWidget::isButtonHovered(int targetNumber, int sourceNumber) const
{
    if (m_hoverTargetNumber == -1 || m_hoverSourceNumber == -1) {
        return false;
    }
    
    
    int colIdx = m_targetNumbers.indexOf(targetNumber);
    int hoverColIdx = m_targetNumbers.indexOf(m_hoverTargetNumber);
    if (sourceNumber == m_hoverSourceNumber && colIdx != -1 && hoverColIdx != -1 && colIdx <= hoverColIdx) {
        return true;
    }
    
    
    int rowIdx = m_sourceNumbers.indexOf(sourceNumber);
    int hoverRowIdx = m_sourceNumbers.indexOf(m_hoverSourceNumber);
    if (targetNumber == m_hoverTargetNumber && rowIdx != -1 && hoverRowIdx != -1 && rowIdx <= hoverRowIdx) {
        return true;
    }
    
    return false;
}


QString MatrixWidget::getButtonStyleSheet(int targetNumber, int sourceNumber) const
{
    std::pair<int, int> key(targetNumber, sourceNumber);
    bool isConnected = m_connections.contains(key);
    bool isHovered = isButtonHovered(targetNumber, sourceNumber);
    
    
    if (!isConnected && !isHovered) {
        return "QPushButton { "
               "  background-color: #f5f5f5; "
               "  border: 1px solid #ccc; "
               "}";
    }
    
    
    if (!isConnected && isHovered) {
        return "QPushButton { "
               "  background-color: #e0e0e0; "  
               "  border: 2px solid #999; "
               "}";
    }
    
    
    const ConnectionState &state = m_connections[key];
    
    
    if (isHovered) {
        return "QPushButton { "
               "  background-color: #66BB6A; "  
               "  border: 2px solid #2E7D32; "
               "  font-weight: bold; "
               "  color: white; "
               "  font-size: 8pt; "
               "}";
    }
    
    
    switch (state.disposition) {
        case 0:  
            return "QPushButton { "
                   "  background-color: #4CAF50; "
                   "  border: 1px solid #45a049; "
                   "  font-weight: bold; "
                   "  color: white; "
                   "  font-size: 8pt; "
                   "}";
        
        case 1:  
            return "QPushButton { "
                   "  background-color: #FF9800; "
                   "  border: 1px solid #F57C00; "
                   "  font-weight: bold; "
                   "  color: white; "
                   "  font-size: 8pt; "
                   "}";
        
        case 2:  
            return "QPushButton { "
                   "  background-color: #FFC107; "
                   "  border: 1px solid #FFA000; "
                   "  font-weight: bold; "
                   "  color: #333; "
                   "  font-size: 8pt; "
                   "}";
        
        case 3:  
            return "QPushButton { "
                   "  background-color: #4CAF50; "
                   "  border: 2px solid #F44336; "
                   "  font-weight: bold; "
                   "  color: white; "
                   "  font-size: 8pt; "
                   "}";
        
        default:  
            return "QPushButton { "
                   "  background-color: #4CAF50; "
                   "  border: 1px solid #45a049; "
                   "  font-weight: bold; "
                   "  color: white; "
                   "  font-size: 8pt; "
                   "}";
    }
}


QString MatrixWidget::getButtonText(int targetNumber, int sourceNumber) const
{
    std::pair<int, int> key(targetNumber, sourceNumber);
    if (!m_connections.contains(key)) {
        return "";
    }
    
    const ConnectionState &state = m_connections[key];
    switch (state.disposition) {
        case 0: return "✓";      
        case 1: return "~";      
        case 2: return "⏳";     
        case 3: return "🔒";     
        default: return "✓";     
    }
}


QString MatrixWidget::getButtonTooltip(int targetNumber, int sourceNumber) const
{
    std::pair<int, int> key(targetNumber, sourceNumber);
    if (!m_connections.contains(key)) {
        return "";
    }
    
    const ConnectionState &state = m_connections[key];
    switch (state.disposition) {
        case 0: return "Connected (Tally)";
        case 1: return "Modified - Change pending confirmation";
        case 2: return "Pending - Waiting for device";
        case 3: return "Locked - Cannot be changed";
        default: return QString("Connected (Unknown disposition: %1)").arg(state.disposition);
    }
}

bool MatrixWidget::eventFilter(QObject *watched, QEvent *event)
{
    QPushButton *btn = qobject_cast<QPushButton*>(watched);
    if (!btn) {
        return QWidget::eventFilter(watched, event);
    }
    
    
    std::pair<int, int> key(-1, -1);
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
        
        updateHoverHighlight(key.first, key.second);
    } else if (event->type() == QEvent::Leave) {
        
        updateHoverHighlight(-1, -1);
    }
    
    return QWidget::eventFilter(watched, event);
}

void MatrixWidget::updateHoverHighlight(int targetNumber, int sourceNumber)
{
    
    int prevHoverTargetNumber = m_hoverTargetNumber;
    int prevHoverSourceNumber = m_hoverSourceNumber;
    
    
    
    m_hoverTargetNumber = -1;
    m_hoverSourceNumber = -1;
    
    
    if (prevHoverTargetNumber != -1 && prevHoverSourceNumber != -1) {
        
        int prevColIdx = m_targetNumbers.indexOf(prevHoverTargetNumber);
        if (prevColIdx != -1) {
            
            for (int col = 0; col <= prevColIdx; col++) {
                int tgtNum = m_targetNumbers[col];
                if (m_buttons.contains(std::pair<int, int>(tgtNum, prevHoverSourceNumber))) {
                    updateConnectionButton(tgtNum, prevHoverSourceNumber);
                }
            }
        }
        
        
        int prevRowIdx = m_sourceNumbers.indexOf(prevHoverSourceNumber);
        if (prevRowIdx != -1) {
            
            for (int row = 0; row <= prevRowIdx; row++) {
                int srcNum = m_sourceNumbers[row];
                if (m_buttons.contains(std::pair<int, int>(prevHoverTargetNumber, srcNum))) {
                    updateConnectionButton(prevHoverTargetNumber, srcNum);
                }
            }
        }
    }
    
    
    
    m_hoverTargetNumber = targetNumber;
    m_hoverSourceNumber = sourceNumber;
    
    
    if (m_hoverTargetNumber != -1 && m_hoverSourceNumber != -1) {
        
        int colIdx = m_targetNumbers.indexOf(m_hoverTargetNumber);
        if (colIdx != -1) {
            
            for (int col = 0; col <= colIdx; col++) {
                int tgtNum = m_targetNumbers[col];
                if (m_buttons.contains(std::pair<int, int>(tgtNum, m_hoverSourceNumber))) {
                    updateConnectionButton(tgtNum, m_hoverSourceNumber);
                }
            }
        }
        
        
        int rowIdx = m_sourceNumbers.indexOf(m_hoverSourceNumber);
        if (rowIdx != -1) {
            
            for (int row = 0; row <= rowIdx; row++) {
                int srcNum = m_sourceNumbers[row];
                if (m_buttons.contains(std::pair<int, int>(m_hoverTargetNumber, srcNum))) {
                    updateConnectionButton(m_hoverTargetNumber, srcNum);
                }
            }
        }
    }
}

bool MatrixWidget::isConnected(int targetNumber, int sourceNumber) const
{
    std::pair<int, int> key(targetNumber, sourceNumber);
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
    
    
    if (enabled) {
        
        
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(255, 0, 0, 38));  
        setAutoFillBackground(true);
        setPalette(pal);
    } else {
        
        setAutoFillBackground(false);
        setPalette(QPalette());
    }
    
    
    for (QPushButton *btn : m_buttons.values()) {
        btn->setEnabled(enabled);
        if (enabled) {
            btn->setCursor(Qt::PointingHandCursor);
        } else {
            btn->setCursor(Qt::ArrowCursor);
        }
    }
}

