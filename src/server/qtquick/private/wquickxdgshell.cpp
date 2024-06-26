// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgshell_p.h"
#include "wseat.h"
#include "woutput.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "wsurface.h"
#include "private/wglobal_p.h"

#include <qwxdgshell.h>
#include <qwseat.h>

#include <QCoreApplication>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
#include <wlr/util/edges.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class XdgShell : public WXdgShell
{
public:
    XdgShell(WQuickXdgShell *qq)
        : qq(qq) {}

    void surfaceAdded(WXdgSurface *surface) override;
    void surfaceRemoved(WXdgSurface *surface) override;

    WQuickXdgShell *qq;
};

class WQuickXdgShellPrivate : public WObjectPrivate, public WXdgShell
{
public:
    WQuickXdgShellPrivate(WQuickXdgShell *qq)
        : WObjectPrivate(qq)
        , WXdgShell()
    {
    }

    W_DECLARE_PUBLIC(WQuickXdgShell)

    XdgShell *xdgShell = nullptr;
};

void XdgShell::surfaceAdded(WXdgSurface *surface)
{
    WXdgShell::surfaceAdded(surface);
    Q_EMIT qq->surfaceAdded(surface);
}

void XdgShell::surfaceRemoved(WXdgSurface *surface)
{
    WXdgShell::surfaceRemoved(surface);
    Q_EMIT qq->surfaceRemoved(surface);
}

WQuickXdgShell::WQuickXdgShell(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickXdgShellPrivate(this), nullptr)
{

}

WServerInterface *WQuickXdgShell::create()
{
    W_D(WQuickXdgShell);

    d->xdgShell = server()->attach<XdgShell>(this);

    return d->xdgShell;
}

WXdgSurfaceItem::WXdgSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WXdgSurfaceItem::~WXdgSurfaceItem()
{

}

QPointF WXdgSurfaceItem::implicitPosition() const
{
    return m_implicitPosition;
}

QSize WXdgSurfaceItem::maximumSize() const
{
    return m_maximumSize;
}

QSize WXdgSurfaceItem::minimumSize() const
{
    return m_minimumSize;
}

inline static int32_t getValidSize(int32_t size, int32_t fallback) {
    return size > 0 ? size : fallback;
}

void WXdgSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();
    if (auto popup = xdgSurface()->handle()->toPopup()) {
        Q_UNUSED(popup);
        setImplicitPosition(xdgSurface()->getPopupPosition());
    } else if (auto toplevel = xdgSurface()->handle()->topToplevel()) {
        const QSize minSize(getValidSize(toplevel->handle()->current.min_width, 0),
                            getValidSize(toplevel->handle()->current.min_height, 0));
        const QSize maxSize(getValidSize(toplevel->handle()->current.max_width, INT_MAX),
                            getValidSize(toplevel->handle()->current.max_height, INT_MAX));

        if (m_minimumSize != minSize) {
            m_minimumSize = minSize;
            Q_EMIT minimumSizeChanged();
        }

        if (m_maximumSize != maxSize) {
            m_maximumSize = maxSize;
            Q_EMIT maximumSizeChanged();
        }
    }
}

void WXdgSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(xdgSurface());
    connect(xdgSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WXdgSurfaceItem::releaseResources);
}

QRectF WXdgSurfaceItem::getContentGeometry() const
{
    return xdgSurface()->getContentGeometry();
}

void WXdgSurfaceItem::setImplicitPosition(const QPointF &newImplicitPosition)
{
    if (m_implicitPosition == newImplicitPosition)
        return;
    m_implicitPosition = newImplicitPosition;
    Q_EMIT implicitPositionChanged();
}

WAYLIB_SERVER_END_NAMESPACE
