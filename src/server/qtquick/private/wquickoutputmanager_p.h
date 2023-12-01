// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wbackend.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickOutputManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickOutputManager: public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickOutputManager)
    Q_PROPERTY(WBackend *waylandBackend READ backend WRITE setBackend)
    QML_NAMED_ELEMENT(OutputManager)
public:
    explicit WQuickOutputManager(QObject *parent = nullptr);

    Q_INVOKABLE void updateConfig();

    WBackend *backend() const;
    void setBackend(WBackend *backend);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
