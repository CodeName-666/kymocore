/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial */
#include <string.h>
#include "kymo_trace.h"
#include "kymo_transport_mux.h"
static uint64_t clockRead(void *user){(void)user;return 0u;}
static uint8_t coreRead(void *user){(void)user;return 0u;}
static uint32_t contextRead(void *user){(void)user;return 1u;}
int main(void)
{
    KymoTraceConfig config;
    KymoTraceRecorder recorder;
    KymoTraceEvent event;
    uint8_t payload[36];
    int result = 0;
    memset(&config,0,sizeof(config));
    memset(&event,0,sizeof(event));
    config.clock_ticks=clockRead;config.core_id=coreRead;config.current_context=contextRead;
    if (KymoTrace_Init(&recorder, &config) == 0u) {
        result = 1;
    }
    (void)KymoTrace_EncodeBatch(0u,&event,1u,payload,sizeof(payload));
    (void)KymoTraceMux_Service((KymoTraceMux *)0);
    return result;
}




