#include "so_stdio.h"
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

#define BUFFER_SIZE 4096
#define PERMISSIONS 0644
#define WRITE_OPERATION 1
#define READ_OPERATION 2
#define ERROR_CODE -1
#define PIPE_READ   0
#define PIPE_WRITE  1
#define COMMAND "sh"
#define FLAG "-c"
#define READ "r"
#define READ_WRITE "r+"
#define WRITE "w"
#define WRITE_READ "w+"
#define APPEND "a"
#define APPEND_READ "a+"

struct _so_file {
    int file_descriptor;
	char *buffer;
    int buffer_offset;
    int last_operation;
    int cursor_position;
    int bytes_read;
    int errors;
    int eof;
    pid_t pid;
};