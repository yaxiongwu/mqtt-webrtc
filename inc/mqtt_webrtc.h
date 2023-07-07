#ifndef __mqtt_webrtc_h__
#define  __mqtt_webrtc_h__

#ifdef __cplusplus
extern "C" {
#endif


#include "inc/gsf.h"

//for json cfg;
#include "mod/mqtt_webrtc/inc/sjb_mqtt_webrtc.h"


#define GSF_IPC_MQTT_WEBRTC "ipc:///tmp/mqtt_webrtc_rep"

enum {
  GSF_ID_MQTT_WEBRTC_CFG = 1,   // gsf_webs_cfg_t;
  GSF_ID_MQTT_WEBRTC_USER= 2,   // gsf_user_t[N];
  GSF_ID_MQTT_WEBRTC_END
};

enum {
  GSF_MQTT_WEBRTC_ERR = -1,
};


#ifdef __cplusplus
}
#endif

#endif