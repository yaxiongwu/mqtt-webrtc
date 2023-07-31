#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "inc/gsf.h"
#include "mod/bsp/inc/bsp.h"
#include "fw/comm/inc/proc.h"

#include "mqtt_webrtc.h"

#include "mongoose.h"
#include "webrtc.h"
#include "mod_call.h"
#include "cfg.h"

GSF_LOG_GLOBAL_INIT("MQTT_WEBRTC", 8 * 8 * 1024);

static pthread_t _pid_mqtt;
// static struct mg_mgr *_mgr_mqtt;

typedef struct
{
  int stat;
  int timeout;
  int lastsec;
  char url[256];
  struct mg_connection *nc;
  struct list_head clients;
} mqtt_conn_t;

typedef struct
{
  rtc_sess_t *rtc_sess; // fisrt field;
  struct list_head list;
} mqtt_client_t;

struct source_info_t
{
  char *id;
  char *localtion;
  char *categorize;
  char *username;
  char *label;
};

static mqtt_conn_t _conn_mqtt;

static void *_mqtt_task(void *parm);

// static const char *s_url = "mqtt://www.bxzryd.cn:1883";
static const char *s_url = "mqtt://120.78.200.246:1883";
static const char *s_sub_topic = "mg/+/test";    // Publish topic
static const char *s_pub_topic = "mg/clnt/test"; // Subscribe topic
static int s_qos = 2;                            // MQTT QoS
static struct mg_connection *s_conn;             // Client connection

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo) { s_signo = signo; }
static const char *sourceId = "source12345";
static const char *topic_sub_source_id_pre = "source/"; // + SourceId;
static const char *topic_pub_source_reg = "server/reg";
static const char *topic_pub_source_query = "server/query";
static const char *topic_sub_candidate_pre = "cand/"; // + SourceId;
static const char *topic_pub_candidate_pre = "cand/";
static const char *topic_sub_offer_pre = "offer/"; // + SourceId;
static const char *topic_pub_offer_pre = "offer/";
static const char *topic_sub_answer_pre = "answer/"; // + SourceId;
static const char *topic_pub_answer_pre = "answer/";
static const char *topic_sub_close_pre = "close/";                             //+cientID
static const char *topic_sub_want_connect_request_pre = "wantConnectRequest/"; // + SourceId;
static const char *topic_pub_want_connect_reply_pre = "wantConnectReply/";
char topic_sub_source_id[50];
char topic_sub_want_connect_request[50];
char topic_sub_answer[50];
char topic_sub_close[50]; //+sourceID
char topic_sub_candidate[50];
char topic_pub_offer[50];
char topic_pub_candidate[50];
struct mg_mqtt_opts sub_opts;

char clientId[20];
struct mg_mqtt_opts pub_opts;

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  rtc_sess_t *rtc_sess;
  if (ev == MG_EV_OPEN)
  {
    MG_INFO(("%lu CREATED", c->id));
    // c->is_hexdumping = 1;
  }
  else if (ev == MG_EV_ERROR)
  {
    // On error, log error message
    MG_ERROR(("%lu ERROR %s", c->id, (char *)ev_data));
  }
  else if (ev == MG_EV_CONNECT)
  {
    // If target URL is SSL/TLS, command client connection to use TLS
    if (mg_url_is_ssl(s_url))
    {
      struct mg_tls_opts opts = {.ca = "ca.pem"};
      mg_tls_init(c, &opts);
    }
  }
  else if (ev == MG_EV_MQTT_OPEN)
  {
    // MQTT connect is successful
    struct mg_str subt = mg_str(""); 

    MG_INFO(("%lu CONNECTED to %s", c->id, s_url));
    // sub source id
    // topic_sub_source_id = (char *)malloc(strlen(topic_sub_source_id_pre) + strlen(sourceId));
    sprintf(topic_sub_source_id, "%s%s", topic_sub_source_id_pre, sourceId);
    memset(&sub_opts, 0, sizeof(sub_opts));
    subt = mg_str(topic_sub_source_id);
    sub_opts.topic = subt; // topic_sub_source_id;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("%lu SUBSCRIBED to %.*s", c->id, (int)subt.len, subt.ptr));

    // sub candidate
    // topic_sub_candidate = (char *)malloc(strlen(topic_sub_candidate_pre) + strlen(sourceId));
    sprintf(topic_sub_candidate, "%s%s", topic_sub_candidate_pre, sourceId);
    memset(&sub_opts, 0, sizeof(sub_opts));
    subt = mg_str(topic_sub_candidate);
    sub_opts.topic = subt; // topic_sub_source_id;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("%lu SUBSCRIBED to %.*s", c->id, (int)subt.len, subt.ptr));

    // sub answer
    /// topic_sub_answer = (char *)malloc(strlen(topic_sub_answer_pre) + strlen(sourceId));
    sprintf(topic_sub_answer, "%s%s", topic_sub_answer_pre, sourceId);
    memset(&sub_opts, 0, sizeof(sub_opts));
    subt = mg_str(topic_sub_answer);
    sub_opts.topic = subt; // topic_sub_source_id;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("%lu SUBSCRIBED to %.*s", c->id, (int)subt.len, subt.ptr));

    // sub want connect
    // topic_sub_want_connect_request = (char *)malloc(strlen(topic_sub_want_connect_request_pre) + strlen(sourceId));
    sprintf(topic_sub_want_connect_request, "%s%s", topic_sub_want_connect_request_pre, sourceId);
    memset(&sub_opts, 0, sizeof(sub_opts));
    subt = mg_str(topic_sub_want_connect_request);
    sub_opts.topic = subt; // topic_sub_source_id;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("%lu SUBSCRIBED to %.*s", c->id, (int)subt.len, subt.ptr));

    // struct mg_mqtt_opts pub_opts;
    memset(&pub_opts, 0, sizeof(pub_opts));
    struct source_info_t sourceInfo;
    sourceInfo.id = sourceId;
    sourceInfo.localtion = "guang dong sheng ";
    sourceInfo.categorize = "car";
    sourceInfo.label = "hi3516";
    char *pubSourceInfo = "{\"id\":\"source12345\",\"localtion\":\"guang dong\",\"categorize\":\"car\",\"label\":\"hi3516\"}";
    struct mg_str pubt = mg_str(topic_pub_source_reg), data = mg_str(pubSourceInfo);
    pub_opts.topic = pubt;
    pub_opts.message = data;
    pub_opts.qos = s_qos, pub_opts.retain = false;
    mg_mqtt_pub(c, &pub_opts);
    MG_INFO(("%lu PUBLISHED %.*s -> %.*s", c->id, (int)data.len, data.ptr,
             (int)pubt.len, pubt.ptr));
  }
  else if (ev == MG_EV_MQTT_MSG)
  {
    // When we get echo response, print it
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *)ev_data;
    MG_INFO(("%lu RECEIVED %.*s <- %.*s", c->id, (int)mm->data.len,
             mm->data.ptr, (int)mm->topic.len, mm->topic.ptr));

    if (strncmp(mm->topic.ptr, topic_sub_want_connect_request, (int)mm->topic.len) == 0)
    {
      MG_INFO(("%lu topic_sub_want_connect_request RECEIVED %.*s <- %.*s", c->id, (int)mm->data.len,
               mm->data.ptr, (int)mm->topic.len, mm->topic.ptr));
      strcpy(clientId, mm->data.ptr);

      int new_flag = 0;
      // new session;

      rtc_sess = rtc_sess_new();
      if (rtc_sess)
      {
        (rtc_sess)->mgr = c->mgr;
        // char *dst = strstr(sdp_json, ",\"src\":\"");
        // sscanf(dst, ",\"src\":\"%127[^\"]", rtc_sess->dst);
        strcpy(rtc_sess->dst, mm->data.ptr);
        printf("rtc_sess_new , src:[%s]\n", rtc_sess->dst);
      }

      if (!(rtc_sess))
      {
        printf("wuyaxiong,rtc_sess_new , conncet too much\n\n");
        // char dst[128] = {0};
        // char *pdst = strstr(sdp_json, ",\"src\":\"");
        // sscanf(pdst, ",\"src\":\"%127[^\"]", dst);

        // sprintf(sdp_json, "{\"type\":\"close\",\"dst\":\"%s\",\"msg\":\"%s\"}", dst, "conncet too much.");
        // mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, sdp_json, strlen(sdp_json));
        return 0;
      }

      // char *topic_pub_offer = (char *)malloc(strlen(topic_pub_offer_pre) + strlen(clientId));
      sprintf(topic_pub_offer, "%s%s", topic_pub_offer_pre, clientId);
      char sdp_json[4096] = {0};
      unsigned int sdp_json_len = sizeof(sdp_json);

      strcpy(rtc_sess->dst, mm->data.ptr);
      sdp_json[0] = '\0';
      rtc_createOffer((rtc_sess), sdp_json, sdp_json_len);
      memset(&pub_opts, 0, sizeof(pub_opts));
      // char *pubSourceInfo = "{\"id\":\"source12345\",\"localtion\":\"guang dong\",\"categorize\":\"car\",\"label\":\"hi3516\"}";
      struct mg_str pubt = mg_str(topic_pub_offer), data = mg_str(sdp_json);
      pub_opts.topic = pubt;
      pub_opts.message = data;
      pub_opts.qos = s_qos, pub_opts.retain = false;
      mg_mqtt_pub(c, &pub_opts);

      sprintf(topic_sub_close, "%s%s", topic_sub_close_pre, sourceId);

      memset(&sub_opts, 0, sizeof(sub_opts));
      
      sub_opts.topic =  mg_str(topic_sub_close); // topic_sub_source_id;
      sub_opts.qos = s_qos;
      mg_mqtt_sub(c, &sub_opts);
      //MG_INFO(("%lu SUBSCRIBED to %.*s", c->id, (int)subt.len, subt.ptr));
    }
    else if (strncmp(mm->topic.ptr, topic_sub_answer, (int)mm->topic.len) == 0)
    {
      char sdp_json[2048] = {0};

      memcpy(sdp_json, mm->data.ptr, mm->data.len);
      // printf("recv answe:[%s]\n\n", mm->data.ptr);

      rtc_setAnswer(rtc_sess, sdp_json);
    }
    else if (strncmp(mm->topic.ptr, topic_sub_candidate, (int)mm->topic.len) == 0)
    {
      // char *topic_pub_candidate = (char *)malloc(strlen(topic_pub_candidate_pre) + strlen(clientId));
      sprintf(topic_pub_candidate, "%s%s", topic_pub_candidate_pre, clientId);
      struct mg_str pubt = mg_str(topic_pub_candidate), data; //= mg_str(sdp_json);
      do
      {
        char *local_ice = rtc_getCandidate(rtc_sess);
        if (!local_ice)
          break;
        {
          // dst = src;
          data = mg_str(local_ice);
          pub_opts.topic = pubt;
          pub_opts.message = data;
          pub_opts.qos = s_qos, pub_opts.retain = false;
          mg_mqtt_pub(c, &pub_opts);
        }
      } while (1);

      char ice_json[512] = {0};
      // char *ice_json = (char *)malloc(512);
      //  strcpy(ice_json, mm->data.ptr);
      memcpy(ice_json, mm->data.ptr, (int)mm->data.len);
      // printf("recv candidate:[%s],len:%d\n\n", ice_json,(int)mm->data.len);
      rtc_setCandidate(rtc_sess, ice_json);
      // free(ice_json);
    }
    else if (strncmp(mm->topic.ptr, topic_sub_close, (int)mm->topic.len) == 0)
    {
         char closeClientId[12];
         strcpy(closeClientId, mm->data.ptr);
         MG_INFO(("%s CLOSED", closeClientId));
          //free list;
          mqtt_client_t *n, *nn;
          list_for_each_entry_safe(n, nn, &conn->clients, list)
          {
              list_del(&n->list);
              if(n->rtc_sess)
                rtc_sess_free(n->rtc_sess);
              free(n);
          }
    }
  }
  else if (ev == MG_EV_CLOSE)
  {
    MG_INFO(("%lu CLOSED", c->id));
    s_conn = NULL; // Mark that we're closed
  }
  (void)fn_data;
}

// int webrtc_proc(struct mg_connection *nc, rtc_sess_t **rtc_sess, struct websocket_message *wm)
// {
//   int ret = 0;

//   if (!strncmp(wm->data, "{\"type\":\"hello", strlen("{\"type\":\"hello")))
//   {
//     char sdp_json[4096] = {0};
//     unsigned int sdp_json_len = sizeof(sdp_json);
//     memcpy(sdp_json, wm->data, wm->size);

//     int new_flag = 0;

//     // new session;
//     if (!(*rtc_sess))
//     {
//       *rtc_sess = rtc_sess_new();
//       if (*rtc_sess)
//       {
//         (*rtc_sess)->mgr = nc->mgr;
//         char *dst = strstr(sdp_json, ",\"src\":\"");
//         sscanf(dst, ",\"src\":\"%127[^\"]", (*rtc_sess)->dst);
//         warn("rtc_sess_new nc:%p, (*rtc_sess):%p, src:[%s]\n", nc, *rtc_sess, (*rtc_sess)->dst);
//         ret = WEBRTC_NEW;
//       }
//     }

//     if (!(*rtc_sess))
//     {
//       char dst[128] = {0};
//       char *pdst = strstr(sdp_json, ",\"src\":\"");
//       sscanf(pdst, ",\"src\":\"%127[^\"]", dst);

//       sprintf(sdp_json, "{\"type\":\"close\",\"dst\":\"%s\",\"msg\":\"%s\"}", dst, "conncet too much.");
//       mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, sdp_json, strlen(sdp_json));
//       return 0;
//     }

//     sdp_json[0] = '\0';
//     rtc_createOffer((*rtc_sess), sdp_json, sdp_json_len);

//     // dst = src;
//     sdp_json[strlen(sdp_json) - 1] = ',';
//     char dst_token[128];
//     sprintf(dst_token, "\"dst\":\"%s\"}", (*rtc_sess)->dst);
//     strcat(sdp_json, dst_token);

//     printf("send createOffer:[%s]\n\n", sdp_json);
//     mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, sdp_json, strlen(sdp_json));

//     return ret;
//   }
//   else if (!strncmp(wm->data, "{\"type\":\"leave", strlen("{\"type\":\"leave")))
//   {
//     printf("LEAVE [%.*s].\n", wm->size, wm->data);
//     return ret = WEBRTC_DEL;
//   }
//   else if (!strncmp(wm->data, "{\"type\":\"answer", strlen("{\"type\":\"answer")))
//   {
//     char sdp_json[4096] = {0};

//     memcpy(sdp_json, wm->data, wm->size);
//     printf("recv createAnswe:[%s]\n\n", sdp_json);

//     rtc_setAnswer((*rtc_sess), sdp_json);
//   }
//   else if (!strncmp(wm->data, "{\"candidate\":\"candidate", strlen("{\"candidate\":\"candidate")))
//   {
//     do
//     {
//       char *local_ice = rtc_getCandidate((*rtc_sess));
//       if (!local_ice)
//         break;
//       {
//         // dst = src;
//         local_ice[strlen(local_ice) - 1] = ',';
//         char dst_token[128];
//         sprintf(dst_token, "\"dst\":\"%s\"}", (*rtc_sess)->dst);
//         strcat(local_ice, dst_token);

//         warn("send candidate:[%s]\n\n", local_ice);
//         mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, local_ice, strlen(local_ice));
//       }
//     } while (1);

//     char ice_json[4096] = {0};
//     memcpy(ice_json, wm->data, wm->size);
//     printf("recv candidate:[%s]\n\n", ice_json);
//     rtc_setCandidate((*rtc_sess), ice_json);
//   }

//   return ret;
// }

// Timer function - recreate client connection if it is closed
static void timer_fn(void *arg)
{
  struct mg_mgr *mgr = (struct mg_mgr *)arg;
  struct mg_mqtt_opts opts = {.clean = true,
                              .user = mg_str("wuyaxiong"),
                              .pass = mg_str("wuyaxiong1982"),
                              .qos = s_qos,
                              .topic = mg_str(s_pub_topic),
                              .version = 4,
                              .message = mg_str("bye")};
  if (s_conn == NULL)
    s_conn = mg_mqtt_connect(mgr, s_url, &opts, fn, NULL);
}

int mqtt_init()
{
  info("mqtt_init 3\n");
  rtc_init();
  if (pthread_create(&_pid_mqtt, NULL, _mqtt_task, NULL) != 0)
  {
    info("create ws_task error%d(%s)\n", errno, strerror(errno));
  };

  return 0;
}

static void *_mqtt_task(void *parm)
{
  struct mg_mgr _mgr_mqtt;
  // Finished, cleanup
  if (!strlen(s_url))
    return -1;

  strncpy(_conn_mqtt.url, s_url, sizeof(_conn_mqtt.url) - 1);
  _conn_mqtt.timeout = 5;
  INIT_LIST_HEAD(&_conn_mqtt.clients);
  //_mgr_mqtt = (struct mg_mgr *)malloc(sizeof(struct mg_mgr));
  mg_mgr_init(&_mgr_mqtt);
  mg_timer_add(&_mgr_mqtt, 10000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_fn, &_mgr_mqtt);

  while (1)
  {
    mg_mgr_poll(&_mgr_mqtt, 6000);
  }
  return NULL;
}

static int reg2bsp()
{
  while (1)
  {
    // register To;
    GSF_MSG_DEF(gsf_mod_reg_t, reg, 8 * 1024);
    reg->mid = GSF_MOD_ID_MQTT_WEBRTC;
    strcpy(reg->uri, GSF_IPC_MQTT_WEBRTC);
    int ret = GSF_MSG_SENDTO(GSF_ID_MOD_CLI, 0, SET, GSF_CLI_REGISTER, sizeof(gsf_mod_reg_t), GSF_IPC_BSP, 2000);
    info("GSF_CLI_REGISTER To:%s, ret:%d, size:%d\n", GSF_IPC_BSP, ret, __rsize);

    static int cnt = 3;
    if (ret == 0)
      break;
    if (cnt-- < 0)
      return -1;
    sleep(1);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("pls input: %s webs_parm.json\n", argv[0]);
    return -1;
  }
  strncpy(mqtt_webrtc_parm_path, argv[1], sizeof(mqtt_webrtc_parm_path) - 1);
  if (json_parm_load(mqtt_webrtc_parm_path, &mqtt_webrtc_cfg) < 0)
  {
    json_parm_save(mqtt_webrtc_parm_path, &mqtt_webrtc_cfg);
    json_parm_load(mqtt_webrtc_parm_path, &mqtt_webrtc_cfg);
  }
  info("wuyaxiong1,cfg.port:%d, cfg.auth:%d\n", mqtt_webrtc_cfg.port, mqtt_webrtc_cfg.auth);

  reg2bsp();

  mqtt_init();

  while (1)
  {
    sleep(10000);
  }

  return 0;
}
