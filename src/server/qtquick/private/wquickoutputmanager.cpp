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

    QWOutputManagerV1 *manager = nullptr;
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

void WQuickOutputManagerPrivate::outputMgrApplyOrTest(QWOutputConfigurationV1 *config, int test)
{
    /*
     * Called when a client such as wlr-randr requests a change in output
     * configuration. This is only one way that the layout can be changed,
     * so any Monitor information should be updated by updatemons() after an
     * output_layout.change event, not here.
     */
    wlr_output_configuration_head_v1 *config_head;
    int ok = 1;

    wl_list_for_each(config_head, &config->handle()->heads, link) {
        auto *output = QW_NAMESPACE::QWOutput::from(config_head->state.output);
        auto *woutput = WOutput::fromHandle(output);

        //output->enable(config_head->state.enabled);


        const auto &state = config_head->state;
        WOutputStateEvent event;
        event.m_accept = false;
        event.m_transform = wlToWOutputTransform(state.transform);
        qDebug() << event.m_accept;
        Q_EMIT woutput->requestOutputStateApply(event);
        qDebug() << "DDDDDDDDDDDDDDDDDDD: " << event.m_accept;

        /*if (config_head->state.enabled) {
            Q_EMIT woutput->requestTransform(wlToWOutputTransform(config_head->state.transform));
            Q_EMIT woutput->requestScale(config_head->state.scale);
            // if (config_head->state.mode)
            //     output->setMode(config_head->state.mode);
            // else
            //     output->setCustomMode({ config_head->state.custom_mode.width,
            //                            config_head->state.custom_mode.height },
            //                           config_head->state.custom_mode.refresh);
            Q_EMIT woutput->requestAdaptiveSyncEnabled(config_head->state.adaptive_sync_enabled);
        }*/

        if (test) {
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
    /* TODO: use a wrapper function? */
    //updatemons(NULL, NULL);
}

void WQuickOutputManager::updateConfig(WBackend *backend) {
    W_D(WQuickOutputManager);

    auto *config = QWOutputConfigurationV1::create();
    qDebug() << "WQuickOutputManager::updateConfig";
    for (const WOutput *output : backend->outputList()) {
//        if (output == root->fallback_output) {
//            continue;
//        }
        qDebug() << "WOutput" <<  output;
        auto *config_head = QWOutputConfigurationHeadV1::create(config, output->handle());
        //struct wlr_box output_box;
        //wlr_output_layout_get_box(root->output_layout,
        //                          output->wlr_output, &output_box);
        // We mark the output enabled when it's switched off but not disabled
        config_head->handle()->state.enabled = true; //!wlr_box_empty(&output_box);
        config_head->handle()->state.x = 0;
        config_head->handle()->state.y = 0;
    }

    d->manager->setConfiguration(config);
}

WAYLIB_SERVER_END_NAMESPACE
