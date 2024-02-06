// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationbar.h"
#include "uicontroller/controller.h"

#include <DStyle>

DWIDGET_USE_NAMESPACE
NavigationBar::NavigationBar(QWidget *parent)
    : DFrame(parent)
{
    setLineWidth(0);
    setFixedWidth(58);

    DStyle::setFrameRadius(this, 0);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignHCenter);

    topLayout = new QVBoxLayout;
    topLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    topLayout->setSpacing(5);

    bottomLayout = new QVBoxLayout;
    bottomLayout->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    bottomLayout->setSpacing(5);

    layout->addLayout(topLayout);
    layout->addLayout(bottomLayout);
}

NavigationBar::~NavigationBar()
{
    if (topLayout)
        delete topLayout;
    if (bottomLayout)
        delete bottomLayout;
}

void NavigationBar::updateUi()
{
    //update top
    auto widgets = topLayout->findChildren<DToolButton *>();
    foreach (auto widget, widgets) {
        topLayout->removeWidget(widget);
    }
    foreach (auto btnList, topBtnsByPriority) {
        foreach (auto btn, btnList) {
            topLayout->addWidget(btn);
        }
    }

    //update bottom
    widgets = bottomLayout->findChildren<DToolButton *>();
    foreach (auto widget, widgets) {
        bottomLayout->removeWidget(widget);
    }
    foreach (auto btnList, bottomBtnsByPriority) {
        foreach (auto btn, btnList) {
            bottomLayout->addWidget(btn);
        }
    }
}

void NavigationBar::addNavItem(QAction *action, itemPositioin pos, quint8 priority)
{
    if (pos == top) {
        auto btn = createToolBtn(action, true);
        topBtnsByPriority[priority].append(btn);
    } else {
        auto btn = createToolBtn(action, false);
        bottomBtnsByPriority[priority].append(btn);
    }

    updateUi();
}

DToolButton *NavigationBar::createToolBtn(QAction *action, bool isNavigationItem)
{
    DToolButton *navBtn = new DToolButton(this);

    navBtn->setToolTip(action->text());
    navBtn->setIcon(action->icon());

    navBtn->setMinimumSize(QSize(48, 48));
    navBtn->setIconSize(QSize(20, 20));
    navBtn->setFocusPolicy(Qt::NoFocus);

    if (isNavigationItem) {
        navBtn->setCheckable(true);
        navBtn->setChecked(false);

        navBtns.insert(action->text(), navBtn);

        connect(navBtn, &DToolButton::clicked, this, [=]() {
            Controller::instance()->switchWidgetNavigation(action->text());
            setNavActionChecked(action->text(), true);
            action->trigger();
        });
        return navBtn;
    }

    connect(navBtn, &DToolButton::clicked, action, &QAction::trigger);

    return navBtn;
}

void NavigationBar::setNavActionChecked(const QString &actionName, bool checked)
{
    for (auto it = navBtns.begin(); it != navBtns.end(); it++) {
        it.value()->setChecked(false);
        if (it.key() == actionName) {
            it.value()->setChecked(checked);
        }
    }
}
