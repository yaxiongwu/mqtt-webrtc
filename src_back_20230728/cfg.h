#ifndef __cfg_h__
#define __cfg_h__

#include "mqtt_webrtc.h"


extern gsf_mqtt_webrtc_cfg_t mqtt_webrtc_cfg;
extern char mqtt_webrtc_parm_path[128];

int json_parm_load(char *filename, gsf_mqtt_webrtc_cfg_t *cfg);
int json_parm_save(char *filename, gsf_mqtt_webrtc_cfg_t *cfg);

#endif