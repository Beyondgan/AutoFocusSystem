#ifndef LIQUIDLENSCONTROLLER_H
#define LIQUIDLENSCONTROLLER_H

#include <QObject>
#include <QMap>
#include "serialmanager.h"

class LiquidLensController : public QObject
{
    Q_OBJECT

public:
    explicit LiquidLensController(QObject *parent = nullptr);
    ~LiquidLensController();

    bool connectLens(const QString &portName, qint32 baudRate = 115200);
    void disconnectLens();
    bool isConnected() const;

    void setFocusByDistance(double distanceMm);
    void setFocusDirect(int focusValue);
    int currentFocusValue() const;
    double currentDistance() const;

    void addCalibrationPoint(double distanceMm, int focusValue);
    void clearCalibrationPoints();
    void loadCalibration(const QMap<double, int> &points);

    void setMinFocus(int min);
    void setMaxFocus(int max);
    int minFocus() const;
    int maxFocus() const;

    int distanceToFocus(double distanceMm) const;

signals:
    void focusChanged(int focusValue);
    void distanceUpdated(double distanceMm);
    void errorOccurred(const QString &error);

private slots:
    void onLensDataReceived(const QByteArray &data);
    void onLensError(const QString &error);

private:
    int interpolateFocus(double distanceMm) const;
    QByteArray buildLensCommand(int focusValue);

    SerialManager *m_serial;
    int m_currentFocus;
    double m_currentDistance;
    int m_minFocus;
    int m_maxFocus;
    QMap<double, int> m_calibration;
};

#endif // LIQUIDLENSCONTROLLER_H
