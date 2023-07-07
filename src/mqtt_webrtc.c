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
//static struct mg_mgr *_mgr_mqtt;

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

static mqtt_conn_t _conn_mqtt;

static void *_mqtt_task(void *parm);

//static const char *s_url = "mqtt://www.bxzryd.cn:1883";
static const char *s_url = "mqtt://120.78.200.246:1883";
static const char *s_sub_topic = "mg/+/test";    // Publish topic
static const char *s_pub_topic = "mg/clnt/test"; // Subscribe topic
static int s_qos = 1;                            // MQTT QoS
static struct mg_connection *s_conn;             // Client connection

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo) { s_signo = signo; }

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
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
    struct mg_str subt = mg_str(s_sub_topic);
    struct mg_str pubt = mg_str(s_pub_topic), data = mg_str("hellowuyaxingnb");
    MG_INFO(("%lu CONNECTED to %s", c->id, s_url));
    struct mg_mqtt_opts sub_opts;
    memset(&sub_opts, 0, sizeof(sub_opts));
    sub_opts.topic = subt;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("%lu SUBSCRIBED to %.*s", c->id, (int)subt.len, subt.ptr));
    struct mg_mqtt_opts pub_opts;
    memset(&pub_opts, 0, sizeof(pub_opts));
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
  }
  else if (ev == MG_EV_CLOSE)
  {
    MG_INFO(("%lu CLOSED", c->id));
    s_conn = NULL; // Mark that we're closed
  }
  (void)fn_data;
}

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