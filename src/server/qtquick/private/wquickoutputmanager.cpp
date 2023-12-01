// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickoutputmanager_p.h"
#include "wtypes.h"
#include "woutput.h"
#include <qwoutputmanagementv1.h>
#include <qwoutput.h>

extern "C" {
#define static
#include <wlr/types/wlr_output_management_v1.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWOutputManagerV1;
using QW_NAMESPACE::QWOutputConfigurationV1;
using QW_NAMESPACE::QWOutputConfigurationHeadV1;

class WQuickOutputManagerPrivate : public WObjectPrivate
{
public:
    WQuickOutputManagerPrivate(WQuickOutputManager *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickOutputManager)

    void outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int test);

    QWOutputManagerV1 *manager { nullptr };
    QPointer<WBackend> backend;
};

WQuickOutputManager::WQuickOutputManager(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickOutputManagerPrivate(this), nullptr)
{

}

void WQuickOutputManager::create()
{
    W_D(WQuickOutputManager);
    WQuickWaylandServerInterface::create();

    d->manager = QWOutputManagerV1::create(server()->handle());
    connect(d->manager, &QWOutputManagerV1::test, this, [d](QWOutputConfigurationV1 *config) {
        qDebug() << "Output Manager V1 test !!!";
        d->outputMgrApplyOrTest(config, true);
    });

    connect(d->manager, &QWOutputManagerV1::apply, this, [d](QWOutputConfigurationV1 *config) {
        qDebug() << "Output Manager V1 apply !!!";
        d->outputMgrApplyOrTest(config, false);
    });
}

static constexpr WOutput::Transform wlToWOutputTransform(wl_output_transform transform)
{
    switch (transform) {
    using enum WOutput::Transform;
    case WL_OUTPUT_TRANSFORM_NORMAL:
        return Normal;
    case WL_OUTPUT_TRANSFORM_90:
        return R90;
    case WL_OUTPUT_TRANSFORM_180:
        return R180;
    case WL_OUTPUT_TRANSFORM_270:
        return R270;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
        return Flipped;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        return Flipped90;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        return Flipped180;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        return Flipped270;
    default:
        Q_UNREACHABLE();
        break;
    }
}

void WQuickOutputManagerPrivate::outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int onlyTest)
{
    W_Q(WQuickOutputManager);
    wlr_output_configuration_head_v1 *config_head;
    int ok = 1;

    wl_list_for_each(config_head, &config->handle()->heads, link) {
        auto *output = QW_NAMESPACE::QWOutput::from(config_head->state.output);
        auto *woutput = WOutput::fromHandle(output);

        const auto &state = config_head->state;

        output->enable(config_head->state.enabled);
        if (config_head->state.enabled) {
            if (config_head->state.mode)
                output->setMode(config_head->state.mode);
            else
                output->setCustomMode({ config_head->state.custom_mode.width,
                                       config_head->state.custom_mode.height },
                                      config_head->state.custom_mode.refresh);
            output->enableAdaptiveSync(state.adaptive_sync_enabled);

            if (!onlyTest) {
                // assume transform/scale/position setting always success
                Q_EMIT woutput->requestTransform(wlToWOutputTransform(state.transform));
                Q_EMIT woutput->requestScale(state.scale);
                Q_EMIT woutput->requestOutputPosition(state.x, state.y);
            }
        }

        if (onlyTest) {
            ok &= output->test();
            output->rollback();
        } else {
            ok &= output->commit();
        }
    }

    if (ok)
        config->sendSucceeded();
    else
        config->sendFailed();
    delete config;

    if (!onlyTest) {
        q->updateConfig();
    }
}

void WQuickOutputManager::updateConfig() {
    W_D(WQuickOutputManager);
    if (!d->backend)
        return;

    auto *config = QWOutputConfigurationV1::create();
    qDebug() << "WQuickOutputManager::updateConfig";

    for (const WOutput *output : d->backend->outputList()) {
        auto *configHead = QWOutputConfigurationHeadV1::create(config, output->handle());
        //configHead->handle()->state.scale = output->scale();
        //configHead->handle()->state.transform = output->orientation()
        qDebug() << "232erwe:" << output->orientation();
        configHead->handle()->state.x = output->x();
        configHead->handle()->state.y = output->y();
    }

    d->manager->setConfiguration(config);
}

WBackend *WQuickOutputManager::backend() const
{
    W_DC(WQuickOutputManager);
    return d->backend;
}

void WQuickOutputManager::setBackend(WBackend *backend)
{
    W_D(WQuickOutputManager);
    if (d->backend == backend)
        return;
    d->backend = backend;
    updateConfig();
}

WAYLIB_SERVER_END_NAMESPACE
