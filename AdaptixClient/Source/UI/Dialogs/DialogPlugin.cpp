#include <UI/Dialogs/DialogPlugin.h>
#include <QGroupBox>

DialogPlugin::DialogPlugin(const QString& cmdPath, const QString& desc, const QList<Argument>& args, QWidget* parent)
    : QDialog(parent), commandPath(cmdPath), description(desc), arguments(args)
{
    setWindowTitle(commandPath);
    setMinimumWidth(450);
    createUI();
}

DialogPlugin::~DialogPlugin() = default;

void DialogPlugin::createUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    
    // Description section
    if (!description.isEmpty()) {
        QGroupBox* descGroup = new QGroupBox("Description", this);
        QVBoxLayout* descLayout = new QVBoxLayout(descGroup);
        descriptionEdit = new QTextEdit(this);
        descriptionEdit->setPlainText(description);
        descriptionEdit->setReadOnly(true);
        descriptionEdit->setMaximumHeight(80);
        descLayout->addWidget(descriptionEdit);
        mainLayout->addWidget(descGroup);
    }
    
    // Parameters section
    if (!arguments.isEmpty()) {
        QGroupBox* paramGroup = new QGroupBox("Parameters", this);
        formLayout = new QGridLayout(paramGroup);
        formLayout->setColumnStretch(1, 1);
        
        int row = 0;
        for (const Argument& arg : arguments) {
            QString labelText = arg.name;
            if (arg.required)
                labelText += " *";
            if (!arg.description.isEmpty())
                labelText += QString(" (%1)").arg(arg.description);
            
            QLabel* label = new QLabel(labelText, this);
            QWidget* input = createInputWidget(arg);
            
            formLayout->addWidget(label, row, 0);
            formLayout->addWidget(input, row, 1);
            
            inputWidgets[arg.name] = input;
            row++;
        }
        
        mainLayout->addWidget(paramGroup);
    }
    
    // Buttons
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Execute");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    setLayout(mainLayout);
}

QWidget* DialogPlugin::createInputWidget(const Argument& arg)
{
    if (arg.type == "file") {
        QWidget* container = new QWidget(this);
        QHBoxLayout* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        
        QLineEdit* lineEdit = new QLineEdit(container);
        lineEdit->setObjectName("input_" + arg.name);
        if (arg.defaultUsed)
            lineEdit->setText(arg.defaultValue.toString());
        
        QPushButton* browseBtn = new QPushButton("...", container);
        browseBtn->setFixedWidth(30);
        connect(browseBtn, &QPushButton::clicked, [lineEdit]() {
            QString file = QFileDialog::getOpenFileName(nullptr, "Select File");
            if (!file.isEmpty())
                lineEdit->setText(file);
        });
        
        layout->addWidget(lineEdit);
        layout->addWidget(browseBtn);
        return container;
        
    } else if (arg.type == "int") {
        QSpinBox* spinBox = new QSpinBox(this);
        spinBox->setObjectName("input_" + arg.name);
        spinBox->setRange(-999999, 999999);
        if (arg.defaultUsed)
            spinBox->setValue(arg.defaultValue.toInt());
        return spinBox;
        
    } else if (arg.type == "bool") {
        QCheckBox* checkBox = new QCheckBox(this);
        checkBox->setObjectName("input_" + arg.name);
        if (arg.defaultUsed)
            checkBox->setChecked(arg.defaultValue.toBool());
        return checkBox;
        
    } else {
        // Default: string type
        QLineEdit* lineEdit = new QLineEdit(this);
        lineEdit->setObjectName("input_" + arg.name);
        if (arg.defaultUsed)
            lineEdit->setText(arg.defaultValue.toString());
        if (!arg.mark.isEmpty())
            lineEdit->setPlaceholderText(arg.mark);
        return lineEdit;
    }
}

QString DialogPlugin::buildCommandLine() const
{
    QString cmdLine = commandPath;
    
    for (const Argument& arg : arguments) {
        if (!inputWidgets.contains(arg.name))
            continue;
        
        QWidget* widget = inputWidgets[arg.name];
        QString value;
        
        if (arg.type == "file") {
            QLineEdit* lineEdit = widget->findChild<QLineEdit*>();
            if (lineEdit)
                value = lineEdit->text();
        } else if (arg.type == "int") {
            QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget);
            if (spinBox)
                value = QString::number(spinBox->value());
        } else if (arg.type == "bool") {
            QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget);
            if (checkBox && checkBox->isChecked())
                value = "true";
        } else {
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget);
            if (lineEdit)
                value = lineEdit->text();
        }
        
        if (value.isEmpty() && !arg.required)
            continue;
        
        // Handle flag arguments
        if (arg.flag && !arg.mark.isEmpty()) {
            if (!value.isEmpty()) {
                if (value.contains(' '))
                    cmdLine += QString(" %1 \"%2\"").arg(arg.mark, value);
                else
                    cmdLine += QString(" %1 %2").arg(arg.mark, value);
            }
        } else {
            if (value.contains(' '))
                cmdLine += QString(" \"%1\"").arg(value);
            else
                cmdLine += " " + value;
        }
    }
    
    return cmdLine;
}

QMap<QString, QString> DialogPlugin::getValues() const
{
    QMap<QString, QString> values;
    
    for (const Argument& arg : arguments) {
        if (!inputWidgets.contains(arg.name))
            continue;
        
        QWidget* widget = inputWidgets[arg.name];
        QString value;
        
        if (arg.type == "file") {
            QLineEdit* lineEdit = widget->findChild<QLineEdit*>();
            if (lineEdit)
                value = lineEdit->text();
        } else if (arg.type == "int") {
            QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget);
            if (spinBox)
                value = QString::number(spinBox->value());
        } else if (arg.type == "bool") {
            QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget);
            value = checkBox && checkBox->isChecked() ? "true" : "false";
        } else {
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget);
            if (lineEdit)
                value = lineEdit->text();
        }
        
        values[arg.name] = value;
    }
    
    return values;
}
