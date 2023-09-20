#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "sbuffer.h"

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }
    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbufferRemove(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    if (buffer->head == NULL) return SBUFFER_NO_DATA;
    *data = buffer->head->data;
    dummy = buffer->head;
    if (buffer->head == buffer->tail) {
        buffer->head = buffer->tail = NULL;
    } else {
        buffer->head = buffer->head->next;
    }
    free(dummy);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;
    dummy->data = *data;
    dummy->next = NULL;
    if (buffer->tail == NULL) {
        buffer->head = buffer->tail = dummy;
    } else {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
    return SBUFFER_SUCCESS;
}

sbuffer_node_t *sbufferGetFirst(sbuffer_t **buffer) {
    if (*buffer == NULL || ((*buffer)->head == NULL) || ((*buffer)->tail == NULL)) {
        return NULL;
    }
    return (*buffer)->head;
}

void writeBuffer(const sbuffer_t *buffer, FILE *fifo) {
    if (buffer == NULL) {
        return;
    }

    char *intro = "\t\t --------\tBuffer\t -------------- \t\t\n";
    char *outro = "--------------------------------------------\n";
    printf("%s", intro);
    writeMsg(fifo, intro);
    char *headerFormat = "| %10s | %10s | %10s |\n";
    printf(headerFormat, "id", "Read by Data-Manager", "Read by Store-Manager");
    printf("%s", outro);
    writeMsg(fifo, outro);
    char *rowFormat = "| %8d | %8d | %8d |\n";
    for (sbuffer_node_t *dummy = buffer->head; dummy != NULL; dummy = dummy->next) {
        printf(rowFormat, dummy->data.id, dummy->data.hasDMread, dummy->data.hasSMread);
        char *temp_string;
        asprintf(&temp_string, rowFormat, dummy->data.hasDMread, dummy->data.hasSMread, dummy->data.id);
        writeMsg(fifo, temp_string);
    }
    printf("%s", outro);
    writeMsg(fifo, outro);
    fflush(stdout);
}

void writeMsg(FILE *fifo, const char *message) {
    if (fifo == NULL) {
        return;
    }
    mkfifo(FIFO_NAME, 0666);
    if (fputs(message, fifo) == EOF) {
        fprintf(stderr, "Error writing data to fifo\n");
        exit(EXIT_FAILURE);
    }
    fflush(fifo);
}
