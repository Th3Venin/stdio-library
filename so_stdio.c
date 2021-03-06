#include "so_file.h"

int compute_open_mode(const char *mode) 
{

    if (strcmp(mode, READ) == 0) {
	return O_RDONLY;
    } else if (strcmp(mode, READ_WRITE) == 0){
	return O_RDWR;
    } else if (strcmp(mode, WRITE) == 0){
	return O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, WRITE_READ) == 0){
	return O_RDWR | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, APPEND) == 0){
	return O_WRONLY | O_CREAT | O_APPEND;
    } else if (strcmp(mode, APPEND_READ) == 0){
	return O_RDWR | O_CREAT | O_APPEND;
    } else {
	return -1;
    }
}

void initialize_stream(SO_FILE *file_stream, int file_descriptor, pid_t pid)
{
    file_stream->file_descriptor = file_descriptor;
    memset(file_stream->buffer, 0, BUFFER_SIZE);
    file_stream->buffer_offset = 0;
    file_stream->last_operation = 0;
    file_stream->cursor_position = 0;
    file_stream->bytes_read = 0;
    file_stream->errors = 0;
    file_stream->eof = 0;
    file_stream->pid = pid;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{

    int file_descriptor;
    int open_mode = -1;

    SO_FILE *file_stream = (SO_FILE *) malloc(sizeof(SO_FILE));

    if (!file_stream) {
	free(file_stream);
		return NULL;
    }

    file_stream->buffer = (char *) malloc(BUFFER_SIZE * sizeof(char));

    if (!file_stream->buffer) {
		free(file_stream->buffer);
		free(file_stream);
		return NULL;
	}

    open_mode = compute_open_mode(mode);

    if (open_mode < 0) {
	free(file_stream->buffer);
	free(file_stream);
	return NULL;
    }

    file_descriptor = open(pathname, open_mode, PERMISSIONS);

    if (file_descriptor < 0) {
	free(file_stream->buffer);
	free(file_stream);
	return NULL;
    }

    initialize_stream(file_stream, file_descriptor, -1);

    return file_stream;
}

int so_fclose(SO_FILE *stream)
{

    int close_result;
    int fflush_result;

    if (stream->last_operation == WRITE_OPERATION) {
	fflush_result = so_fflush(stream);

	if (fflush_result < 0) {
	    stream->errors = ERROR_CODE;
	    free(stream->buffer);
	    free(stream);
	    return SO_EOF;
	}
    }

    close_result = close(stream->file_descriptor);

	if (close_result < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	free(stream->buffer);
	free(stream);

	return 0;
}

int so_fgetc(SO_FILE *stream)
{

    int read_result;
    int buffer_position;

    if (!stream) {
	stream->errors = ERROR_CODE;
	return SO_EOF;
    }

	if ((stream->buffer_offset == 0) || (stream->buffer_offset == BUFFER_SIZE)
     || (stream->last_operation == WRITE_OPERATION)
     || (stream->buffer_offset == stream->bytes_read)) {
		memset(stream->buffer, 0, BUFFER_SIZE);

		read_result = read(stream->file_descriptor, stream->buffer, BUFFER_SIZE);

		if (read_result == 0) {
			stream->eof = 1;
			return SO_EOF;
		}
	else if (read_result < 0) {
			stream->errors = ERROR_CODE;
			return SO_EOF;
		}

		stream->buffer_offset = 0;
	stream->bytes_read = read_result;
	}

	buffer_position = stream->buffer_offset;
	stream->buffer_offset++;
    stream->cursor_position++;
    stream->last_operation = READ_OPERATION;

	return (int)stream->buffer[buffer_position];
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{

    int total_bytes = size * nmemb;
    int chunk_number = 0;
    int read_result;
    int read_count;

    char *memory_address = (char *)ptr;

    stream->bytes_read = 0;

    if (!stream) {
	stream->errors = ERROR_CODE;
	return SO_EOF;
    }

    while (chunk_number < total_bytes) {
	read_result = so_fgetc(stream);
	memory_address[chunk_number] = read_result;

	if ((stream->errors == ERROR_CODE || stream->eof == 1)
	 && read_result < 0){
	    break;
	}

	chunk_number++;
    }

    read_count = chunk_number / size;
    return read_count;
}

int so_fputc(int c, SO_FILE *stream)
{

    int write_result;

    if (!stream) {
	stream->errors = ERROR_CODE;
	return SO_EOF;
    }

	if (stream->buffer_offset == BUFFER_SIZE) {
		write_result = so_fflush(stream);

	if (write_result < 0) {
	    stream->errors = ERROR_CODE;
	    return SO_EOF;
	}
	}

	stream->buffer[stream->buffer_offset] = c;
	stream->buffer_offset++;
    stream->cursor_position++;
    stream->last_operation = WRITE_OPERATION;

	return c;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    int total_bytes = size * nmemb;
    int chunk_number = 0;
    int write_result;

    char *memory_address = (char *)ptr;

    if (!stream) {
	stream->errors = ERROR_CODE;
	return SO_EOF;
    }

    while (chunk_number < total_bytes) {
	write_result = so_fputc((int)memory_address[chunk_number], stream);

	if ((stream->errors == ERROR_CODE || stream->eof == 1)
	 && write_result < 0){
	    break;
	}

	chunk_number++;
    }

    return nmemb;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
    int seek_result;

    if (!stream) {
	stream->errors = ERROR_CODE;
	return -1;
    }

    if (stream->last_operation == READ_OPERATION) {
	stream->buffer_offset = 0;
		memset(stream->buffer, 0, BUFFER_SIZE);
    }

    if (stream->last_operation == WRITE_OPERATION) {
	so_fflush(stream);
    }

    seek_result = lseek(stream->file_descriptor, offset, whence);

    if (seek_result < 0) {
	return -1;
    }

    stream->cursor_position += seek_result;
    return 0;
}

long so_ftell(SO_FILE *stream)
{
    if (!stream) {
	stream->errors = ERROR_CODE;
	return -1;
    }

    return stream->cursor_position;
}

int so_fflush(SO_FILE *stream)
{

    int write_result;

    if (!stream) {
	stream->errors = ERROR_CODE;
	return SO_EOF;
    }

    write_result = write(stream->file_descriptor,
     stream->buffer, stream->buffer_offset);

    if (write_result < 0) {
	stream->errors = ERROR_CODE;
	return SO_EOF;
    } else{
	while (write_result < stream->buffer_offset) {
	    write_result += write(stream->file_descriptor,
	     stream->buffer + write_result,
	     stream->buffer_offset - write_result);
	}
    }

    memset(stream->buffer, 0, BUFFER_SIZE);
    stream->buffer_offset = 0;

    return 0;
}

int so_fileno(SO_FILE *stream)
{
    return stream->file_descriptor;
}

int so_feof(SO_FILE *stream)
{
    return stream->eof;
}

int so_ferror(SO_FILE *stream)
{
    return stream->errors;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	pid_t pid;

	int procs[2];
    int pipe_result;
    int new_file_descriptor = 0;

    SO_FILE *file_stream;

    pipe_result = pipe(procs);

    if (pipe_result < 0) {
	return NULL;
    }

    pid = fork();

    switch (pid) {

	case -1:
	close(procs[PIPE_READ]);
		close(procs[PIPE_WRITE]);
		return NULL;

	case 0:
	if (strcmp(type, READ) == 0) {
			close(procs[PIPE_READ]);
			dup2(procs[PIPE_WRITE], STDOUT_FILENO);
			close(procs[PIPE_WRITE]);
		}

		else if (strcmp(type, WRITE) == 0) {
			close(procs[PIPE_WRITE]);
			dup2(procs[PIPE_READ], STDIN_FILENO);
			close(procs[PIPE_READ]);
		}

		execlp(COMMAND, COMMAND, FLAG, command, NULL);
		exit(1);

	default:
	if (strcmp(type, READ) == 0) {
			close(procs[PIPE_WRITE]);
			new_file_descriptor = procs[PIPE_READ];
		}

		else if (strcmp(type, WRITE) == 0) {
			close(procs[PIPE_READ]);
			new_file_descriptor = procs[PIPE_WRITE];
		}
		break;
	}

    file_stream = (SO_FILE *) malloc(sizeof(SO_FILE));

    if (!file_stream) {
	free(file_stream);
		return NULL;
    }

    file_stream->buffer = (char *) malloc(BUFFER_SIZE * sizeof(char));

    if (!file_stream->buffer) {
		free(file_stream->buffer);
		free(file_stream);
		return NULL;
	}

    initialize_stream(file_stream, new_file_descriptor, pid);

    return file_stream;
}

int so_pclose(SO_FILE *stream)
{
    pid_t pid = stream->pid;

    int close_result;
    int wait_result;
    int status;
    int options;

	switch (pid) {

    case -1:
	return -1;

    default:
	close_result = so_fclose(stream);

	if (close_result < 0) {
	    return -1;
	}

    options = 0;

	wait_result = waitpid(pid, &status, options);

	if (wait_result < 0) {
	    return -1;
	}

	return status;
    }
}