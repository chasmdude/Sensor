#ifndef __errmacros_h__
#define __errmacros_h__

#include <errno.h>

#define CHECK_MKFIFO(err)                                    \
        do {                                                \
            if ( (err) == -1 )                                \
            {                                                \
                if ( errno != EEXIST )                        \
                {                                            \
                    perror("Error executing mkfifo");        \
                    exit( EXIT_FAILURE );                    \
                }                                            \
            }                                                \
        } while(0)

#define FILE_OPEN_ERROR(fp)                                \
        do {                                                \
            if ( (fp) == NULL )                                \
            {                                                \
                perror("File open failed");                    \
                exit( EXIT_FAILURE );                        \
            }                                                \
        } while(0)

#endif
