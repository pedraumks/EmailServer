#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

static void handle_client(int fd);

user_list_t from_user;
user_list_t to_user;
net_buffer_t buf;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {

    // TODO To be implemented

    struct utsname unameData;
    char request[MAX_LINE_LENGTH];
    char template[13] = "temp_XXXXXX\0";
    int fb, len, status = 0;
    buf = nb_create(fd, MAX_LINE_LENGTH);
    from_user = create_user_list();
    to_user = create_user_list();
    uname(&unameData);
    char *message = malloc(MAX_LINE_LENGTH);
    memset(message, '\0', MAX_LINE_LENGTH);
    strcat(message, "220 ");
    strcat(message, unameData.nodename);
    strcat(message, " Simple Mail Transfer Service Ready \r\n");
    send_string(fd, message);
    while (1) {
        len = nb_read_line(buf, request);
        if (len > 0) {
            if (strncasecmp(request, "HELO ", 5) == 0) {
                if (status == 0) {
                    char *ps = strchr(request, ' ');
                    char *user_host = malloc(MAX_LINE_LENGTH);
                    strcpy(user_host, ps + 1);
                    memset(message, '\0', MAX_LINE_LENGTH);
                    strcat(message, "250 ");
                    strcat(message, unameData.nodename);
                    strcat(message, " greets ");
                    strcat(message, user_host);
                    send_string(fd, message);
                    free(user_host);
                    status = 1;
                } else {
                    send_string(fd, "503 COMMAND OUT OF SEQUENCE\r\n");
                }
            } else if (strncasecmp(request, "MAIL FROM:", 10) == 0) {
                if (status == 1 || status == 4) {
                    char *ps, *pr;
                    ps = strchr(request, '<');
                    pr = strchr(request, '>');
                    size_t size = (pr != NULL && ps != NULL) ? pr - ps - 1 : 0;
                    if (size > 0) {
                        char *temp = malloc(size);
                        strncpy(temp, ps + 1, size);
                        add_user_to_list(&from_user, temp);
                        memset(temp, '\0', size);
                        free(temp);
                        send_string(fd, "250 OK \r\n");
                        status = 2;
                    } else {
                        send_string(fd, "501 PARAM ERROR IN MAIL FROM\r\n");
                    }
                } else {
                    send_string(fd, "503 COMMAND OUT OF SEQUENCE\r\n");
                }
            } else if (strncasecmp(request, "RCPT TO:", 8) == 0) {
                if (status == 2 || status == 3) {
                    char *ps, *pr;
                    ps = strchr(request, '<');
                    pr = strchr(request, '>');
                    size_t size = (ps != NULL && pr != NULL) ? pr - ps - 1 : 0;
                    if (size > 0) {
                        char *temp = malloc(size);
                        strncpy(temp, ps + 1, size);
                        if (is_valid_user(temp, NULL)) {
                            add_user_to_list(&to_user, temp);
                            send_string(fd, "250 OK \r\n");
                            status = 3;
                        } else {
                            send_string(fd, "550 NO SUCH USER\r\n");
                        }
                        memset(temp, '\0', size);
                        free(temp);
                    } else {
                        send_string(fd, "501 PARAM ERROR IN RCPT TO\r\n");
                    }
                } else {
                    send_string(fd, "503 COMMAND OUT OF SEQUENCE \r\n");
                }
            } else if (strcasecmp(request, "DATA\r\n") == 0) {
                if (status == 3) {
                    send_string(fd, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
                    fb = mkstemp(template);
                    nb_read_line(buf, request);
                    while (strncasecmp(request, ".\r\n", 3) != 0) {
                        write(fb, request, strlen(request));
                        nb_read_line(buf, request);
                    }
                    write(fb, request, strlen(request));
                    save_user_mail(template, to_user);
                    close(fb);
                    unlink(template);
                    send_string(fd, "250 OK \r\n");
                    status = 4;
                } else {
                    send_string(fd, "503 COMMAND OUT OF SEQUENCE\r\n");
                }
            } else if (strcasecmp(request, "NOOP\r\n") == 0) {
                send_string(fd, "250 OK \r\n");
            } else if (strcasecmp(request, "QUIT\r\n") == 0) {
                strcpy(message, "\0");
                strcat(message, "221 ");
                strcat(message, unameData.nodename);
                strcat(message, " Service closing transmission channel\r\n");
                send_string(fd, message);
                free(message);
                destroy_user_list(from_user);
                destroy_user_list(to_user);
                nb_destroy(buf);
                close(fd);
                break;
            } else if (strncasecmp(request, "EHLO", 4) == 0 ||
                       strncasecmp(request, "RSET", 4) == 0 ||
                       strncasecmp(request, "VRFY", 4) == 0 ||
                       strncasecmp(request, "EXPN", 4) == 0 ||
                       strncasecmp(request, "HELP", 4) == 0) {
                send_string(fd, "502 COMMAND NOT IMPLEMENTED\r\n");
            } else {
                send_string(fd, "500 INVALID COMMAND\r\n");
            }
        } else {
            send_string(fd, "500 COMMAND NOT PROVIDED\r\n");
        }
    }
}
