/*
    EmberViewer - Widget for displaying Ember+ matrix crosspoints
    
    Copyright (C) 2025 Magnus Overli
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MATRIXWIDGET_H
#define MATRIXWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QPainter>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QSplitter>

// Custom widget for rotated text labels (90° counter-clockwise)
class RotatedLabel : public QWidget
{
    Q_OBJECT
public:
    RotatedLabel(const QString &text, int buttonWidth, QWidget *parent = nullptr);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    
private:

    void updateDisplayText();
    QString m_fullText;
    QString m_displayText;
};

// Custom widget for source labels (horizontal expansion)
class SourceLabel : public QWidget
{
    Q_OBJECT
public:
    SourceLabel(const QString &text, int buttonHeight, QWidget *parent = nullptr);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    
private:
    void updateDisplayText();
    QString m_fullText;
    QString m_displayText;
};

// Main matrix display widget
class MatrixWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit MatrixWidget(QWidget *parent = nullptr);
    ~MatrixWidget();
    
    // Update matrix data
    void setMatrixInfo(const QString &identifier, const QString &description, 
                       int type, int targetCount, int sourceCount);
    void setMatrixPath(const QString &path);
    void setTargetLabel(int targetNumber, const QString &label);
    void setSourceLabel(int sourceNumber, const QString &label);
    void setConnection(int targetNumber, int sourceNumber, bool connected, int disposition);
    void clearConnections();
    void clearTargetConnections(int targetNumber);
    void rebuild();
    
    // Query connection state
    bool isConnected(int targetNumber, int sourceNumber) const;
    int getMatrixType() const { return m_matrixType; }
    QString getTargetLabel(int targetNumber) const;
    QString getSourceLabel(int sourceNumber) const;
    QList<int> getTargetNumbers() const { return m_targetNumbers; }
    QList<int> getSourceNumbers() const { return m_sourceNumbers; }
    
    // Crosspoint editing control
    void setCrosspointsEnabled(bool enabled);

signals:
    void crosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);
    void crosspointToggleRequested();
    
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    
private slots:
    void onTopSplitterMoved(int pos, int index);
    void onBottomSplitterMoved(int pos, int index);
    void onVerticalSplitterMoved(int pos, int index);

private:
    void buildGrid();
    void updateConnectionButton(int targetNumber, int sourceNumber);
    void updateHoverHighlight(int targetNumber, int sourceNumber);
    void connectScrollSync();
    void clearLayoutAndWidgets(QLayout *layout);
    void enforceStaticHandlePositions();
    
    // Style calculation helpers
    bool isButtonHovered(int targetNumber, int sourceNumber) const;
    QString getButtonStyleSheet(int targetNumber, int sourceNumber) const;
    QString getButtonText(int targetNumber, int sourceNumber) const;
    QString getButtonTooltip(int targetNumber, int sourceNumber) const;
    
    // Matrix metadata
    QString m_identifier;
    QString m_description;
    int m_matrixType;  // 0=OneToN, 1=OneToOne, 2=NToN
    int m_targetCount;  // Expected count (might not match actual)
    int m_sourceCount;  // Expected count (might not match actual)
    
    // Actual target/source numbers (can be sparse/non-contiguous)
    QList<int> m_targetNumbers;  // Ordered list of actual target numbers
    QList<int> m_sourceNumbers;  // Ordered list of actual source numbers
    
    // Labels
    QMap<int, QString> m_targetLabels;  // targetNumber -> label
    QMap<int, QString> m_sourceLabels;  // sourceNumber -> label
    
    // Connection state with disposition
    struct ConnectionState {
        bool connected;
        int disposition;  // 0=Tally, 1=Modified, 2=Pending, 3=Locked
    };
    QMap<QPair<int, int>, ConnectionState> m_connections;
    
    // UI components - Frozen pane structure with splitters
    QLabel *m_headerLabel;
    
    // Splitter structure
    QSplitter *m_outerVerticalSplitter;
    QSplitter *m_topHorizontalSplitter;
    QSplitter *m_bottomHorizontalSplitter;
    
    // Top row widgets
    QPushButton *m_cornerWidget;
    QScrollArea *m_targetHeaderScrollArea;
    
    // Bottom row widgets
    QScrollArea *m_sourcesSidebarScrollArea;
    QScrollArea *m_buttonGridScrollArea;
    
    // Container widgets
    QWidget *m_targetHeaderContainer;
    QWidget *m_sourcesSidebarContainer;
    QWidget *m_buttonGridContainer;
    
    // Layouts
    QHBoxLayout *m_targetHeaderLayout;
    QVBoxLayout *m_sourcesSidebarLayout;
    QGridLayout *m_buttonGridLayout;
    
    // Button references: (targetNumber, sourceNumber) -> button
    QMap<QPair<int, int>, QPushButton*> m_buttons;
    
    // Hover tracking
    int m_hoverTargetNumber;
    int m_hoverSourceNumber;
    
    // Crosspoint editing state
    bool m_crosspointsEnabled;
    QString m_matrixPath;  // OID path for signal emission
    
    // Splitter state tracking
    bool m_userAdjustedHandles;
    
    // Constants
    static const int BUTTON_SIZE = 18;
    static const int GRID_SPACING = 0;  // No spacing for seamless hover
    static const int LABEL_HEIGHT = 120;  // Height for rotated target labels
    static const int MAX_LABEL_WIDTH = 100;
};

#endif // MATRIXWIDGET_H

