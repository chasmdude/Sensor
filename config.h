#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;
typedef time_t sensor_ts_t;
typedef int sensor_count_t;

typedef struct {
    sensor_id_t id;         /** < sensor id */
    sensor_id_t roomID;
    sensor_value_t value;   /** < sensor value */
    sensor_ts_t sensorTs;         /** < sensor timestamp */
    sensor_value_t sData[RUN_AVG_LENGTH];
    sensor_count_t runLength;
    bool hasSMread;
    bool hasDMread;
} sensor_data_t;

#endif /* _CONFIG_H_ */
