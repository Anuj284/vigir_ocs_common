/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013-2015, Team ViGIR ( TORC Robotics LLC, TU Darmstadt, Virginia Tech, Oregon State University, Cornell University, and Leibniz University Hanover )
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Team ViGIR, TORC Robotics, nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/
//@TODO_ADD_AUTHOR_INFO
#include "ui_logSbar.h"
#include "logSbar.h"

LogSbar::LogSbar(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LogSbar)
{
    ui->setupUi(this);

    miniError = new MiniError(this);
    Qt::WindowFlags flags = miniError->windowFlags();
    flags |= Qt::WindowStaysOnTopHint;
    flags |= Qt::FramelessWindowHint;
    flags |= Qt::Dialog; // //ensure ghub as a dialog box, not a seperate window/tab
    miniError->setWindowFlags(flags);
   // miniError->show();
    //miniError->setWindowOpacity(0);

    miniJoint = new MiniJoint(this);
    miniJoint->setWindowFlags(flags);
   // miniJoint->show();
   // miniJoint->setWindowOpacity(0);

    //create animations
    errorFadeIn = new QPropertyAnimation(miniError, "windowOpacity");
    errorFadeIn->setEasingCurve(QEasingCurve::InOutQuad);
    errorFadeIn->setDuration(500);
    errorFadeIn->setStartValue(0.0);
    errorFadeIn->setEndValue(.74);

    errorFadeOut = new QPropertyAnimation(miniError, "windowOpacity");
    errorFadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    errorFadeOut->setDuration(300);
    errorFadeOut->setStartValue(0.74);
    errorFadeOut->setEndValue(0.0);

    jointFadeIn = new QPropertyAnimation(miniJoint, "windowOpacity");
    jointFadeIn->setEasingCurve(QEasingCurve::InOutQuad);
    jointFadeIn->setDuration(500);
    jointFadeIn->setStartValue(0.0);
    jointFadeIn->setEndValue(.74);

    jointFadeOut = new QPropertyAnimation(miniJoint, "windowOpacity");
    jointFadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    jointFadeOut->setDuration(300);
    jointFadeOut->setStartValue(0.74);
    jointFadeOut->setEndValue(0.0);

    setJointStatus(JOINT_OK);//default

    numError = 0;
    ui->errorCount->setText("0");

    //want to hide window when fadeout finishes
    connect(errorFadeOut,SIGNAL(finished()),this,SLOT(hideErrorWindow()));
    connect(jointFadeOut,SIGNAL(finished()),this,SLOT(hideJointWindow()));

    //connect right click windows
    connect(ui->jointWidget,SIGNAL(toggleJointList()),miniJoint,SLOT(toggleJointListWindow()));
    connect(ui->errorWidget,SIGNAL(toggleErrorLog()),miniError,SLOT(toggleErrorLogWindow()));
    window_control_pub_ = n_.advertise<std_msgs::Int8>( "/flor/ocs/window_control", 1, false);

}

void LogSbar::hideJointWindow()
{
    miniJoint->hide();
}
void LogSbar::hideErrorWindow()
{
    miniError->hide();
}

//wrapper to ignore string from signal
void LogSbar::receiveJointData(int status,QString jointName)
{
    setJointStatus(status);
}
void LogSbar::setJointStatus(int status)
{
    switch(status)
    {
    case JOINT_OK:
        ui->jointStatus->setText("OK");
        ui->jointStatus->setStyleSheet("QLabel{color: green; }");
        break;
    case JOINT_WARN:
        ui->jointStatus->setText("WARN");
        ui->jointStatus->setStyleSheet("QLabel{color: yellow; }");
        break;
    case JOINT_ERROR:
        ui->jointStatus->setText("ERROR");
        ui->jointStatus->setStyleSheet("QLabel{color: red; }");
        break;
    }
}

void LogSbar::receiveErrorData(QString time, QString message)
{
    numError++;
    QString s = QString::number(numError);
    ui->errorCount->setText(s);
    ui->errorCount->setStyleSheet("QLabel{color: red; }");
}

//always used to set 0
void LogSbar::resetErrorCount()
{
    numError = 0;
    QString s = QString::number(numError);
    ui->errorCount->setText(s);
    ui->errorCount->setStyleSheet("QLabel{color: rgb(80,80,80); }");
}

//methods for emitting signals with reference to parent
void LogSbar::notifyMiniError()
{
    Q_EMIT makeErrorActive();
}
void LogSbar::notifyMiniJoint()
{
    Q_EMIT makeJointActive();
}

LogSbar::~LogSbar()
{
    delete(errorFadeIn);
    delete(errorFadeOut);
    delete(jointFadeIn);
    delete(jointFadeOut);
    delete ui;
}

ErrorWidget::ErrorWidget(QWidget * parent):
    QWidget(parent)
{
    //grab reference to LogSbar  //tight coupling but only special occurence
    myParent = qobject_cast<LogSbar*>(this->parent()->parent()->parent());
}

//mouse enter/leave trigger these methods
void ErrorWidget::enterEvent(QEvent * event)
{
    //pull up mini error window
    myParent->getMiniError()->show();
    myParent->getErrorFadeIn()->start();
    myParent->getMiniError()->setGeometry(myParent->getUi()->statusBar->mapToGlobal(QPoint(0,0)).x(),myParent->getUi()->statusBar->mapToGlobal(QPoint(0,0)).y() - 150,400, 150);
    myParent->notifyMiniError();
}
void ErrorWidget::leaveEvent(QEvent * event)
{
    myParent->getErrorFadeOut()->start();

}

void ErrorWidget::mousePressEvent(QMouseEvent * event)
{
    myParent->resetErrorCount();
    myParent->getMiniError()->setViewed();
}

ErrorWidget::~ErrorWidget()
{
}


JointWidget::JointWidget(QWidget * parent):
    QWidget(parent)
{
    myParent = qobject_cast<LogSbar*>(this->parent()->parent()->parent());
}

void JointWidget::enterEvent(QEvent * event)
{
    myParent->getMiniJoint()->show();
    myParent->getJointFadeIn()->start();
    myParent->getMiniJoint()->setGeometry(myParent->getUi()->statusBar->mapToGlobal(QPoint(0,0)).x() + 100,myParent->getUi()->statusBar->mapToGlobal(QPoint(0,0)).y() - 150,400, 150);
    myParent->notifyMiniJoint();
}
void JointWidget::leaveEvent(QEvent * event)
{
    myParent->getJointFadeOut()->start();
}


JointWidget::~JointWidget()
{

}

