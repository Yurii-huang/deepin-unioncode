// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COREPLUGIN_H
#define COREPLUGIN_H

#include <framework/framework.h>
#include "common/util/eventdefinitions.h"

DPF_USE_NAMESPACE

class Runtime : public dpf::Plugin, EventHandler, public AutoEventHandlerRegister<Runtime>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.deepin.plugin.unioncode" FILE "runtime.json")
public:
    static EventHandler::Type type()
    {
        return EventHandler::Type::Sync;
    }

    static QStringList topics()
    {
        return { runtime.topic, commandLine.topic };
    }

    virtual void initialize() override;
    virtual bool start() override;
    virtual dpf::Plugin::ShutdownFlag stop() override;
    virtual void eventProcess(const Event&) override;
};

#endif // COREPLUGIN_H
