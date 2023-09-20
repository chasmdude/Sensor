#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "connection_manager.h"
#include "./lib/tcpsock.h"
#include "./lib/dplist.h"
#include "poll.h"
#include "sbuffer.h"
#include "errmacros.h" //this is the professors code as taken from Toledo


tcpsock_t *server;
dplist_t *pollFd;
dplist_t *socketNodeList;
FILE *fifo;

void *element_copy_poll(void *element);

void element_free_poll(void **element);

int element_compare_poll(void *x, void *y);

void *element_copy_node(void *element);

void element_free_node(void **element);

int element_compare_node(void *x, void *y);

void pollTillPipesClosed(sbuffer_t *buffer, int alive, int *fd);

void debugLogSockets();

void debugLogPoll();

void handleNewTCPConn(int *fd, int *alive, int *attempts);

int handleTimeOut(int alive);

void listenTo(int port_number, sbuffer_t *buffer) {
    printf("-----------------------BEGINNING OF CONNECTION-MANAGER-----------------------\n");
    int result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result);
    fifo = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifo);

    pollFd = dpl_create(&element_copy_poll, &element_free_poll, &element_compare_poll);
    socketNodeList = dpl_create(&element_copy_node, &element_free_node, &element_compare_node);
    int alive = 0, currFd;
    if (tcp_passive_open(&server, port_number) != TCP_NO_ERROR)
        exit(EXIT_FAILURE);

    pollfd_t *temp_poll = NULL;
    temp_poll = calloc(1, sizeof(pollfd_t));
    temp_poll->events = POLLIN;
    tcp_get_sd(server, &currFd);
    temp_poll->fd = currFd;
    dpl_insert_at_index(pollFd, temp_poll, 0, true);
    free(temp_poll);
    alive++;
    pollTillPipesClosed(buffer, alive, &currFd);
    *(buffer->alive) = 0;
    pthread_cond_broadcast(buffer->condition);
    fclose(fifo);
    printf("CONN-MANAGER: Test server is shutting down\n");
    fflush(stdout);
}

void pollTillPipesClosed(sbuffer_t *buffer, int alive, int *fd) {
    int attempts = 0,bytes =0;
    while (alive > 0 && *(buffer->alive)) {
        if (DEBUG_PRINT) {
            debugLogSockets();
            debugLogPoll();
        }
        if (attempts == MAX_SERVER_ATTEMPTS) {
            printf("CONN-MANAGER:Idle for long time\n");
            writeMsg(fifo, "CONN-MANAGER:Idle for long time\n");
            alive = 0;
        }
        alive = handleTimeOut(alive);
        struct pollfd poll_fd_temp[dpl_size(pollFd) + 1];
        for (int i = 0; i < dpl_size(pollFd) + 1; i++) {
            poll_fd_temp[i] = *(pollfd_t *) dpl_get_element_at_index(pollFd, i);
        }

        int result;
        if (attempts < MAX_SERVER_ATTEMPTS) {
            result = poll(poll_fd_temp, alive, TIMEOUT * 1000);
            if (result == -1) {
                printf("CONN-MANAGER: problem encountered while polling\n");
                fflush(stdout);
                exit(1);
            }
        }

        for (int i = 0; i < dpl_size(pollFd); i++) {
            dpl_remove_at_index(pollFd, i, true);
            pollfd_t *currPoll = NULL;
            currPoll = malloc(sizeof(pollfd_t));
            memset(currPoll, 0, sizeof(pollfd_t));
            currPoll->events = poll_fd_temp[i].events;
            currPoll->revents = poll_fd_temp[i].revents;
            currPoll->fd = poll_fd_temp[i].fd;
            dpl_insert_at_index(pollFd, currPoll, i, true);
            free(currPoll);
        }
        int i;
        for (i = 0; i < dpl_size(pollFd); i++) {
            pollfd_t *iterPoll = NULL;
            iterPoll = (pollfd_t *) dpl_get_element_at_index(pollFd, 0);
            if (i == 0) {
                if (iterPoll->revents == POLLIN) {
                    handleNewTCPConn(fd, &alive, &attempts);
                } else {
                    attempts++;
                }
            }
            if (i > 0) {
                sensor_data_t *data = calloc(1, sizeof (sensor_data_t));
                bytes = sizeof(data->id);
                iterPoll = (pollfd_t *) dpl_get_element_at_index(pollFd, i);
                if (iterPoll->revents == POLLIN) {
                    tcp_node_t *currNode = dpl_get_element_at_index(socketNodeList, i - 1);
                    tcp_receive(currNode->socket, &(data->id), &bytes);
                    bytes= sizeof(data->value);
                    tcp_receive(currNode->socket, &(data->value), &bytes);
                    bytes= sizeof(data->sensorTs);
                    result = tcp_receive(currNode->socket, &(data->sensorTs), &bytes);
                    if (result == TCP_NO_ERROR) {
                        attempts = 0;
                        sbuffer_insert(buffer, data);
                        pthread_cond_broadcast(buffer->condition);
                        char *msgHelper = NULL;
                        asprintf(&msgHelper, "CONN-MANAGER: From %d  %8hu, %8lf, %8ld\n",
                                 i, data->id, data->value, data->sensorTs);
                        writeMsg(fifo, msgHelper);
                        printf("%s", msgHelper);
                        free(msgHelper);

                        tcp_node_t *temp_node = (tcp_node_t *) dpl_get_element_at_index(socketNodeList, i - 1);
                        if (temp_node->lastTS == 0) {
                            char *temp_string = NULL;
                            asprintf(&temp_string, "Sensor Node: %d has opened fd new connection\n", data->id);
                            printf("%s", temp_string);
                            writeMsg(fifo, temp_string);
                            free(temp_string);
                            temp_node->lastTS = time(NULL);
                            temp_node->sensorId = data->id;
                        }

                        tcp_node_t *pTcpNode = (tcp_node_t *) dpl_get_element_at_index(socketNodeList, i - 1);
                        asprintf(&msgHelper, "CONN-MANAGER: last-active set %ld \n",
                                 pTcpNode->lastTS = time(NULL));
                        free(msgHelper);
                    }
                    if (result == TCP_CONNECTION_CLOSED) {
                        tcp_node_t *tempNode = (tcp_node_t *) dpl_get_element_at_index(socketNodeList, i - 1);
                        char *temp_string = NULL;
                        asprintf(&temp_string, "Connection closed with sensor node (ID: %d)\n",
                                 tempNode->sensorId);
                        printf("%s", temp_string);
                        writeMsg(fifo, temp_string);
                        free(temp_string);
                        tcp_close(&(tempNode->socket));
                        dpl_remove_at_index(socketNodeList, i - 1, true);
                        dpl_remove_at_index(pollFd, i, true);
                        alive--;
                    }
                    if (result != TCP_NO_ERROR && result != TCP_CONNECTION_CLOSED) {
                        char *tempString = NULL;
                        asprintf(&tempString,
                                 "CONN-MANAGER: While Receiving data, encountered err code : %d\n", result);
                        free(tempString);
                    }
                }
                free(data);
            }
        }
    }
}

int handleTimeOut(int alive) {
    for (int j = 0; j < dpl_size(socketNodeList); j++) {
        tcp_node_t *temp_sock = ((tcp_node_t *) (dpl_get_element_at_index(socketNodeList, j)));
        time_t temp_ts;
        int currSd = 0;
        temp_ts = temp_sock->lastTS;
        tcp_get_sd(temp_sock->socket, &currSd);
        if (temp_ts <= (time(NULL) - TIMEOUT) && temp_ts != 0) {
            char *temp_string = NULL;
            asprintf(&temp_string, "CONN-MANAGER: socket %d has been idle for long time \n", currSd);
            writeMsg(fifo, "CONN-MANAGER: socket %d has been idle for long time\n");
            printf("%s", temp_string);
            free(temp_string);
            tcp_close(&((tcp_node_t *) dpl_get_element_at_index(socketNodeList, j))->socket);
            dpl_remove_at_index(socketNodeList, j, true);
            dpl_remove_at_index(pollFd, j + 1, true);
            alive--;
            printf("CONN-MANAGER: connection closed\n");
        }
    }
    return alive;
}

void handleNewTCPConn(int *fd, int *alive, int *attempts) {
    (*attempts) = 0;
    printf("CONN-MANAGER: waiting for new socket\n");
    fflush(stdout);
    tcp_node_t *currClient = malloc(sizeof(tcp_node_t));
    currClient->lastTS = 0;
    currClient->sensorId = 1;
    currClient->socket = NULL;
    if (tcp_wait_for_connection(server, &(currClient->socket)) != TCP_NO_ERROR) {
        exit(1);
    }
    printf("CONN-MANAGER: got fd new socket\n");
    tcp_get_sd((currClient->socket), fd);
    pollfd_t *currPoll = NULL;
    currPoll = calloc(1, sizeof(pollfd_t));
    currPoll->revents = 0;
    currPoll->events = POLLIN;
    currPoll->fd = (*fd);
    dpl_insert_at_index(pollFd, currPoll, (*alive), true);
    dpl_insert_at_index(socketNodeList, currClient, (*alive) - 1, true);
    free(currPoll);
    free(currClient);
    printf("CONN-MANAGER: inserted new socket\n");
    (*alive)++;
}

void debugLogPoll() {
    char *intro = "\t\t --------\tCONNECTION-MANAGER: Poll\t -------------- \t\t\n";
    char *outro = "--------------------------------------------\n";
    writeMsg(fifo, intro);
    printf("%s", intro);
    for (int j = 0; j < dpl_size(pollFd); j++) {
        pollfd_t *currPill = (pollfd_t *) dpl_get_element_at_index(pollFd, j);
        char *temp_string;
        asprintf(&temp_string, "\n\t\t || %8d | %8hd | %8hd ||\n", currPill->fd, currPill->events, currPill->revents);
        printf("%s", temp_string);
        writeMsg(fifo, temp_string);
        free(temp_string);
    }
    writeMsg(fifo, intro);
    printf("%s", intro);
    printf("%s", outro);
}

void debugLogSockets() {
    char *intro = "\t\t --------\tCONNECTION-MANAGER: Sockets\t -------------- \t\t\n";
    char *outro = "--------------------------------------------\n";
    printf("%s", intro);
    printf("%s", outro);
    char *rowFormat = "| %10ld | 10%d | %10p |";
    for (int j = 0; j < dpl_size(socketNodeList); j++) {
        tcp_node_t *tempNode = (tcp_node_t *) dpl_get_element_at_index(socketNodeList, j);
        char *temp_char;
        asprintf(&temp_char, rowFormat, tempNode->lastTS, tempNode->sensorId, tempNode->socket);
        printf("%s \n", temp_char);
        free(temp_char);
    }
    printf("%s", outro);
    printf("%s", intro);
}

void connectionManagerFree() {
    printf("CONNECTION-MANAGER: closing socket, freeing poll, socket list\n");
    tcp_close(&server);
    dpl_free(&pollFd, true);
    dpl_free(&socketNodeList, true);
    printf("CONNECTION-MANAGER: freed everything\n");
    fflush(stdout);
}

void *element_copy_poll(void *element) {
    pollfd_t *copy = malloc(sizeof(pollfd_t));
    *copy = *((pollfd_t *) element);
    return (void *) copy;
}

void element_free_poll(void **element) {
    free(*element);
    *element = NULL;
}

int element_compare_poll(void *x, void *y) {
    double x_value = ((pollfd_t *) x)->fd;
    double y_value = ((pollfd_t *) y)->fd;

    if (x_value < y_value) {
        return -1;
    } else if (x_value == y_value) {
        return 0;
    }
    return 1;
}

void *element_copy_node(void *element) {
    tcp_node_t *copy = malloc(sizeof(tcp_node_t));
    *copy = *((tcp_node_t *) element);
    return (void *) copy;
}

void element_free_node(void **element) {
    free(*element);
    *element = NULL;
}

int element_compare_node(void *x, void *y) {
    time_t x_value = ((tcp_node_t *) x)->lastTS;
    time_t y_value = ((tcp_node_t *) y)->lastTS;

    if (x_value < y_value) {
        return -1;
    } else if (x_value == y_value) {
        return 0;
    }
    return 1;
}