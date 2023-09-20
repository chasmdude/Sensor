
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "connection_manager.h"
#include "sbuffer.h"
#include "config.h"
#include "data_operator.h"
#include "storage_manager.h"
#include "errmacros.h"

void *writerThread(void *arg);

void *dataThread(void *arg);

void *sqlThread(void *arg);

sbuffer_t *buff;
pthread_t w_thread;
pthread_t r_threads[2];

int port = 1234;

pthread_cond_t condition;
pthread_mutex_t bufferCleanupLock;
sem_t sema_lock;

void mainProcess() {
    FILE *fifo = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fifo);
    volatile int thread_alive = 1;
    while (true) {
        printf("---------------START OF MAIN---------------\n");
        sbuffer_init(&buff);
        buff->CLock = &bufferCleanupLock;
        buff->condition = &condition;
        buff->lock = &sema_lock;
        buff->alive = &thread_alive;
        pthread_create(&w_thread, NULL, &writerThread, NULL);
        pthread_create(&r_threads[0], NULL, &dataThread, NULL);
        pthread_create(&r_threads[1], NULL, &sqlThread, NULL);

        pthread_join(w_thread, NULL);
        pthread_join(r_threads[0], NULL);
        pthread_join(r_threads[1], NULL);

        sbuffer_free(&buff);
        fclose(fifo);
        printf("---------------END OF MAIN---------------\n");
        pthread_exit(NULL);
    }
}

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        port = atoi(argv[1]);
        printf("Using provided PORT: %d\n", port);
    } else {
        printf("Using Default port %d \n", port);
    }

    int mkf = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(mkf);
    sem_init(&sema_lock, 0, 1);
    pid_t loggerPid;
    loggerPid = fork();
    if (loggerPid == 0) {
        char logger_buff[MAX];
        FILE *fifo = fopen(FIFO_NAME, "r");
        FILE_OPEN_ERROR(fifo);
        FILE *logger_file = fopen("gateway.log", "w");
        FILE_OPEN_ERROR(logger_file);
        char *tempStr = NULL;
        int ctr = 0;
        do {
            tempStr = fgets(logger_buff, MAX, fifo);
            if (tempStr != 0) {
                time_t long_time;
                struct tm *struct_time;
                long_time = time(NULL);
                struct_time = localtime(&long_time);
                char *temp_time = asctime(struct_time);
                temp_time[strlen(temp_time) - 1] = '0';
                fprintf(logger_file, "%d %s %s \n", ctr++, temp_time, logger_buff);
                fflush(logger_file);
            }
        } while (tempStr != 0);
        fclose(fifo);
        fclose(logger_file);
    } else {
        mainProcess();
    }
}

void *writerThread(void *arg) {
    listenTo(port, buff);
    connectionManagerFree();
    pthread_detach(w_thread);
    pthread_exit(NULL);
}

void *dataThread(void *arg) {
    FILE *sensor_map = fopen("room_sensor.map", "r");
    FILE_OPEN_ERROR(sensor_map);
    ParseSensorData(sensor_map, buff);
    dataMgrFree();
    pthread_detach(r_threads[0]);
    pthread_exit(NULL);
}

void *sqlThread(void *arg) {
    DBCONN *conn = init_connection(1, buff);
    insertSensorFromBuffer(conn, buff);
    disconnect(conn);
    pthread_detach(r_threads[1]);
    pthread_exit(NULL);
}