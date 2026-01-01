#ifndef DIALOGPLUGIN_H
#define DIALOGPLUGIN_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <Agent/Commander.h>

class DialogPlugin : public QDialog
{
Q_OBJECT

    QString commandPath;
    QString description;
    QList<Argument> arguments;
    
    QGridLayout* formLayout = nullptr;
    QTextEdit* descriptionEdit = nullptr;
    QDialogButtonBox* buttonBox = nullptr;
    
    QMap<QString, QWidget*> inputWidgets;
    
    void createUI();
    QWidget* createInputWidget(const Argument& arg);

public:
    explicit DialogPlugin(const QString& commandPath, const QString& description, const QList<Argument>& args, QWidget* parent = nullptr);
    ~DialogPlugin() override;
    
    QString buildCommandLine() const;
    QMap<QString, QString> getValues() const;
};

#endif
