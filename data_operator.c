#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include "config.h"
#include "data_operator.h"
#include "errmacros.h"
#include "lib/dplist.h"
#include "sbuffer.h"

dplist_t *sensorList = NULL;

FILE *getFIFO(const FILE *fp_sensor_map, const sbuffer_t *buffer, int result);

void *element_copy(void *element);

void element_free(void **element);

int element_compare(void *x, void *y);

uint16_t datamgrGetSensorIdAtIndex(int index);

void ParseSensorData(FILE *fp_sensor_map, sbuffer_t *buffer) {
    printf("---------------START OF DATA-MANAGER---------------\n");

    int result = 0;
    FILE *fifo = getFIFO(fp_sensor_map, buffer, result);

    sensorList = dpl_create(&element_copy, &element_free, &element_compare);
    uint16_t sensorID;
    uint16_t roomID;
    uint16_t currSensorId;
    long tempTs;
    double tempValue;
    double currValue;
    sensor_data_t *sensor_data = NULL;
    for (int i = 0; fscanf(fp_sensor_map, "%hd %hd\n", &roomID, &sensorID) != EOF; i++) {
        sensor_data = calloc(1, sizeof(sensor_data_t));
        sensor_data->roomID = roomID;
        sensor_data->id = sensorID;
        sensor_data->sData[RUN_AVG_LENGTH - 1] = SET_MIN_TEMP-1;
        dpl_insert_at_index(sensorList, sensor_data, i, false);
    }
    fclose(fp_sensor_map);
    printf("DATA MANAGER: Inserted sensor map into DPList\n");

    sbuffer_node_t *currSensor = NULL;
    while (*(buffer->alive) || sbufferGetFirst(&buffer) != NULL) {
        sem_wait(buffer->lock);
        if (sbufferGetFirst(&buffer) != NULL) {
            if (currSensor == NULL && sbufferGetFirst(&buffer) != NULL) {
                currSensor = sbufferGetFirst(&buffer);
            }

            if (sbufferGetFirst(&buffer)->data.hasSMread && sbufferGetFirst(&buffer)->data.hasDMread) {
                printf("DATA MANAGER: Buffer completed its cycle and hence deleting \n");
                fflush(stdout);
                sensor_data_t datap;
                sbufferRemove(buffer, &datap);
                printf("DATA Manager: buffer removed");
                writeMsg(fifo, "DATA Manager: buffer removed");
                writeBuffer(buffer, fifo);
                if (sbufferGetFirst(&buffer) == NULL) {
                    currSensor = NULL;
                }
                if (currSensor == NULL) {
                    currSensor = sbufferGetFirst(&buffer);
                }
            }

            if (sbufferGetFirst(&buffer) != NULL && !sbufferGetFirst(&buffer)->data.hasDMread) {
                currSensor = sbufferGetFirst(&buffer);
            }

            if (currSensor != NULL && !currSensor->data.hasDMread) {
                sem_post(buffer->lock);
                sensor_data_t *tempSensorData = NULL;
                tempSensorData = &currSensor->data;
                currSensorId = tempSensorData->id;
                currValue = tempSensorData->value;
                tempTs = tempSensorData->sensorTs;
                sensor_data_t *ele = NULL;
                int j;
                bool isabsent = true;
                for (j = 0; j < datamgr_get_total_sensors(); j++) {
                    if (datamgrGetSensorIdAtIndex(j) == currSensorId) {
                        isabsent = false;
                        char *helperStr = NULL;
                        asprintf(&helperStr, "DATA MANAGER: Sensor :%d in room: %d\n",
                                 datamgrGetSensorIdAtIndex(j), datamgr_get_room_id(currSensorId));
                        printf("%s", helperStr);
                        free(helperStr);
                        ele = dpl_get_element_at_index(sensorList, j);
                        ele->runLength %= RUN_AVG_LENGTH;
                        ele->sData[ele->runLength] = currValue;
                        ele->runLength++;
                        if (ele->sensorTs < tempTs) ele->sensorTs = tempTs;
                        if (ele->sData[RUN_AVG_LENGTH - 1] == SET_MIN_TEMP-1) {
                            tempValue = 0;
                        } else {
                            tempValue = 0;
                            for (int o = 0; o < RUN_AVG_LENGTH; o++) {
                                tempValue = tempValue + ele->sData[o];
                            }
                        }
                        ele->value = tempValue / RUN_AVG_LENGTH;

                        for (int k = 0; k < RUN_AVG_LENGTH; k++) {
                            char *temp_string;
                            asprintf(&temp_string, " |  %8f |", ele->sData[k]);
                            printf("%s\n", temp_string);
                            free(temp_string);
                        }
                        printf("\n");
                        printf("DATA MANAGER: Avg Val of %d :-> %f\n", ele->roomID, ele->value);
                        if (ele->value > SET_MAX_TEMP && ele->value != 0) {
                            char *temp_string;
                            asprintf(&temp_string,
                                     "Too hot (Avg temperature = %f)\n", ele->value);
                            writeMsg(fifo, temp_string);
                            free(temp_string);
                        }
                        if (ele->value < SET_MIN_TEMP && ele->value != 0) {
                            char *tempString;
                            asprintf(&tempString,
                                     "Sensor %d : too cold Avg temperature = %f\n",
                                     ele->id, ele->value);
                            writeMsg(fifo, tempString);
                            free(tempString);
                        }
                    } else {
                        if (j == datamgr_get_total_sensors() - 1 && isabsent) {
                            char *temp_string;
                            asprintf(&temp_string, "Received sensor data with invalid sensor node ID %d\n",
                                     currSensorId);
                            writeMsg(fifo, temp_string);
                            free(temp_string);
                        }
                    }
                }
                sem_wait(buffer->lock);
                tempSensorData->hasDMread = true;
                printf("DATA: used: ");
                writeMsg(fifo, "DATA: used: ");
                writeBuffer(buffer, fifo);
            }
            if (currSensor != NULL && currSensor->next != NULL) currSensor = currSensor->next;
        } else {
            if (*(buffer->alive)) {
                printf("DATA-MGR: I am waiting on a signal from Connection Manager\n");
                pthread_cond_wait(buffer->condition, buffer->CLock);
            }
        }
        sem_post(buffer->lock);
    }
    pthread_cond_signal(buffer->condition);
    fclose(fifo);
    dpl_free(&sensorList, false);
    printf("---------------END OF DATA MANAGER---------------\n");
}

void dataMgrFree() {
    printf("DATA MANAGER: Free\n");
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id_input) {
    for (int i = 0; i < datamgr_get_total_sensors(); ++i) {
        if (datamgrGetSensorIdAtIndex(i) == sensor_id_input) {
            return ((sensor_data_t *) dpl_get_element_at_index(sensorList, i))->roomID;
        }
    }
    ERROR_HANDLER(1, "Invalid Sensor Id");
}

uint16_t datamgrGetSensorIdAtIndex(int index) {
    if (sensorList == NULL || index < 0) return -1;
    return ((sensor_data_t *) dpl_get_element_at_index(sensorList, index))->id;
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id_input) {
    if (sensorList == NULL) return -1;
    for (int i = 0; i < datamgr_get_total_sensors(); ++i) {
        if (datamgrGetSensorIdAtIndex(i) == sensor_id_input) {
            return ((sensor_data_t *) dpl_get_element_at_index(sensorList, i))->value;
        }
    }
    ERROR_HANDLER(1, "Invalid Sensor Id");
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id_input) {
    if (sensorList == NULL) return -1;
    for (int i = 0; i < datamgr_get_total_sensors(); ++i) {
        if (datamgrGetSensorIdAtIndex(i) == sensor_id_input) {
            return ((sensor_data_t *) dpl_get_element_at_index(sensorList, i))->sensorTs;
        }
    }
    ERROR_HANDLER(1, "Invalid Sensor Id");
}

int datamgr_get_total_sensors() {
    return dpl_size(sensorList);
}

FILE *getFIFO(const FILE *fp_sensor_map, const sbuffer_t *buffer, int result) {
    result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result);
    FILE *fifo = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifo);

    if (buffer == NULL) {
        printf("DATA MANAGER: problem getting sensor data\n");
    }
    if (fp_sensor_map == NULL) {
        printf("DATA MANAGER: problem opening room_sensor.map\n");
    }
    return fifo;
}

void *element_copy(void *element) {
    sensor_data_t *copy = malloc(sizeof(sensor_data_t));
    *copy = *((sensor_data_t *) element); // Use assignment to copy the structure
    return (void *) copy;
}

void element_free(void **element) {
    free(*element);
    *element = NULL;
}

int element_compare(void *x, void *y) {
    double x_value = ((sensor_data_t *) x)->value;
    double y_value = ((sensor_data_t *) y)->value;

    if (x_value < y_value) {
        return -1;
    } else if (x_value > y_value) {
        return 1;
    }
    return 0;
}
