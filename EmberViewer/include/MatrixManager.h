









#ifndef MATRIXMANAGER_H
#define MATRIXMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>

class VirtualizedMatrixWidget;
class EmberConnection;
class QWidget;

class MatrixManager : public QObject
{
    Q_OBJECT

public:
    explicit MatrixManager(EmberConnection *connection, QObject *parent = nullptr);
    ~MatrixManager();

    
    QWidget* getMatrix(const QString &path) const;
    void clear();

public slots:
    
    void onMatrixReceived(const QString &path, int number, const QString &identifier, const QString &description,
                         int type, int targetCount, int sourceCount);
    void onMatrixTargetReceived(const QString &matrixPath, int targetNumber, const QString &label);
    void onMatrixSourceReceived(const QString &matrixPath, int sourceNumber, const QString &label);
    void onMatrixConnectionReceived(const QString &matrixPath, int targetNumber, int sourceNumber, bool connected, int disposition);
    void onMatrixConnectionsCleared(const QString &matrixPath);
    void onMatrixTargetConnectionsCleared(const QString &matrixPath, int targetNumber);

signals:
    void matrixWidgetCreated(const QString &path, QWidget *widget);
    void matrixDimensionsUpdated(const QString &path, QWidget *widget);

private:
    EmberConnection *m_connection;
    QMap<QString, QWidget*> m_matrixWidgets;  
    
    static constexpr int MATRIX_LABEL_PATH_MARKER = 666999666;
};

#endif 
