#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sqlite3.h>
#include <unistd.h>
#include "sbuffer.h"
#include <sys/stat.h>
#include "config.h"
#include "errmacros.h"
#include "storage_manager.h"


FILE *fifoLogger = NULL;

bool connectedToDatabase(char *db_name, sqlite3 **db);

int validateConn(int connRes);

void logConnLostMsg();

int performQuery(sqlite3 *conn, callback_t f, char *full_query);

bool connectedToDatabase(char *db_name, sqlite3 **db) {
    if ((sqlite3_open(db_name, db)) == SQLITE_OK) {
        char *succMsg = "\nDatabase Opened Successfully\n";
        printf("%s", succMsg);
        writeMsg(fifoLogger, succMsg);
        return true;
    } else {
        fprintf(stderr, "\nSQL: Unable to establish connection to the SQL server.\n Retrying in %d seconds...\n",
                dbConnRetryDelayTime);
        usleep(dbConnRetryDelayTime);
        char *tempMsg = NULL;
        if (asprintf(&tempMsg, "\nSQL: Connection to SQL server failed.\n") != -1) {
            writeMsg(fifoLogger, tempMsg);
            free(tempMsg);
        } else {
            fprintf(stderr, "\n Memory allocation error for tempMsg.\n");
        }
        sqlite3_close(*db);
        return false;
    }
}

DBCONN *init_connection(char clear_up_flag, sbuffer_t *buffer) {
    int result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result);
    fifoLogger = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifoLogger);

    sqlite3 *db = NULL;
    char *errMsg = NULL;
    for (int i = 1, allowedAttempts = dbConnMaxAttempts; !connectedToDatabase(TO_STRING(DB_NAME), &db);) {
        if (i == allowedAttempts) {
            *(buffer->alive) = 0;
            sqlite3_free(errMsg);
            return NULL;
        }
        printf("\nSQL: Attempt %d to connect\n", ++i);
    }

    char *qualifiedQuery = NULL;
    if (clear_up_flag == 1) {
        asprintf(&qualifiedQuery,
                 "DROP TABLE IF EXISTS %s; CREATE TABLE %s(id INTEGER PRIMARY KEY AUTOINCREMENT, sensorId INTEGER, sensor_value NUMERIC, timestamp INTEGER);",
                 TO_STRING(TABLE_NAME), TO_STRING(TABLE_NAME));
    } else {
        asprintf(&qualifiedQuery,
                 "CREATE TABLE IF NOT EXISTS %s(id INTEGER PRIMARY KEY AUTOINCREMENT, sensorId INTEGER, sensor_value NUMERIC, timestamp INTEGER);",
                 TO_STRING(TABLE_NAME));
    }

    if (qualifiedQuery != NULL) {
        printf("\nSQL: Query: %s\n", qualifiedQuery);
        int execResult = sqlite3_exec(db, qualifiedQuery, 0, 0, &errMsg);
        free(qualifiedQuery);

        if (execResult != SQLITE_OK) {
            char *connLost = "\nConnection to SQL server lost.\n";
            fprintf(stderr, "%s", connLost);
            char *logger_message = NULL;
            asprintf(&logger_message, "%s", connLost);
            writeMsg(fifoLogger, logger_message);
            free(logger_message);
            sqlite3_free(errMsg);
            sqlite3_close(db);
            return NULL;
        }

        char *message;
        asprintf(&message, "\nNew table %s created.\n", TO_STRING(TABLE_NAME));
        writeMsg(fifoLogger, message);
        free(message);

        sqlite3_free(errMsg);
        return db;
    } else {
        fprintf(stderr, "\nMemory allocation error for qualifiedQuery.\n");
        sqlite3_close(db);
        return NULL;
    }
}

void disconnect(DBCONN *conn) {
    sqlite3_close(conn);
}

int insert_sensor(DBCONN *conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts) {
    if (conn == NULL) {
        char *errorMsg = "\nSQL: connection lost while inserting a sensor\n";
        printf("%s", errorMsg);
        writeMsg(fifoLogger, errorMsg);
        fflush(stdout);
        fclose(fifoLogger);
        return -1;
    }
    char *errMsg = 0;
    char *fullQuery = NULL;
    char *temp_string = NULL;
    asprintf(&fullQuery, "INSERT INTO main.%s(sensorId,sensor_value,timestamp) VALUES (%hd,%lf,%ld);",
             TO_STRING(TABLE_NAME), id, value, ts);
    asprintf(&temp_string, "SQL: Query :%s\n", fullQuery);
    printf("%s", temp_string);
    writeMsg(fifoLogger, temp_string);
    int execResult = sqlite3_exec(conn, fullQuery, 0, 0, &errMsg);
    free(temp_string);
    free(fullQuery);
    sqlite3_free(errMsg);
    return validateConn(execResult);
}

int insertSensorFromBuffer(DBCONN *conn, sbuffer_t *buffer) {
    printf("---------------SQL BEGINS---------------\n");
    uint16_t sensorIdTemp;
    long currTime;
    double currTempValue;

    if (conn == NULL || buffer == NULL) {
        printf("SQL: connection or buff is not valid\n");
        fclose(fifoLogger);
        return -1;
    }

    sbuffer_node_t *currSensor = NULL;
    while (*(buffer->alive) || sbufferGetFirst(&buffer) != NULL) {
        sem_wait(buffer->lock);
        if (sbufferGetFirst(&buffer) != NULL) {
            if (currSensor == NULL) {
                currSensor = sbufferGetFirst(&buffer);
            }

            if (sbufferGetFirst(&buffer)->data.hasSMread && sbufferGetFirst(&buffer)->data.hasDMread) {
                printf("SQL: data-mgr and store-mgr read this data\n");
                fflush(stdout);
                sensor_data_t datap;
                sbufferRemove(buffer, &datap);
                printf("SQL: Deleted\n");
                writeMsg(fifoLogger, "SQL: Deleted\n");
                writeBuffer(buffer, fifoLogger);
                if (sbufferGetFirst(&buffer) == NULL) {
                    currSensor = NULL;
                }
                if (currSensor == NULL) {
                    currSensor = sbufferGetFirst(&buffer);
                }
            }

            if (sbufferGetFirst(&buffer) != NULL && !sbufferGetFirst(&buffer)->data.hasSMread) {
                currSensor = sbufferGetFirst(&buffer);
            }

            if (currSensor != NULL && !currSensor->data.hasSMread) {
                sem_post(buffer->lock);
                sensor_data_t *tempSensorData = NULL;
                tempSensorData = &currSensor->data;
                sensorIdTemp = tempSensorData->id;
                currTempValue = tempSensorData->value;
                currTime = tempSensorData->sensorTs;
                insert_sensor(conn, sensorIdTemp, currTempValue, currTime);
                sem_wait(buffer->lock);
                tempSensorData->hasSMread = true;
                writeMsg(fifoLogger, "SQL:Read: ");
                printf("SQL: Read: ");
                writeBuffer(buffer, fifoLogger);
            }
            if (currSensor != NULL && currSensor->next != NULL) {
                currSensor = currSensor->next;
            }
        } else {
            if (*(buffer->alive)) {
                printf("SQL: Waiting for Connection-Manager\n");
                pthread_cond_wait(buffer->condition, buffer->CLock);
            }
        }
        sem_post(buffer->lock);
    }
    pthread_cond_signal(buffer->condition);
    fclose(fifoLogger);

    printf("----------------- SQL ENDS -------------------\n");
    return 0;
}

int find_sensor_all(DBCONN *conn, callback_t f) {
    char *queryString = "SELECT * FROM %s";
    if (conn == NULL) {
        return -1;
    }
    char *full_query = NULL;
    asprintf(&full_query, queryString, TO_STRING(TABLE_NAME));
    return performQuery(conn, f, full_query);
}

int find_sensor_by_value(DBCONN *conn, sensor_value_t value, callback_t f) {
    char *queryString = "SELECT * FROM %s WHERE sensor_value == %f";
    if (conn == NULL) return -1;
    char *full_query = NULL;
    asprintf(&full_query, queryString, TO_STRING(TABLE_NAME), value);
    return performQuery(conn, f, full_query);
}

int performQuery(sqlite3 *conn, callback_t f, char *full_query) {
    printf("SQL: we query :%s\n", full_query);
    char *err_msg = 0;
    int rc = sqlite3_exec(conn, full_query, f, 0, &err_msg);
    free(full_query);
    sqlite3_free(err_msg);
    return validateConn(rc);
}

int find_sensor_exceed_value(DBCONN *conn, sensor_value_t value, callback_t f) {
    if (conn == NULL) return -1;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE sensor_value > %f", TO_STRING(TABLE_NAME), value);
    return performQuery(conn, f, full_query);
}

int find_sensor_by_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f) {
    if (conn == NULL) return -1;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE timestamp == %ld", TO_STRING(TABLE_NAME), ts);
    return performQuery(conn, f, full_query);
}

int find_sensor_after_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f) {
    if (conn == NULL) return -1;
    char *full_query = NULL;
    asprintf(&full_query, "SELECT * FROM %s WHERE timestamp > %ld", TO_STRING(TABLE_NAME), ts);
    return performQuery(conn, f, full_query);
}

int validateConn(int connRes) {
    if (connRes != SQLITE_OK) {
        fprintf(stderr, "Connection to SQL server lost.\n");
        logConnLostMsg();
        return -1;
    } else return 0;
}

void logConnLostMsg() {
    char *logger_message = NULL;
    asprintf(&logger_message, "Connection to SQL server lost.\n");
    writeMsg(fifoLogger, logger_message);
    free(logger_message);
}
