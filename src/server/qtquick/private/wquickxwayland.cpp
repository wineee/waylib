// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxwayland_p.h"
#include "wquickwaylandserver.h"
#include "wseat.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"

#include <qwxwayland.h>
#include <qwxwaylandsurface.h>
#include <qwxwaylandshellv1.h>

#include <QQmlInfo>
#include <private/qquickitem_p.h>

extern "C" {
#define class className
#include <wlr/xwayland.h>
#include <wlr/xwayland/shell.h>
#undef class
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WXWaylandShellV1::WXWaylandShellV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

QWXWaylandShellV1 *WXWaylandShellV1::shell() const
{
    return m_shell;
}

WServerInterface *WXWaylandShellV1::create()
{
    m_shell = QWXWaylandShellV1::create(server()->handle(), 1);
    if (!m_shell)
        return nullptr;

    Q_EMIT shellChanged();
    return new WServerInterface(m_shell, m_shell->handle()->global);
}

class XWayland : public WXWayland
{
public:
    XWayland(WQuickXWayland *qq)
        : WXWayland(qq->compositor(), qq->lazy())
        , qq(qq)
    {

    }

    void surfaceAdded(WXWaylandSurface *surface) override;
    void surfaceRemoved(WXWaylandSurface *surface) override;
    void create(WServer *server) override;

    WQuickXWayland *qq;
};

void XWayland::surfaceAdded(WXWaylandSurface *surface)
{
    WXWayland::surfaceAdded(surface);

    surface->safeConnect(&WXWaylandSurface::isToplevelChanged,
                     qq, &WQuickXWayland::onIsToplevelChanged);
    surface->safeConnect(&QWXWaylandSurface::associate,
                     qq, [this, surface] {
        qq->addSurface(surface);
    });
    surface->safeConnect(&QWXWaylandSurface::dissociate,
                     qq, [this, surface] {
        qq->removeSurface(surface);
    });

    if (surface->surface())
        qq->addSurface(surface);
}

void XWayland::surfaceRemoved(WXWaylandSurface *surface)
{
    WXWayland::surfaceRemoved(surface);
    Q_EMIT qq->surfaceRemoved(surface);

    qq->removeToplevel(surface);
}

void XWayland::create(WServer *server)
{
    WXWayland::create(server);
    setSeat(qq->seat());
}

WQuickXWayland::WQuickXWayland(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

bool WQuickXWayland::lazy() const
{
    return m_lazy;
}

void WQuickXWayland::setLazy(bool newLazy)
{
    if (m_lazy == newLazy)
        return;

    if (xwayland && xwayland->isValid()) {
        qmlWarning(this) << "Can't change \"lazy\" after xwayland created";
        return;
    }

    m_lazy = newLazy;
    Q_EMIT lazyChanged();
}

QWCompositor *WQuickXWayland::compositor() const
{
    return m_compositor;
}

void WQuickXWayland::setCompositor(QWLRoots::QWCompositor *compositor)
{
    if (m_compositor == compositor)
        return;

    if (xwayland && xwayland->isValid()) {
        qmlWarning(this) << "Can't change \"compositor\" after xwayland created";
        return;
    }

    m_compositor = compositor;
    Q_EMIT compositorChanged();

    if (isPolished())
        tryCreateXWayland();
}

WClient *WQuickXWayland::client() const
{
    if (!xwayland || !xwayland->isValid())
        return nullptr;
    return xwayland->waylandClient();
}

pid_t WQuickXWayland::pid() const
{
    if (!xwayland || !xwayland->isValid())
        return 0;
    return xwayland->handle()->handle()->server->pid;
}

QByteArray WQuickXWayland::displayName() const
{
    if (!xwayland)
        return {};
    return xwayland->displayName();
}

WSeat *WQuickXWayland::seat() const
{
    return m_seat;
}

void WQuickXWayland::setSeat(WSeat *newSeat)
{
    if (m_seat == newSeat)
        return;
    m_seat = newSeat;

    if (xwayland && xwayland->isValid())
        xwayland->setSeat(m_seat);

    Q_EMIT seatChanged();
}

WServerInterface *WQuickXWayland::create()
{
    tryCreateXWayland();
    return xwayland;
}

void WQuickXWayland::tryCreateXWayland()
{
    if (!m_compositor)
        return;

    Q_ASSERT(!xwayland);

    xwayland = server()->attach<XWayland>(this);
    xwayland->safeConnect(&QWXWayland::ready, this, &WQuickXWayland::ready);

    Q_EMIT displayNameChanged();
}

void WQuickXWayland::onIsToplevelChanged()
{
    auto surface = qobject_cast<WXWaylandSurface*>(sender());
    Q_ASSERT(surface);

    if (!surface->surface())
        return;

    if (surface->isToplevel()) {
        addToplevel(surface);
    } else {
        removeToplevel(surface);
    }
}

void WQuickXWayland::addSurface(WXWaylandSurface *surface)
{
    Q_EMIT surfaceAdded(surface);

    if (surface->isToplevel())
        addToplevel(surface);
}

void WQuickXWayland::removeSurface(WXWaylandSurface *surface)
{
    Q_EMIT surfaceRemoved(surface);

    if (surface->isToplevel())
        removeToplevel(surface);
}

void WQuickXWayland::addToplevel(WXWaylandSurface *surface)
{
    if (toplevelSurfaces.contains(surface))
        return;
    toplevelSurfaces.append(surface);
    Q_EMIT toplevelAdded(surface);
}

void WQuickXWayland::removeToplevel(WXWaylandSurface *surface)
{
    if (toplevelSurfaces.removeOne(surface))
        Q_EMIT toplevelRemoved(surface);
}

WXWaylandSurfaceItem::WXWaylandSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WXWaylandSurfaceItem::~WXWaylandSurfaceItem()
{

}

bool WXWaylandSurfaceItem::setShellSurface(WToplevelSurface *surface)
{
    if (!WSurfaceItem::setShellSurface(surface))
        return false;

    if (surface) {
        Q_ASSERT(surface->surface());
        surface->safeConnect(&WXWaylandSurface::surfaceChanged, this, [this] {
            WSurfaceItem::setSurface(xwaylandSurface()->surface());
        });

        auto updateGeometry = [this] {
            const auto rm = resizeMode();
            if (rm != SizeFromSurface && m_positionMode != PositionFromSurface)
                return;
            if (!isVisible())
                return;

            updateSurfaceState();

            if (rm == SizeFromSurface) {
                resize(rm);
            }
            if (m_positionMode == PositionFromSurface) {
                doMove(m_positionMode);
            }
        };

        xwaylandSurface()->safeConnect(&WXWaylandSurface::requestConfigure,
                this, [updateGeometry, this] {
            if (m_ignoreConfigureRequest)
                return;
            const QRect geometry(expectSurfacePosition(m_positionMode),
                                 expectSurfaceSize(resizeMode()));
            configureSurface(geometry);
            updateGeometry();
        });
        xwaylandSurface()->safeConnect(&WXWaylandSurface::geometryChanged, this, updateGeometry);
        connect(this, &WXWaylandSurfaceItem::topPaddingChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        connect(this, &WXWaylandSurfaceItem::leftPaddingChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        // TODO: Maybe we shouldn't think about the effectiveVisible for surface/item's position
        // This behovior can control by compositor using PositionMode::ManualPosition
        connect(this, &WXWaylandSurfaceItem::effectiveVisibleChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
    }
    return true;
}

WXWaylandSurfaceItem *WXWaylandSurfaceItem::parentSurfaceItem() const
{
    return m_parentSurfaceItem;
}

void WXWaylandSurfaceItem::setParentSurfaceItem(WXWaylandSurfaceItem *newParentSurfaceItem)
{
    if (m_parentSurfaceItem == newParentSurfaceItem)
        return;
    if (m_parentSurfaceItem) {
        m_parentSurfaceItem->disconnect(this);
    }

    m_parentSurfaceItem = newParentSurfaceItem;
    Q_EMIT parentSurfaceItemChanged();

    if (m_parentSurfaceItem)
        connect(m_parentSurfaceItem, &WSurfaceItem::surfaceSizeRatioChanged, this, &WXWaylandSurfaceItem::updatePosition);
    checkMove(m_positionMode);
}

QSize WXWaylandSurfaceItem::maximumSize() const
{
    return m_maximumSize;
}

QSize WXWaylandSurfaceItem::minimumSize() const
{
    return m_minimumSize;
}

WXWaylandSurfaceItem::PositionMode WXWaylandSurfaceItem::positionMode() const
{
    return m_positionMode;
}

void WXWaylandSurfaceItem::setPositionMode(PositionMode newPositionMode)
{
    if (m_positionMode == newPositionMode)
        return;
    m_positionMode = newPositionMode;
    Q_EMIT positionModeChanged();
}

void WXWaylandSurfaceItem::move(PositionMode mode)
{
    if (mode == ManualPosition) {
        qmlWarning(this) << "Can't move WXWaylandSurfaceItem for ManualPosition mode.";
        return;
    }

    if (!isVisible())
        return;

    doMove(mode);
}

QPointF WXWaylandSurfaceItem::positionOffset() const
{
    return m_positionOffset;
}

void WXWaylandSurfaceItem::setPositionOffset(QPointF newPositionOffset)
{
    if (m_positionOffset == newPositionOffset)
        return;
    m_positionOffset = newPositionOffset;
    Q_EMIT positionOffsetChanged();

    updatePosition();
}

bool WXWaylandSurfaceItem::ignoreConfigureRequest() const
{
    return m_ignoreConfigureRequest;
}

void WXWaylandSurfaceItem::setIgnoreConfigureRequest(bool newIgnoreConfigureRequest)
{
    if (m_ignoreConfigureRequest == newIgnoreConfigureRequest)
        return;
    m_ignoreConfigureRequest = newIgnoreConfigureRequest;
    Q_EMIT ignoreConfigureRequestChanged();
}

void WXWaylandSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();

    QSize minSize = xwaylandSurface()->minSize();
    if (!minSize.isValid())
        minSize = QSize(0, 0);

    QSize maxSize = xwaylandSurface()->maxSize();
    if (maxSize.isValid())
        maxSize = QSize(INT_MAX, INT_MAX);

    if (m_minimumSize != minSize) {
        m_minimumSize = minSize;
        Q_EMIT minimumSizeChanged();
    }

    if (m_maximumSize != maxSize) {
        m_maximumSize = maxSize;
        Q_EMIT maximumSizeChanged();
    }
}

void WXWaylandSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(xwaylandSurface());
    connect(xwaylandSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WXWaylandSurfaceItem::releaseResources);
    updatePosition();
}

bool WXWaylandSurfaceItem::doResizeSurface(const QSize &newSize)
{
    configureSurface(QRect(expectSurfacePosition(m_positionMode), newSize));
    return true;
}

QRectF WXWaylandSurfaceItem::getContentGeometry() const
{
    return xwaylandSurface()->getContentGeometry();
}

QSizeF WXWaylandSurfaceItem::getContentSize() const
{
    return (size() - QSizeF(leftPadding() + rightPadding(), topPadding() + bottomPadding())) * surfaceSizeRatio();
}

void WXWaylandSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    WSurfaceItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.topLeft() != oldGeometry.topLeft()
        && m_positionMode == PositionToSurface && xwaylandSurface()) {
        configureSurface(QRect(expectSurfacePosition(m_positionMode),
                               expectSurfaceSize(resizeMode())));
    }
}

void WXWaylandSurfaceItem::doMove(PositionMode mode)
{
    Q_ASSERT(mode != ManualPosition);
    Q_ASSERT(isVisible());

    if (mode == PositionFromSurface) {
        const QPoint epos = expectSurfacePosition(mode);
        QPointF pos = epos;

        const qreal ssr = m_parentSurfaceItem ? m_parentSurfaceItem->surfaceSizeRatio() : 1.0;
        const auto pt = parentItem();
        if (pt && !qFuzzyCompare(ssr, 1.0)) {
            const QPoint pepos = m_parentSurfaceItem->expectSurfacePosition(mode);
            pos = pepos + (epos - pepos) / ssr;
        }

        setPosition(pos - m_positionOffset);
    } else if (mode == PositionToSurface) {
        configureSurface(QRect(expectSurfacePosition(mode), expectSurfaceSize(resizeMode())));
    }
}

void WXWaylandSurfaceItem::updatePosition()
{
    checkMove(m_positionMode);
}

void WXWaylandSurfaceItem::configureSurface(const QRect &newGeometry)
{
    if (!isVisible())
        return;
    xwaylandSurface()->configure(newGeometry);
    updateSurfaceState();
}

QPoint WXWaylandSurfaceItem::expectSurfacePosition(PositionMode mode) const
{
    if (mode == PositionFromSurface) {
        const bool useRequestPositon = !xwaylandSurface()->isBypassManager()
                                       && xwaylandSurface()->requestConfigureFlags()
                                              .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_POSITION);
        return useRequestPositon
                   ? xwaylandSurface()->requestConfigureGeometry().topLeft()
                   : xwaylandSurface()->geometry().topLeft();
    } else if (mode == PositionToSurface) {
        QPointF pos = position();
        const qreal ssr = m_parentSurfaceItem ? m_parentSurfaceItem->surfaceSizeRatio() : 1.0;
        const auto pt = parentItem();
        if (pt && !qFuzzyCompare(ssr, 1.0)) {
            const QPointF poffset(m_parentSurfaceItem->leftPadding(), m_parentSurfaceItem->topPadding());
            pos = pt->mapToItem(m_parentSurfaceItem, pos) - poffset;
            pos = pt->mapFromItem(m_parentSurfaceItem, pos * ssr + poffset);
        }

        return (pos + m_positionOffset + QPointF(leftPadding(), topPadding())).toPoint();
    }

    return xwaylandSurface()->geometry().topLeft();
}

QSize WXWaylandSurfaceItem::expectSurfaceSize(ResizeMode mode) const
{
    if (mode == SizeFromSurface) {
        const bool useRequestSize = !xwaylandSurface()->isBypassManager()
                                    && xwaylandSurface()->requestConfigureFlags()
                                           .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_SIZE);
        return useRequestSize
                   ? xwaylandSurface()->requestConfigureGeometry().size()
                   : xwaylandSurface()->geometry().size();
    } else if (mode == SizeToSurface) {
        return WXWaylandSurfaceItem::getContentSize().toSize();
    }

    return xwaylandSurface()->geometry().size();
}

WAYLIB_SERVER_END_NAMESPACE
