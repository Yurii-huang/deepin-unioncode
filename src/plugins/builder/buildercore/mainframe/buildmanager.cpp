/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhouyi<zhouyi1@uniontech.com>
 *
 * Maintainer: zhouyi<zhouyi1@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "buildmanager.h"
#include "compileoutputpane.h"
#include "problemoutputpane.h"
#include "commonparser.h"
#include "transceiver/buildersender.h"

#include "services/builder/builderservice.h"
#include "services/builder/ioutputparser.h"
#include "services/window/windowservice.h"
#include "services/project/projectinfo.h"
#include "services/builder/buildergenerator.h"

#include "base/abstractaction.h"

using namespace dpfservice;

class BuildManagerPrivate
{
    friend class BuildManager;

    QSharedPointer<QAction> buildAction;
    QSharedPointer<QAction> rebuildAction;
    QSharedPointer<QAction> cleanAction;

    CompileOutputPane *compileOutputPane = nullptr;
    ProblemOutputPane *problemOutputPane = nullptr;

    QString activedKitName;
    QString activedWorkingDir;

    std::unique_ptr<IOutputParser> outputParser = nullptr;
};

BuildManager *BuildManager::instance()
{
    static BuildManager ins;
    return &ins;
}

BuildManager::BuildManager(QObject *parent)
    : QObject(parent)
    , d(new BuildManagerPrivate())
{
    addMenu();

    d->compileOutputPane = new CompileOutputPane();
    d->problemOutputPane = new ProblemOutputPane();
    d->outputParser.reset(new CommonParser());
    connect(d->outputParser.get(), &IOutputParser::addOutput, this, &BuildManager::addOutput, Qt::DirectConnection);
    connect(d->outputParser.get(), &IOutputParser::addTask, d->problemOutputPane, &ProblemOutputPane::addTask, Qt::DirectConnection);

    QObject::connect(this, &BuildManager::sigOutputCompileInfo, this, &BuildManager::slotOutputCompileInfo);
    QObject::connect(this, &BuildManager::sigOutputProblemInfo, this, &BuildManager::slotOutputProblemInfo);
    QObject::connect(this, &BuildManager::sigBuildState, this, &BuildManager::slotBuildState);
}

BuildManager::~BuildManager()
{
    if (d) {
        delete d;
    }
}

void BuildManager::addMenu()
{
    auto &ctx = dpfInstance.serviceContext();
    auto windowService = ctx.service<WindowService>(WindowService::name());
    if (!windowService)
        return;

    auto actionInit = [&](QAction *action, QString actionID, QKeySequence key){
        ActionManager::getInstance()->registerAction(action, actionID, action->text(), key);
        AbstractAction *actionImpl = new AbstractAction(action);
        windowService->addAction(dpfservice::MWM_BUILD, actionImpl);
    };

    d->buildAction.reset(new QAction("Build"));
    actionInit(d->buildAction.get(), "Build.Build", QKeySequence(Qt::Modifier::CTRL | Qt::Key::Key_B));

    d->rebuildAction.reset(new QAction("Rebuild"));
    actionInit(d->rebuildAction.get(), "Build.Rebuild", QKeySequence(Qt::Modifier::CTRL | Qt::Modifier::SHIFT | Qt::Key::Key_B));

    d->cleanAction.reset(new QAction("Clean"));
    actionInit(d->cleanAction.get(), "Build.Clean", QKeySequence(Qt::Modifier::CTRL | Qt::Key::Key_C));

    QObject::connect(d->buildAction.get(), &QAction::triggered,
                     this, &BuildManager::buildProject, Qt::DirectConnection);
    QObject::connect(d->rebuildAction.get(), &QAction::triggered,
                     this, &BuildManager::rebuildProject, Qt::DirectConnection);
    QObject::connect(d->cleanAction.get(), &QAction::triggered,
                     this, &BuildManager::cleanProject, Qt::DirectConnection);
}

void BuildManager::buildProject()
{
    execBuildStep({Build});
}

void BuildManager::rebuildProject()
{
    execBuildStep({Clean, Build});
}

void BuildManager::cleanProject()
{
    execBuildStep({Clean});
}


void BuildManager::execBuildStep(QList<BuildMenuType> menuTypelist)
{
    auto &ctx = dpfInstance.serviceContext();
    auto builderService = ctx.service<BuilderService>(BuilderService::name());
    if (builderService) {
        auto generator = builderService->create<BuilderGenerator>(d->activedKitName);
        if (generator) {
            QList<BuildCommandInfo> list;
            foreach (auto menuType, menuTypelist) {
                BuildCommandInfo info;
                info.kitName = d->activedKitName;
                info.workingDir = d->activedWorkingDir;
                generator->getMenuCommand(info, menuType);
                list.append(info);
            }
            generator->appendOutputParser(d->outputParser);
            execCommands(list);
        }
    }
}

CompileOutputPane *BuildManager::getCompileOutputPane() const
{
    return d->compileOutputPane;
}

ProblemOutputPane *BuildManager::getProblemOutputPane() const
{
    return d->problemOutputPane;
}

void BuildManager::startBuild()
{
    d->compileOutputPane->clearContents();
    d->problemOutputPane->clearContents();

    auto &ctx = dpfInstance.serviceContext();
    auto windowService = ctx.service<WindowService>(WindowService::name());
    if (windowService) {
        windowService->switchWidgetContext("Co&mpile Output");
    }
}

void BuildManager::setActivedProjectInfo(const QString &kitName, const QString &workingDir)
{
    d->activedKitName = kitName;
    d->activedWorkingDir = workingDir;
}

void BuildManager::clearActivedProjectInfo()
{
    d->activedKitName.clear();
    d->activedWorkingDir.clear();
}

void BuildManager::handleCommand(const BuildCommandInfo &commandInfo)
{
    auto &ctx = dpfInstance.serviceContext();
    auto builderService = ctx.service<BuilderService>(BuilderService::name());
    if (builderService) {
        auto generator = builderService->create<BuilderGenerator>(commandInfo.kitName);
        if (generator) {
            generator->appendOutputParser(d->outputParser);
            execCommands({commandInfo});
        }
    }
}

bool BuildManager::execCommands(const QList<BuildCommandInfo> &commandList)
{
    if (!commandList.isEmpty()) {
        startBuild();
        QtConcurrent::run([=](){
            QMutexLocker locker(&releaseMutex);
            for (auto command : commandList) {
                execCommand(command);
            }
        });
    }

    return true;
}

bool BuildManager::execCommand(const BuildCommandInfo &info)
{
    outBuildState(BuildState::kBuilding);
    bool ret = false;
    QString retMsg = tr("Error: The \"%1\" command not found, please install.\n").arg(info.program);
    QProcess process;
    process.setWorkingDirectory(info.workingDir);

    QString startMsg = tr("Start execute command: \"%1\" \"%2\" in workspace \"%3\".\n")
            .arg(info.program, info.arguments.join(" "), info.workingDir);
    outputLog(startMsg, OutputFormat::NormalMessage);

    connect(&process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [&](int exitcode, QProcess::ExitStatus exitStatus) {
        if (0 == exitcode && exitStatus == QProcess::ExitStatus::NormalExit) {
            ret = true;
            retMsg = tr("The process \"%1\" exited normally.\n").arg(process.program());
        } else if (exitStatus == QProcess::NormalExit) {
            ret = false;
            retMsg = tr("The process \"%1\" exited with code %2.\n")
                           .arg(process.program(), QString::number(exitcode));
        } else {
            ret = false;
            retMsg = tr("The process \"%1\" crashed.\n")
                           .arg(process.program());
        }
    });

    connect(&process, &QProcess::readyReadStandardOutput, [&]() {
        process.setReadChannel(QProcess::StandardOutput);
        while (process.canReadLine()) {
            QString line = QString::fromUtf8(process.readLine());
            outputLog(line, OutputFormat::NormalMessage);
        }
    });

    connect(&process, &QProcess::readyReadStandardError, [&]() {
        process.setReadChannel(QProcess::StandardError);
        while (process.canReadLine()) {
            QString line = QString::fromUtf8(process.readLine());
            outputLog(line, OutputFormat::Stderr);
            outputError(line);
        }
    });

    process.start(info.program, info.arguments);
    process.waitForFinished();

    outputLog(retMsg, ret ? OutputFormat::NormalMessage : OutputFormat::Stderr);

    QString endMsg = tr("Execute command finished.\n");
    outputLog(endMsg, OutputFormat::NormalMessage);

    BuildState buildState = ret ? BuildState::kNoBuild : BuildState::kBuildFailed;
    outBuildState(buildState);

    BuilderSender::notifyBuildState(buildState, info);
    return ret;
}

void BuildManager::outputLog(const QString &content, const OutputFormat format)
{
    emit sigOutputCompileInfo(content, format);
}

void BuildManager::outputError(const QString &content)
{
    emit sigOutputProblemInfo(content);
}

void BuildManager::slotOutputCompileInfo(const QString &content, const OutputFormat format)
{
    d->outputParser->stdOutput(content, format);
}

void BuildManager::slotOutputProblemInfo(const QString &content)
{
    d->outputParser->stdError(content);
}

void BuildManager::addOutput(const QString &content, const OutputFormat format)
{
    QString newContent = content;
    if (OutputFormat::NormalMessage == format
            || OutputFormat::ErrorMessage == format) {
        QDateTime curDatetime = QDateTime::currentDateTime();
        QString time = curDatetime.toString("hh:mm:ss");
        newContent = time + ": " + newContent + "\r";
    }
    d->compileOutputPane->appendText(newContent, format);
}

void BuildManager::outBuildState(const BuildState &buildState)
{
    emit sigBuildState(buildState);
}

void BuildManager::slotBuildState(const BuildState &buildState)
{
    switch (buildState) {
    case BuildState::kNoBuild:
    case BuildState::kBuildFailed:
        d->buildAction->setEnabled(true);
        d->rebuildAction->setEnabled(true);
        d->cleanAction->setEnabled(true);
        break;
    case BuildState::kBuilding:
        d->buildAction->setEnabled(false);
        d->rebuildAction->setEnabled(false);
        d->cleanAction->setEnabled(false);
        break;
    }
}
