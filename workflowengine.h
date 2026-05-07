#ifndef WORKFLOWENGINE_H
#define WORKFLOWENGINE_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include "serialmanager.h"
#include "axiscontroller.h"
#include "liquidlenscontroller.h"

class WorkflowEngine : public QObject
{
    Q_OBJECT

public:
    enum WorkflowState {
        Stopped,
        Initializing,
        WaitingForAxis,
        Measuring,
        AdjustingFocus,
        Paused,
        Error
    };
    Q_ENUM(WorkflowState)

    struct ScanPoint {
        double position;
        double measuredDistance;
        int focusValue;
    };

    explicit WorkflowEngine(QObject *parent = nullptr);
    ~WorkflowEngine();

    void setLaserSensor(SerialManager *sensor);
    void setAxisController(AxisController *axis);
    void setLiquidLens(LiquidLensController *lens);

    void startSinglePoint();
    void startContinuousScan(double startPos, double endPos, double stepSize);
    void startContinuousScan(const QList<double> &positions);
    void pause();
    void resume();
    void stop();

    WorkflowState state() const;
    QList<ScanPoint> scanResults() const;
    void clearResults();

    void setMeasureDelay(int msec);
    void setAutoFocusEnabled(bool enabled);
    void setScanAxis(int axis);

signals:
    void stateChanged(WorkflowState state);
    void distanceMeasured(double distance, int focusValue);
    void scanPointCompleted(const ScanPoint &point);
    void scanFinished();
    void progressChanged(int current, int total);
    void errorOccurred(const QString &error);
    void logMessage(const QString &msg);

private slots:
    void onSensorDataReceived(const QByteArray &data);
    void onAxisMoveFinished(bool success, int axis);
    void onAxisPositionChanged(double position, int axis);
    void onMeasureTimeout();

private:
    void setState(WorkflowState state);
    void processNextPoint();
    double parseDistance(const QByteArray &data);
    void requestMeasurement();

    SerialManager *m_laserSensor;
    AxisController *m_axis;
    LiquidLensController *m_lens;

    WorkflowState m_state;
    QQueue<double> m_scanPositions;
    QList<ScanPoint> m_results;
    int m_currentIndex;
    int m_totalPoints;

    QTimer *m_measureTimer;
    int m_measureDelay;
    bool m_autoFocusEnabled;
    bool m_continuousMode;
    double m_lastDistance;
    double m_currentAxisPos;
    int m_scanAxis;

    double m_pendingDistance;
};

#endif
