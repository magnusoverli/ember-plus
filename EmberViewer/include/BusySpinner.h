#ifndef BUSYSPINNER_H
#define BUSYSPINNER_H

#include <QWidget>
#include <QTimer>
#include <QColor>
#include <QSize>

class BusySpinner : public QWidget
{
    Q_OBJECT

public:
    explicit BusySpinner(QWidget *parent = nullptr);
    ~BusySpinner();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void start();
    void stop();
    bool isSpinning() const { return m_spinning; }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void rotate();

private:
    QTimer *m_timer;
    int m_angle;
    bool m_spinning;
    QColor m_color;
    
    static constexpr int ROTATION_INTERVAL_MS = 80;
    static constexpr int ANGLE_STEP = 30;
    static constexpr int WIDGET_SIZE = 20;
};

#endif
