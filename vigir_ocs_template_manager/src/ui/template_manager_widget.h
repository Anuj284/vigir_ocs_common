#ifndef TemplateManagerWidget_H
#define TemplateManagerWidget_H


#include <QMainWindow>
#include <QWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>

#include <QPainter>
#include <QtGui>


namespace Ui {
class TemplateManagerWidget;
}

class TemplateManagerWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit TemplateManagerWidget(QWidget *parent = 0);
    ~TemplateManagerWidget();
    
private:
    void addTreeWidgetChild(QTreeWidgetItem* item);

    Ui::TemplateManagerWidget* ui;
    QString templateDirPath;
    QString templatePath;

public Q_SLOTS:
    void treeItemChanged(QTreeWidgetItem * current, QTreeWidgetItem * previous);
    void insertButtonPressed();

Q_SIGNALS:
    void insertTemplate(QString);

};

#endif // TemplateManagerWidget_H
