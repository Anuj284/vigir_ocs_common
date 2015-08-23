#ifndef MINIERROR_H
#define MINIERROR_H

#include <QMainWindow>
#include <ros/ros.h>
#include <vigir_control_msgs/VigirControlModeCommand.h>
#include <vigir_ocs_msgs/OCSRobotStatus.h>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include "robotStatus.h"

namespace Ui {
class MiniError;
}


class MiniError : public QMainWindow
{
    Q_OBJECT

public:
    ~MiniError();
    void setViewed();
    explicit MiniError(QWidget *parent = 0);


private:
    Ui::MiniError *ui;
    int newErrors;
    void enterEvent(QEvent*);
    void leaveEvent(QEvent*);
    QPropertyAnimation * errorFadeIn;
    QPropertyAnimation * errorFadeOut;
    QTableWidgetItem * simTime;
    QTableWidgetItem * errorMessage;
    robotStatus * robStatus;
    QTimer* timer;
    bool visible;
    QRect * originalGeometry;
    bool eventFilter(QObject* object,QEvent* event);

public Q_SLOTS:
    void receiveErrorData(QString,QString);
    void startActiveTimer();
    void toggleErrorLogWindow();

private Q_SLOTS:
    void hideWindow();

};


#endif // MINIERROR
