#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024

char* rcpt_user;
char* rcpt_pass;
mail_list_t user_mail;
mail_item_t item;

static void handle_client(int fd);

int main(int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\r\n", argv[0]);
    return 1;
  }

  run_server(argv[1], handle_client);

  return 0;
}

void handle_client(int fd) {

  // TODO To be implemented

    net_buffer_t buf = nb_create(fd, MAX_LINE_LENGTH);
    rcpt_user = malloc(MAX_USERNAME_SIZE);
    rcpt_pass = malloc(MAX_PASSWORD_SIZE);
    char request[MAX_LINE_LENGTH];
    char command_buf[MAX_LINE_LENGTH];
    int len;
    int isUserSet = 0;

    send_string(fd, "+OK POP3 server ready \r\n");

    while(1) {
        nb_destroy(buf);
        buf = nb_create(fd, MAX_LINE_LENGTH);
        len = nb_read_line(buf, request);
        if (len > 0) {
            if (strncasecmp(request, "USER ", 5) == 0 && !isUserSet) {
                char *ps, *pr;
                ps = strchr(request, ' ');
                pr = strchr(request, '\r');
                size_t size = (ps != NULL && pr != NULL) ? pr - ps - 1 : 0;
                if (size > 0) {
                    rcpt_user = realloc(rcpt_user, size);
                    rcpt_user[size] = '\0';
                    strncpy(rcpt_user, ps + 1, size);
                    if (is_valid_user(rcpt_user, NULL)) {
                        send_string(fd, "+OK that is a valid mailbox \r\n");
                        isUserSet = 1;
                        continue;
                    } else {
                        send_string(fd, "-ERR Sorry that is not a valid mailbox \r\n");
                        continue;
                    }
                } else {
                    send_string(fd, "-ERR no mailbox provided \r\n");
                    continue;
                }
            } else if (strncasecmp(request, "PASS ", 5) == 0 && isUserSet) {
                char *ps, *pr;
                ps = strchr(request, ' ');
                pr = strchr(request, '\r');
                size_t size = (ps != NULL && pr != NULL) ? pr - ps - 1 : 0;
                if (size > 0) {
                    rcpt_pass = realloc(rcpt_pass, size);
                    rcpt_pass[size] = '\0';
                    strncpy(rcpt_pass, ps + 1, size);
                    if (is_valid_user(rcpt_user, rcpt_pass)) {
                        send_string(fd, "+OK maildrop locked and ready \r\n");
                        user_mail = load_user_mail(rcpt_user);
                        break;
                    } else {
                        send_string(fd, "-ERR invalid password \r\n");
                        isUserSet = 0;
                        continue;
                    };
                } else {
                    send_string(fd, "-ERR no password provided \r\n");
                    isUserSet = 0;
                    continue;
                }
            } else if (strcasecmp(request, "QUIT\r\n") == 0) {
                send_string(fd, "+OK POP3 server signing off \r\n");
                free(rcpt_user);
                free(rcpt_pass);
                close(fd);
            } else {
                send_string(fd, "-ERR invalid command entered \r\n");
                isUserSet = 0;
                continue;
            }
        }
    }

    while(1) {
        nb_destroy(buf);
        buf = nb_create(fd, MAX_LINE_LENGTH);
        len = nb_read_line(buf, request);
        if (len > 0) {
            if (strcasecmp(request, "STAT\r\n") == 0) {
                send_string(fd, "+OK %u %zu\r\n", get_mail_count(user_mail), get_mail_list_size(user_mail));
            } else if (strncasecmp(request, "LIST ", 5) == 0 || strcasecmp(request, "LIST\r\n") == 0) {
                char *ps, *pr, *command;
                ps = strchr(request, ' ');
                pr = strchr(request, '\r');
                size_t size = (ps != NULL && pr != NULL) ? pr - ps - 1 : 0;
                if (size > 0) {
                    command = command_buf;
                    command[size] = '\0';
                    strncpy(command, ps + 1, size);
                    int msgnum = atoi(command);
                    if (msgnum > 0) {
                        item = get_mail_item(user_mail, (unsigned)msgnum - 1);
                        if (item == NULL) {
                            send_string(fd, "-ERR no such message\r\n");
                        } else {
                            send_string(fd, "+OK %u %zu\r\n", msgnum, get_mail_item_size(item));
                        }
                    } else {
                        send_string(fd, "-ERR please enter a valid number\r\n");
                    }
                } else {
                    send_string(fd, "+OK %u messages (%zu octets)\r\n", get_mail_count(user_mail), get_mail_list_size(user_mail));
                    int num = get_mail_count(user_mail);
                    for (int i = 0; i < num; i++) {
                        item = get_mail_item(user_mail, (unsigned)i);
                        if (item) {
                            send_string(fd, "%u %zu\r\n", i + 1, get_mail_item_size(item));
                        } else {
                            num++;
                        }
                    }
                }
            } else if (strncasecmp(request, "RETR ", 5) == 0) {
                char *ps, *pr, *command;
                ps = strchr(request, ' ');
                pr = strchr(request, '\r');
                size_t size = (ps != NULL && pr != NULL) ? pr - ps - 1 : 0;
                if (size > 0) {
                    command = command_buf;
                    command[size] = '\0';
                    strncpy(command, ps + 1, size);
                    int msgnum = atoi(command);
                    if (msgnum > 0) {
                        item = get_mail_item(user_mail, (unsigned)msgnum - 1);
                        if (item == NULL) {
                            send_string(fd, "-ERR no such message\r\n");
                        } else {
                            send_string(fd, "+OK %zu octets\r\n", get_mail_item_size(item));
                            char chunk[MAX_LINE_LENGTH];
                            size_t nread;
                            const char *filename = get_mail_item_filename(item);
                            FILE *file = fopen(filename, "r");
                            if (file == NULL) {
                                send_string(fd, "-ERR cannot open file\r\n");
                            }
                            if (file) {
                                while ((nread = fread(chunk, 1, sizeof(chunk), file)) > 0) {
                                    send_all(fd, chunk, nread);
                                }
                                if (ferror(file)) {
                                    send_string(fd, "-ERR cannot read file\r\n");
                                }
                                fclose(file);
                            }
                        }
                    } else {
                        send_string(fd, "-ERR please enter a valid number\r\n");
                    }
                } else {
                    send_string(fd, "-ERR no message number specified to be retrieved.\r\n");
                }
            } else if (strncasecmp(request, "DELE ", 5) == 0) {
                char *ps, *pr, *command;
                ps = strchr(request, ' ');
                pr = strchr(request, '\r');
                size_t size = (ps != NULL && pr != NULL) ? pr - ps - 1 : 0;
                if (size > 0) {
                    command = command_buf;
                    command[size] = '\0';
                    strncpy(command, ps + 1, size);
                    int msgnum = atoi(command);
                    if (msgnum > 0) {
                        item = get_mail_item(user_mail, (unsigned)msgnum - 1);
                        if (item == NULL) {
                            send_string(fd, "-ERR mail item already deleted or specified position is invalid.\r\n");
                        } else {
                            mark_mail_item_deleted(item);
                            send_string(fd, "+OK message deleted.\r\n");
                        }
                    } else {
                        send_string(fd, "-ERR please enter a valid number\r\n");
                    }
                } else {
                    send_string(fd, "-ERR no message number specified to be deleted.\r\n");
                }
            } else if (strcasecmp(request, "RSET\r\n") == 0) {
                unsigned int mail_recovered = reset_mail_list_deleted_flag(user_mail);
                send_string(fd, "+OK %u messages recovered.\r\n", mail_recovered);
            } else if (strcasecmp(request, "NOOP\r\n") == 0) {
                send_string(fd, "+OK \r\n");
            } else if (strcasecmp(request, "QUIT\r\n") == 0) {
                destroy_mail_list(user_mail);
                send_string(fd, "+OK POP3 server signing off (%d messages left) \r\n", get_mail_count(user_mail));
                close(fd);
                break;
            } else {
                send_string(fd, "-ERR invalid command entered \r\n");
                continue;
            }
        }

    }
}