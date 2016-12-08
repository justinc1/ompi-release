/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */

#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "opal/runtime/opal_osv_support.h"
#include <stdio.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h>
#include "opal/util/argv.h"
#include <assert.h>
#include <string.h>

/* 
 * Return 1 if inside OSv, 0 otherwise.
 * */
int opal_is_osv() {
    /* osv_execve is imported as weak symbol, it is NULL if not present */
    if(osv_execve) {
        return true;
    }
    else {
        return false;
    }
}

/**
 * Replacement for getpid().
 * In Linux, return usual process ID.
 * In OSv, return thread ID instead of process ID.
 */
pid_t opal_getpid()
{
    pid_t id;
    if(opal_is_osv()) {
        id = syscall(__NR_gettid);
    }
    else {
        id = getpid();
    }
    return id;
}


static int tcp_connect(char *host, int port) {
  int sockfd, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  fprintf(stderr, "HTTP Connecting to %s:%d\n", host, port);
  /* Create a socket point */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("ERROR opening socket");
    return -1;
  }
  /* resolve DNS name */
  server = gethostbyname(host);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host %s\n", host);
    return -1;
  }
  /* connect to server */
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(port);
  /* Now connect to the server */
  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR connecting");
    return -1;
  }

  return sockfd;
}

static http_client_t http_connect(char *host, int port) {
  http_client_t httpc = {NULL, 0, -1};
  httpc.sockfd = tcp_connect(host, port);
  if (httpc.sockfd < 0) {
      return httpc;
  }
  httpc.host = strdup(host); // free
  httpc.port = port;
  return httpc;

}

static int tcp_close(int sockfd) {
    close(sockfd);
}

static int http_close(http_client_t *httpc) {
    if (httpc->sockfd != -1) {
        close(httpc->sockfd);
        httpc->sockfd = -1;
    }
    if (httpc->host) {
        free(httpc->host);
        httpc->host = NULL;
    }
}

static int tcp_write(int sockfd, char* buf, size_t sz) {
    size_t sz2;
    sz2 = write(sockfd, buf, sz);
    if(sz2 < 0) {
        perror("ERROR tcp_write failed");
        exit(1);
    }
    if(sz2 != sz) {
        perror("ERROR tcp_write incomplete");
        exit(1);
    }
    return sz2;
}

static char rfc3986[256] = {0};
static char html5[256] = {0};
static int url_table_setup_done = 0;

void url_encoder_rfc_tables_init() {
    if(url_table_setup_done)
        return;
    url_table_setup_done = 1;

    int i;
    for (i = 0; i < 256; i++) {
        rfc3986[i] = isalnum( i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
        html5[i] = isalnum( i) || i == '*' || i == '-' || i == '.' || i == '_' ? i : (i == ' ') ? '+' : 0;
    }
}

char *url_encode(char *table, unsigned char *s) {
    url_encoder_rfc_tables_init();
    char *enc0, *enc;
    enc0 = malloc(strlen(s)*3);
    if (enc0 == NULL)
        return NULL;

    enc = enc0;
    for ( ; *s; s++) {
        if (table[*s]) {
            sprintf( enc, "%c", table[*s]);
        }
        else {
            sprintf( enc, "%%%02X", *s);
        }
        while (*++enc);
    }
    return enc0;
}

static int http_send(http_client_t httpc, char* method, char* buf) {
    char buf2[1024*10];
    int pos = 0;

    /* POST/PUT data */
    /*
    HTTP/1.1 could be used to reuse existing tcp connection.
    But OSv REST server is 1.0 only anyway.
    */
    pos += snprintf(buf2+pos, sizeof(buf2)-pos, "%s %s HTTP/1.0\r\n", method, buf);
    if (pos >= sizeof(buf2)) {
        return -1;
    }
    // headers, skip User-Agent
    pos += snprintf(buf2+pos, sizeof(buf2)-pos, "Host: %s:%d\r\n", httpc.host, httpc.port);
    if (pos >= sizeof(buf2)) {
        return -1;
    }
    pos += snprintf(buf2+pos, sizeof(buf2)-pos, "Accept: */*\r\n");
    if (pos >= sizeof(buf2)) {
        return -1;
    }
    // done
    pos += snprintf(buf2+pos, sizeof(buf2)-pos, "\r\n");
    if (pos >= sizeof(buf2)) {
        return -1;
    }

    fprintf(stderr, "HTTP send %s\n", buf2);
    return tcp_write(httpc.sockfd, buf2, strlen(buf2));
}

static int http_get(http_client_t httpc, char* buf) {
    return http_send(httpc, "GET", buf);
}

static int http_put(http_client_t httpc, char* buf) {
    return http_send(httpc, "PUT", buf);
}

static int http_post(http_client_t httpc, char* buf) {
    return http_send(httpc, "POST", buf);
}

static int tcp_read(int sockfd, char* buf, size_t sz) {
    size_t sz2;
    bzero(buf, sz);
    sz2 = read(sockfd, buf, sz);
    if(sz2 < 0) {
        perror("ERROR tcp_read failed");
        exit(1);
    }
    return sz2;
}

static int http_read(http_client_t httpc, char* buf, size_t sz) {
    int ret;
    /*
    HACK != FIX
    Prevent occassional/frequent hang. When client, say orted connects to other OSv VM,
    it might hang while reading from TCP socket. Looking at tcpdump,
    all is ok - valid response data was sent, fin and fin-ack were sent, but read(sockfd) hangs.
    We should look closer at this, and try to repeat on a simpler test case -
    it could be OSv problem.
    For now, we just forget to read data - it was ignored anyway.
    */
#if 0
    ret = tcp_read(httpc.sockfd, buf, sz);
    fprintf(stderr, "HTTP received %s\n", buf);
#endif
    // TODO check buf
    return 200;
}

/*
Start program on remote OSv VM.
Return 0 on success, negative number on error.

It does:
POST /env/PATH?val=%2Fusr%2Fbin%3A%2Fusr%2Flib
PUT /app/?command=...
*/
int opal_osvrest_run(char *host, int port, char **argv) {
    int ret = -1;
    /*
    Setup env PATH. It should not be empty, or even /usr/lib/orted.so (with abs path)
    will not be "found" on PATH;
    */
    char env_path[] = "/env/PATH?val=%2Fusr%2Fbin%3A%2Fusr%2Flib";

    /* Assemble command to run on OSv VM. */
    char* var;
    var = opal_argv_join(argv, ' ');
    /* OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                         "%s plm:osv_rest: host %s:%d, cmd %s)",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), host, port, var)); */
    fprintf(stderr, "TTRT osv_rest_run host %s:%d, cmd %s", host, port, var);
    /* urlencode data part */
    char *var_enc = url_encode(html5, var);
    fprintf(stderr, "HTTP run var     %s\n", var);
    fprintf(stderr, "HTTP run var_enc %s\n", var_enc);
    /* build full URL */
    size_t var2_maxlen = strlen(var) + 100;
    char *var2 = malloc(var2_maxlen);
    if (var2==NULL) {
        goto DONE;
    }
    snprintf(var2, var2_maxlen, "/app/?command=%s", var_enc);

    http_client_t httpc;
    char buf[1024];

    /* Make sure OSv VM is fully up. Retry up to 3 times. */
    int max_connect_retries = 3;
    while (max_connect_retries-- > 0) {
        httpc = http_connect(host, port);
        if (httpc.sockfd < 0) {
            sleep(1);
            continue;
        }
        http_get(httpc, "/os/version");
        http_read(httpc, buf, sizeof(buf));
        http_close(&httpc);
        break;
    }

    httpc = http_connect(host, port);
    if (httpc.sockfd < 0) {
        goto DONE;
    }
    http_post(httpc, env_path);
    http_read(httpc, buf, sizeof(buf));
    http_close(&httpc);

    httpc = http_connect(host, port);
    if (httpc.sockfd < 0) {
        goto DONE;
    }
    http_put(httpc, var2);
    http_read(httpc, buf, sizeof(buf));
    ret = 0;

DONE:
    http_close(&httpc);
    if(var) {
        free(var);
    }
    if (var_enc) {
        free(var_enc);
    }
    if(var2) {
        free(var2);
    }
    return ret;
}
