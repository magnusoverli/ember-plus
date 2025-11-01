







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
#include <utility>
#include <QSplitter>


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


class MatrixWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit MatrixWidget(QWidget *parent = nullptr);
    ~MatrixWidget();
    
    
    void setMatrixInfo(const QString &identifier, const QString &description, 
                       int type, int targetCount, int sourceCount);
    void setMatrixPath(const QString &path);
    void setTargetLabel(int targetNumber, const QString &label);
    void setSourceLabel(int sourceNumber, const QString &label);
    void setConnection(int targetNumber, int sourceNumber, bool connected, int disposition);
    void clearConnections();
    void clearTargetConnections(int targetNumber);
    void rebuild();
    
    
    bool isConnected(int targetNumber, int sourceNumber) const;
    int getMatrixType() const { return m_matrixType; }
    QString getTargetLabel(int targetNumber) const;
    QString getSourceLabel(int sourceNumber) const;
    QList<int> getTargetNumbers() const { return m_targetNumbers; }
    QList<int> getSourceNumbers() const { return m_sourceNumbers; }
    
    
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
    
    
    bool isButtonHovered(int targetNumber, int sourceNumber) const;
    QString getButtonStyleSheet(int targetNumber, int sourceNumber) const;
    QString getButtonText(int targetNumber, int sourceNumber) const;
    QString getButtonTooltip(int targetNumber, int sourceNumber) const;
    
    
    QString m_identifier;
    QString m_description;
    int m_matrixType;  
    int m_targetCount;  
    int m_sourceCount;  
    
    
    QList<int> m_targetNumbers;  
    QList<int> m_sourceNumbers;  
    
    
    QMap<int, QString> m_targetLabels;  
    QMap<int, QString> m_sourceLabels;  
    
    
    struct ConnectionState {
        bool connected;
        int disposition;  
    };
    QMap<std::pair<int, int>, ConnectionState> m_connections;
    
    
    QLabel *m_headerLabel;
    
    
    QSplitter *m_outerVerticalSplitter;
    QSplitter *m_topHorizontalSplitter;
    QSplitter *m_bottomHorizontalSplitter;
    
    
    QPushButton *m_cornerWidget;
    QScrollArea *m_targetHeaderScrollArea;
    
    
    QScrollArea *m_sourcesSidebarScrollArea;
    QScrollArea *m_buttonGridScrollArea;
    
    
    QWidget *m_targetHeaderContainer;
    QWidget *m_sourcesSidebarContainer;
    QWidget *m_buttonGridContainer;
    
    
    QHBoxLayout *m_targetHeaderLayout;
    QVBoxLayout *m_sourcesSidebarLayout;
    QGridLayout *m_buttonGridLayout;
    
    
    QMap<std::pair<int, int>, QPushButton*> m_buttons;
    
    
    int m_hoverTargetNumber;
    int m_hoverSourceNumber;
    
    
    bool m_crosspointsEnabled;
    QString m_matrixPath;  
    
    
    bool m_userAdjustedHandles;
    
    
    static const int BUTTON_SIZE = 18;
    static const int GRID_SPACING = 0;  
    static const int LABEL_HEIGHT = 120;  
    static const int MAX_LABEL_WIDTH = 100;
};

#endif 

