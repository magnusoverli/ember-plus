/*
 * MatrixWidget.h - Widget for displaying Ember+ matrix crosspoints
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

// Custom widget for rotated text labels (90° counter-clockwise)
class RotatedLabel : public QWidget
{
    Q_OBJECT
public:
    RotatedLabel(const QString &text, int buttonWidth, int labelHeight, int maxTextLength = 120, QWidget *parent = nullptr);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
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
    void setConnection(int targetNumber, int sourceNumber, bool connected);
    void clearConnections();
    void rebuild();
    
    // Query connection state
    bool isConnected(int targetNumber, int sourceNumber) const;
    int getMatrixType() const { return m_matrixType; }
    
    // Crosspoint editing control
    void setCrosspointsEnabled(bool enabled);

signals:
    void crosspointClicked(const QString &matrixPath, int targetNumber, int sourceNumber);
    
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    
private:
    void buildGrid();
    void updateConnectionButton(int targetNumber, int sourceNumber);
    void updateHoverHighlight(int targetNumber, int sourceNumber);
    
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
    
    // Connection state: (targetNumber, sourceNumber) -> connected
    QSet<QPair<int, int>> m_connections;
    
    // UI components
    QGridLayout *m_grid;
    QWidget *m_gridWidget;
    QLabel *m_headerLabel;
    
    // Button references: (targetNumber, sourceNumber) -> button
    QMap<QPair<int, int>, QPushButton*> m_buttons;
    
    // Hover tracking
    int m_hoverTargetNumber;
    int m_hoverSourceNumber;
    
    // Crosspoint editing state
    bool m_crosspointsEnabled;
    QString m_matrixPath;  // OID path for signal emission
    
    // Constants
    static const int BUTTON_SIZE = 18;
    static const int GRID_SPACING = 0;  // No spacing for seamless hover
    static const int LABEL_HEIGHT = 120;  // Height for rotated target labels
    static const int MAX_LABEL_WIDTH = 100;
};

#endif // MATRIXWIDGET_H

