#ifndef BEHAVIOR_NOTIFICATION_H
#define BEHAVIOR_NOTIFICATION_H

#include <QMainWindow>
#include <ros/ros.h>
#include <ros/publisher.h>
#include <ros/subscriber.h>
#include <QtCore>
#include <QMessageBox>
#include <QMouseEvent>
#include <vigir_be_input/BehaviorInputAction.h>
#include "complex_action_server.h"

//typedef typename ActionServer<ActionSpec>::GoalHandlePtr GoalHandlePtr;

namespace Ui
{
    class BehaviorNotification; //window name
}

typedef actionlib::ComplexActionServer<vigir_be_input::BehaviorInputAction> BehaviorServer;

class BehaviorNotification : public QWidget
{
    Q_OBJECT

public:
    ~BehaviorNotification();
    explicit BehaviorNotification(QWidget *parent = 0);
    bool getConfirmed() { return confirmed_; }
    void setActionText(QString);
    void setGoal(BehaviorServer::GoalHandlePtr );

private:
    bool eventFilter(QObject* object,QEvent* event);

    Ui::BehaviorNotification *ui;
    bool confirmed_;
    BehaviorServer::GoalHandlePtr goal_;


public Q_SLOTS:
    void confirm();
    void abort();


private Q_SLOTS:


Q_SIGNALS:
    void sendConfirmation(QString,const BehaviorServer::GoalHandlePtr);
    void sendAbort(QString,const BehaviorServer::GoalHandlePtr);
};


#endif // BEHAVIOR_NOTIFICATION_H
