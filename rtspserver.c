#include <stdio.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#define DEFAULT_UDP_PORT 5004
#define DEFAULT_RTSP_PORT 8554
#define DEFAULT_MOUNT_POINT "/live"
#define DEFAULT_ENCODER "H264"

static guint cintr = FALSE;
static gboolean quit = FALSE;
static GMainLoop *main_loop = NULL;
static gchar *cfg_file = NULL;
static int log_level = 0; 

static GOptionEntry entries[] = {
  {"cfg-file", 'c', 0, G_OPTION_ARG_FILENAME, &cfg_file,
      "Set the config file", NULL},
  {"log-level", 'l', 0, G_OPTION_ARG_INT, &log_level,
      "Set the log level", NULL},
  {NULL}
};

struct Settings 
{
    guint udp_port; 
    guint rtsp_port;
    gchar *mount_point; 
    guint64 udp_buffer_size; 
    gchar *encoder_name; 
}; 

static void load_settings(struct Settings *settings, const gchar *cfg_file)
{
    g_assert(settings); 
    settings->udp_port = DEFAULT_UDP_PORT; 
    settings->rtsp_port = DEFAULT_RTSP_PORT;
    settings->udp_buffer_size = 512 * 1024; 
    settings->encoder_name = (char*)DEFAULT_ENCODER; 

    if (cfg_file) {
        g_print("Load config from file: %s\n", cfg_file); 
        JsonParser *parser = json_parser_new();
        GError *error = NULL;
        json_parser_load_from_file(parser, cfg_file, &error);
        if (error) {
          g_print ("Unable to parse %s: %s\n", cfg_file, error->message);
          g_error_free (error);
        }
        else {
          JsonNode *root = json_parser_get_root (parser);
          if (!JSON_NODE_HOLDS_OBJECT (root)) {
            g_print ("Invalid format in json file %s\n", cfg_file);
          }
          else {
            JsonObject *root_object = json_node_get_object (root);
            if (!json_object_has_member(root_object, "rtspserver")) {
              g_print ("Unable to get rtspserver object in json file %s\n", cfg_file); 
            }
            else {
              JsonObject *object = json_object_get_object_member (root_object, "rtspserver");
              g_assert (object); 
              if (json_object_has_member(object, "udp_port"))
                settings->udp_port = json_object_get_int_member (object, "udp_port");
              if (json_object_has_member(object, "rtsp_port"))
                settings->rtsp_port = json_object_get_int_member (object, "rtsp_port");
              if (json_object_has_member(object, "mount_point")) {
                const gchar *mp = json_object_get_string_member (object, "mount_point");
                settings->mount_point = g_strdup(mp);
              }
              if (json_object_has_member(object, "udp_buffer_size"))
                settings->udp_buffer_size = json_object_get_int_member (object, "udp_buffer_size");
              if (json_object_has_member(object, "encoder_name")) {
                const gchar *enc = json_object_get_string_member (object, "encoder_name");
                settings->encoder_name = g_strdup(enc);
              }
            }
          }
        }
        g_object_unref (parser);
    } 
}

// Loop function to check the status of interrupts.
// It comes out of loop if application got interrupted.
static gboolean check_for_interrupt (gpointer data)
{
  if (quit) {
    return FALSE;
  }

  if (cintr) {
    cintr = FALSE;

    quit = TRUE;
    g_main_loop_quit (main_loop);
    return FALSE;
  }
  return TRUE;
}

// Function to handle program interrupt signal.
// It installs default handler after handling the interrupt.
static void _intr_handler (int signum)
{
  struct sigaction action;

  memset (&action, 0, sizeof (action));
  action.sa_handler = SIG_DFL;

  sigaction (SIGINT, &action, NULL);

  cintr = TRUE;
}

// Function to install custom handler for program interrupt signal.
static void _intr_setup (void)
{
  struct sigaction action;

  memset (&action, 0, sizeof (action));
  action.sa_handler = _intr_handler;

  sigaction (SIGINT, &action, NULL);
}

int main (int argc, char *argv[])
{
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    char udpsrc_pipeline[512];
    char rtsp_port_str[64];
    GOptionContext *optctx;
    GError *error = NULL;

    optctx = g_option_context_new (NULL);
    g_option_context_add_main_entries (optctx, entries, NULL);
    g_option_context_add_group (optctx, gst_init_get_option_group ());
    if (!g_option_context_parse (optctx, &argc, &argv, &error)) {
        g_printerr ("Error parsing options: %s\n", error->message);
        g_option_context_free (optctx);
        g_clear_error (&error);
        return -1;
    }
    g_option_context_free (optctx);

    g_print("log level: %d\n", log_level); 

    struct Settings settings; 
    memset(&settings, sizeof(settings), 0); 
    settings.udp_port = DEFAULT_UDP_PORT;
    settings.rtsp_port = DEFAULT_RTSP_PORT;
    settings.mount_point = DEFAULT_MOUNT_POINT;
    settings.encoder_name = DEFAULT_ENCODER; 
    load_settings(&settings, cfg_file); 

    g_print("udp port: %d\n", settings.udp_port); 
    g_print("rtsp port: %d\n", settings.rtsp_port); 
    g_print("mount point: '%s'\n", settings.mount_point); 
    g_print("encoder name: %s\n", settings.encoder_name); 

    guint64 udp_buffer_size = settings.udp_buffer_size; 
    if (udp_buffer_size == 0)
        udp_buffer_size = 512 * 1024;

    sprintf (udpsrc_pipeline,
    "(udpsrc name=pay0 port=%d buffer-size=%lu " 
    "caps=\"application/x-rtp, media=video, clock-rate=90000, encoding-name=%s, payload=96\")",
    settings.udp_port, udp_buffer_size, settings.encoder_name);

    sprintf (rtsp_port_str, "%d", settings.rtsp_port);
 
    main_loop = g_main_loop_new (NULL, FALSE);

    /* create a server instance */
    server = gst_rtsp_server_new ();
    g_object_set (server, "service", rtsp_port_str, NULL);

    /* get the mount points for this server, every server has a default object
    * that be used to map uri mount points to media factories */
    mounts = gst_rtsp_server_get_mount_points (server);

    /* make a media factory for a test stream. The default media factory can use
    * gst-launch syntax to create pipelines.
    * any launch line works as long as it contains elements named pay%d. Each
    * element with pay%d names will be a stream */
    factory = gst_rtsp_media_factory_new ();
    gst_rtsp_media_factory_set_launch (factory, udpsrc_pipeline);
    gst_rtsp_media_factory_set_shared (factory, TRUE);

    /* attach the factory to mount point */
    gst_rtsp_mount_points_add_factory (mounts, settings.mount_point, factory);

    /* don't need the ref to the mapper anymore */
    g_object_unref (mounts);

    /* attach the server to the default maincontext */
    gst_rtsp_server_attach (server, NULL);

    _intr_setup ();
    g_timeout_add (400, check_for_interrupt, NULL);

    /* start serving */
    g_print ("stream ready at rtsp://localhost:%d/%s\n", settings.rtsp_port, settings.mount_point);
    g_main_loop_run (main_loop);

    return 0;
}
