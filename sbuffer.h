
#ifndef DB_NAME

#define DB_NAME Sensor.db
#endif
#ifndef TABLE_NAME

#define TABLE_NAME SensorData
#endif
#ifndef SET_MAX_TEMP
#error SET_MAX_TEMP not set
#endif

#define MAX 500

#ifndef SET_MIN_TEMP
#error SET_MIN_TEMP not set
#endif

#ifndef TIMEOUT
#error TIMEOUT not set
#endif

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0 //0 for no debug printing and 1 for debug printing
#endif

#define FIFO_NAME "logForFIFO"
#define MAX_SERVER_ATTEMPTS 3

#define dbConnRetryDelayTime 3000000
#define dbConnMaxAttempts 4

#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#include <stdio.h>
#include <semaphore.h>
#include "config.h"

#define SBUFFER_FAILURE (-1)
#define SBUFFER_SUCCESS 0
#define SBUFFER_NO_DATA 1

typedef struct sbuffer sbuffer_t;
typedef struct sbuffer_node sbuffer_node_t;

typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
} sbuffer_node_t;


typedef struct sbuffer {
    pthread_cond_t *condition;
    pthread_mutex_t *CLock;
    sem_t *lock;
    volatile int *alive;
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
} sbuffer_t;

/**
 * Allocates and initializes a new shared buffer
 * \param buffer a double pointer to the buffer that needs to be initialized
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_init(sbuffer_t **buffer);

/**
 * All allocated resources are freed and cleaned up
 * \param buffer a double pointer to the buffer that needs to be freed
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_free(sbuffer_t **buffer);

/**
 * Removes the first sensor data in 'buffer' (at the 'head') and returns this sensor data as '*data'
 * If 'buffer' is empty, the function doesn't block until new sensor data becomes available but returns SBUFFER_NO_DATA
 * \param buffer a pointer to the buffer that is used
 * \param data a pointer to pre-allocated sensor_data_t space, the data will be copied into this structure. No new memory is allocated for 'data' in this function.
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbufferRemove(sbuffer_t *buffer, sensor_data_t *data);

/**
 * Inserts the sensor data in 'data' at the end of 'buffer' (at the 'tail')
 * \param buffer a pointer to the buffer that is used
 * \param data a pointer to sensor_data_t data, that will be copied into the buffer
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
*/
int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data);

sbuffer_node_t *sbufferGetFirst(sbuffer_t **buffer);

void writeMsg(FILE *fifo, const char *message);

void writeBuffer(const sbuffer_t *buffer, FILE *fifo);

#endif
