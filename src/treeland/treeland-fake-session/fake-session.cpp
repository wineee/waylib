// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fake-session.h"

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QWindow>
#include <QTimer>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

ForeignToplevelManager::ForeignToplevelManager()
    : QWaylandClientExtensionTemplate<ForeignToplevelManager>(1)
{

}

void ForeignToplevelManager::ztreeland_foreign_toplevel_manager_v1_toplevel(struct ::ztreeland_foreign_toplevel_handle_v1 *toplevel)
{
    ForeignToplevelHandle* handle = new ForeignToplevelHandle(toplevel);
    emit newForeignToplevelHandle(handle);
}

ForeignToplevelHandle::ForeignToplevelHandle(struct ::ztreeland_foreign_toplevel_handle_v1 *object)
    : QWaylandClientExtensionTemplate<ForeignToplevelHandle>(1)
    , QtWayland::ztreeland_foreign_toplevel_handle_v1(object)
{}

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_app_id_changed(const QString &app_id)
{
}

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_pid_changed(uint32_t pid)
{
    emit pidChanged(pid);
}

ShortcutManager::ShortcutManager()
    : QWaylandClientExtensionTemplate<ShortcutManager>(1)
{

}

ShortcutContext::ShortcutContext(struct ::ztreeland_shortcut_context_v1 *object)
    : QWaylandClientExtensionTemplate<ShortcutContext>(1)
    , QtWayland::ztreeland_shortcut_context_v1(object)
{

}

void ShortcutContext::ztreeland_shortcut_context_v1_shortcut(uint32_t keycode, uint32_t modify)
{
    qDebug() << Q_FUNC_INFO << keycode << modify;
    emit shortcutHappended(keycode, modify);
}

FakeSession::FakeSession(int argc, char* argv[])
    : QGuiApplication(argc, argv)
    , m_shortcutManager(new ShortcutManager)
    , m_toplevelManager(new ForeignToplevelManager)
{
    connect(m_shortcutManager, &ShortcutManager::activeChanged, this, [=] {
        qDebug() << m_shortcutManager->isActive();
        if (m_shortcutManager->isActive()) {

            ShortcutContext* context = new ShortcutContext(m_shortcutManager->get_shortcut_context());
            connect(context, &ShortcutContext::shortcutHappended, this, [=](uint32_t keycode, uint32_t modify) {
                auto keyEnum = static_cast<Qt::Key>(keycode);
                auto modifyEnum = static_cast<Qt::KeyboardModifiers>(modify);
                qDebug() << keyEnum << modifyEnum;
                if (keyEnum == Qt::Key_Super_L && modifyEnum == Qt::NoModifier) {
                    static QProcess process;
                    if (process.state() == QProcess::ProcessState::Running) {
                        process.kill();
                    }
                    process.start("dofi", {"-S", "run"});
                    return;
                }
                if (keyEnum == Qt::Key_T && modifyEnum.testFlags(Qt::ControlModifier | Qt::AltModifier)) {
                    QProcess::startDetached("x-terminal-emulator");
                    return;
                }
            });
        }
    });

    connect(m_toplevelManager, &ForeignToplevelManager::newForeignToplevelHandle, this, [=](ForeignToplevelHandle *handle) {
        connect(handle, &ForeignToplevelHandle::pidChanged, this, [=](uint32_t pid) {
            qDebug() << "toplevel pid: " << pid;
        });
    });

    emit m_shortcutManager->activeChanged();
}

int main (int argc, char *argv[]) {
    FakeSession helper(argc, argv);

    return helper.exec();
}