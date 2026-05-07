#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "serialmanager.h"
#include "axiscontroller.h"
#include "liquidlenscontroller.h"
#include "workflowengine.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRefreshPorts();

    void onConnectLaser();
    void onDisconnectLaser();
    void onConnectLens();
    void onDisconnectLens();
    void onConnectAxis();
    void onDisconnectAxis();

    void onAxisMoveTo();
    void onAxisHome();
    void onAxisStop();
    void onAxisSpeedChanged(int value);

    void onLensFocusChanged(int value);
    void onLensCalibrate();

    void onSingleMeasure();
    void onStartScan();
    void onPauseScan();
    void onResumeScan();
    void onStopScan();

    void onWorkflowStateChanged(WorkflowEngine::WorkflowState state);
    void onDistanceMeasured(double distance, int focusValue);
    void onScanPointCompleted(const WorkflowEngine::ScanPoint &point);
    void onScanFinished();
    void onProgressChanged(int current, int total);
    void onLogMessage(const QString &msg);

    void onLaserDataReceived(const QByteArray &data);

private:
    void initUi();
    void initConnections();
    void updatePortComboBoxes();
    void updateConnectionStatus();
    void appendLog(const QString &msg);

    Ui::MainWindow *ui;

    SerialManager *m_laserSensor;
    AxisController *m_axisController;
    LiquidLensController *m_lensController;
    WorkflowEngine *m_workflow;
};

#endif // MAINWINDOW_H
