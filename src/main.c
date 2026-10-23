#include <cstring.h>
#include <mongoose.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <yyjson.h>

struct http_request_ctx {
  const char *url;
  bool done;
  int status_code;
  char *response_body;
  size_t response_len;
  Arena *arena;
};

static bool s_done = false;

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  struct http_request_ctx *ctx = (struct http_request_ctx *)c->fn_data;
  if (ev == MG_EV_CONNECT) {
    const char *url = (const char *)ctx->url;
    struct mg_str host = mg_url_host(url);

    if (mg_url_is_ssl(url)) {
      struct mg_tls_opts opts = {.name = host, .skip_verification = true};
      mg_tls_init(c, &opts);
    }

    mg_printf(c,
              "GET %s HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "User-Agent: mongoose\r\n"
              "Accept: */*\r\n"
              "Connection: close\r\n\r\n",
              mg_url_uri(url), (int)host.len, host.buf);

  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    ctx->status_code = mg_http_status(hm);
    ctx->response_len = hm->body.len;
    ctx->response_body = (char *)arena_alloc(ctx->arena, hm->body.len + 1);

    if (ctx->response_body != NULL) {
      memcpy(ctx->response_body, hm->body.buf, hm->body.len);
      ctx->response_body[hm->body.len] = '\0';
    }

    printf("HTTP Status: %d\n", mg_http_status(hm));
    printf("Body (%.*s):\n%.*s\n", (int)hm->body.len, "bytes",
           (int)hm->body.len, hm->body.buf);

    c->is_closing = 1;
    // s_done = true;
    ctx->done = true;
  } else if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
    // s_done = true;
    ctx->done = true;
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

  struct http_request_ctx ctx = {.url = url,
                                 .done = false,
                                 .status_code = 0,
                                 .response_body = NULL,
                                 .response_len = 0,
                                 .arena = arena};

  if (mg_http_connect(&mgr, ctx.url, fn, &ctx) == NULL) {
    fprintf(stderr, "Failed to initiate connection\n");
    mg_mgr_free(&mgr);
    arena_free(&arena);
    return 1;
  }

  while (!ctx.done) {
    mg_mgr_poll(&mgr, 50);
  }

  // --- Access the received data here ---
  if (ctx.response_body != NULL) {
    printf("Received HTTP Status: %d\n", ctx.status_code);
    printf("Received %zu bytes:\n%s\n", ctx.response_len, ctx.response_body);

    yyjson_doc *doc = yyjson_read(ctx.response_body, ctx.response_len, 0);

    if (!doc) {
      fprintf(stderr, "Failed to parse JSON\n");
      return 1;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *dependency = yyjson_obj_get(root, "raylib");
    yyjson_val *remote = yyjson_obj_get(dependency, "remote");

    printf("Remote: %s\n", yyjson_get_str(remote));

    yyjson_doc_free(doc);

  } else {
    printf("Request failed or no body received. Status: %d\n", ctx.status_code);
  }

  mg_mgr_free(&mgr);
  arena_free(&arena);
  return 0;
}
