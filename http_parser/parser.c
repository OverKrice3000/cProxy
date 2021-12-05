#include <stdio.h>
#include "proxy.h"
#include <stdbool.h>
#include "parser.h"
#include "logger/log.h"
#include "errcodes.h"
#include "tasks/tasks.h"

#define NEWLINE "\r\n\0"
#define NEWLINE_LENGTH 2

int parse_query(char** query_init, int* query_length){
    int progress = *query_length;
    char* query = *query_init;
    bool forced = (progress == GET_MAX_LENGTH) ? true : false;

    if(progress < 4)
        return PR_QUERY_UNFINISHED;
    if(!starts_with_name("GET\0", query)){
        log_info("THREAD %d: Received an unsupported method %.4s", curthread_id(), query);
        return PR_METHOD_NOT_SUPPORTED;
    }
    progress -= 4;
    query += 4;

    while(progress-- && *(query++) != ' ');

    if(progress < 8 + NEWLINE_LENGTH)
        return PR_QUERY_UNFINISHED;
    if(!starts_with_name("HTTP/1.0\0", query)){
        log_info("THREAD %d: Received an unsupported version %.8s", curthread_id(), query);
        return PR_VERSION_NOT_SUPPORTED;
    }
    progress -= 8;
    query += 8;

    if(!starts_with_name(NEWLINE, query)){
        log_info("THREAD %d: Received an unsupported version %.10s", curthread_id(), query);
        return PR_VERSION_NOT_SUPPORTED;
    }
    progress -= NEWLINE_LENGTH;
    query += NEWLINE_LENGTH;

    char* last_query = query;
    int last_progress = progress;
    while(true){
        while(progress-- && *(query++) != ':');
        if(progress <= 0){
            if(forced)
                break;
            return PR_QUERY_UNFINISHED;
        }
        while(progress-- && *(query++) != '\n');
        if(progress <= 0){
            if(forced)
                break;
            return PR_QUERY_UNFINISHED;
        }
        if(starts_with_name(NEWLINE, query)) {
            progress -= NEWLINE_LENGTH;
            *query_length -= progress;
            return PR_SUCCESS;
        }
        last_query = query;
        last_progress = progress;
    }
    *last_query = '\n';
    *query_length -= (last_progress - 1);
    return PR_SUCCESS;
}

int find_header_by_name(char* query, char* name, char** value){
    while(*(query++) != ' ');
    while(*(query++) != ' ');
    while(*(query++) != '\n');
    while(true){
        char* temp_query = query;
        while(*(query++) != ':');
        query++;
        if(starts_with_name(name, query)){
            *value = query;
            return PR_SUCCESS;
        }
        while(*(query++) != '\n');
        if(starts_with_name(NEWLINE, query))
            break;
    }
    return PR_NO_SUCH_HEADER;
}

bool starts_with_name(char* name, char* string){
    if(*name != *string)
        return false;
    while(*(++name) != '\0' && *name == *(++string));
    if(*name == '\0')
        return true;
    else
        return false;
}

