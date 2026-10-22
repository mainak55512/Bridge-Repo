#include <cstring.h>
#include <mongoose.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

static bool s_done = false;

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_CONNECT) {
    const char *url = (const char *)c->fn_data;
    struct mg_str host = mg_url_host(url);

    // Initialize TLS if connecting via HTTPS
    if (mg_url_is_ssl(url)) {
      struct mg_tls_opts opts = {
          .name = host,
          .skip_verification = true // Set to false if passing a CA certificate
      };
      mg_tls_init(c, &opts);
    }

    // Send HTTP GET request
    mg_printf(c,
              "GET %s HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "User-Agent: mongoose\r\n"
              "Accept: */*\r\n"
              "Connection: close\r\n\r\n",
              mg_url_uri(url), (int)host.len, host.buf);

  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    printf("HTTP Status: %d\n", mg_http_status(hm));
    printf("Body (%.*s):\n%.*s\n", (int)hm->body.len, "bytes",
           (int)hm->body.len, hm->body.buf);

    c->is_closing = 1;
    s_done = true;

  } else if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
    s_done = true;
  }
}

int main(void) {
  Arena *arena = arena_init(1024);
  mg_log_set(MG_LL_NONE);
  mg_boot_timestamp_ms = (uint64_t)time(NULL) * 1000 - mg_millis();
  struct mg_mgr mgr;
  char *url = "https://mainak55512.github.io/flint-cherts/packages/raylib";

  int url_len = strlen(url);

  if (url[url_len] != '/') {
    url = string(string_concat_cstr(arena, 2, url, "/"));
  }

  mg_mgr_init(&mgr);

  if (mg_http_connect(&mgr, url, fn, (void *)url) == NULL) {
    fprintf(stderr, "Failed to initiate connection\n");
    mg_mgr_free(&mgr);
    return 1;
  }

  while (!s_done) {
    mg_mgr_poll(&mgr, 50);
  }

  mg_mgr_free(&mgr);
  arena_free(&arena);
  return 0;
}
