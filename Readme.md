# Room Sensor Software

## Overview

The Room Sensor Software is a simple application designed to monitor room temperature using sensor data. It consists of three main components: Connection Manager, Data Operator, and Storage Manager. These components work together to receive, process, and store temperature data from sensors. Additionally, a Shared Buffer is used for efficient data sharing among the components.

## Components

### 1. Connection Manager

The Connection Manager acts as a server for sensor clients. Sensor clients connect to the server using TCP connections and send room temperature data along with metadata. The Connection Manager is responsible for receiving this data and inserting it into the Shared Buffer. Synchronization between components is achieved using mutex, conditions, and semaphores to ensure thread safety.

### 2. Data Operator

The Data Operator component reads sensor data from the Shared Buffer, which has been inserted by the Connection Manager. It performs the following tasks:

- Filters out invalid sensor IDs.
- Calculates the average of the latest temperatures.
- Notifies if the temperature exceeds predefined minimum and maximum thresholds.

The Data Operator plays a crucial role in processing and analyzing the incoming temperature data.

### 3. Storage Manager

The Storage Manager is responsible for reading sensor data from the Shared Buffer and inserting it into the Sensor Database. This component ensures that the sensor data is stored for future reference or analysis.

## Shared Buffer

The Shared Buffer is an essential part of the system, internally implemented as a Thread-Safe Data Producer List (DP List). It serves as a central hub for sharing data among the components. The Shared Buffer is populated by the Connection Manager and read by both the Data Operator and Storage Manager. To ensure data integrity and prevent race conditions, various synchronization mechanisms such as mutex, conditions, and semaphores are employed.

## Getting Started

To use the Room Sensor Software, follow these steps:
- Use run in makefile to compile and run the main file having all the server side components
- Run the client that is generated after above step
- example command to run sensor client: ./client 102 60 127.0.0.1 1234
- SET_MAX_TEMP -> compile argument to set maximum temperature above which is considered too hot.
- SET_MIN_TEMP -> compile argument to set minimum temperature below which is considered too cold.
- TIMEOUT -> compile argument to set maximum time for waiting to listen to clients  
- PORT -> compile argument to set server port
- DEBUG_PRINT -> compile argument to toggle debug logging

## Conclusion

The Room Sensor Software is a robust solution for monitoring room temperature using sensor data. Its modular architecture allows for easy scalability and customization. By utilizing components like the Connection Manager, Data Operator, and Storage Manager, along with the Shared Buffer for efficient data sharing, this software provides a reliable and extensible solution for room temperature monitoring.