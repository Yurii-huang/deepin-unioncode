// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "runtime.h"
#include "base/abstractwidget.h"
#include "services/terminal/terminalservice.h"

using namespace dpfservice;

struct RuntimePrivate {
    TerminalService *terminalService = nullptr;
}runtime_pri;

void Runtime::initialize()
{
    qInfo() << __FUNCTION__;
}

bool Runtime::start()
{
    qInfo() << __FUNCTION__;
    auto &ctx = dpfInstance.serviceContext();
    QString errStr;
    if (!ctx.load(dpfservice::TerminalService::name(), &errStr)) {
        qCritical() << errStr;
    }
    runtime_pri.terminalService = dpfGetService(TerminalService);
    if (!runtime_pri.terminalService)
        return false;
    return true;
}

dpf::Plugin::ShutdownFlag Runtime::stop()
{
    return Sync;
}

void Runtime::eventProcess(const Event &e)
{
    if (e.data() == commandLine.build.name) {
        runtime_pri.terminalService;
    }
}
