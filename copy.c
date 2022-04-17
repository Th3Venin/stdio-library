#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include "so_stdio.h"
#include "utils.h"
#include <string.h>
#define INVALID_FLAG -1
#define BUFFER_SIZE 4096

struct _so_file {
	int _fileDescriptor; // file descriptorul fisierului
	char* _openingMode; //modul de deschidere al fisierului
	int _endOfFile; // daca am ajuns la end of file
	char _buffer[BUFFER_SIZE]; // bufferul in care scriem sau din care citim 
	char _lastOperation[1024]; //numele ultimei operatii 
	int _actualPosInBuffer; //pozitia actuala in buffer 
	int _actualPosInFile; //dimensiunea actuala a bufferului
	int _errorFile; //daca intalnesc eroare
	int _nrBytesRead; //numarul de bytes cititi
	pid_t _childPid; // pid ul procesului copil
};



SO_FILE *so_fopen(const char *pathname, const char *mode) {
	
	int openingFlag = -100;
	if(isRead(mode) == 1) {
		openingFlag = O_RDONLY;
	}
	else if (isWrite(mode) == 1) {
		openingFlag = O_WRONLY | O_TRUNC | O_CREAT;
	} 
	else if (isReadWrite(mode) == 1) {
		openingFlag = O_RDWR;
	}
	else if (isWriteRead(mode) == 1) {
		openingFlag = O_RDWR | O_TRUNC | O_CREAT;
	}
	else if (isAppend(mode) == 1) {
		openingFlag = O_APPEND | O_CREAT | O_WRONLY;
	}
	else if (isAppendRead(mode) == 1) {
		openingFlag = O_APPEND | O_CREAT | O_RDWR;
	}
	else {
		openingFlag = INVALID_FLAG;
	}
	
	if (openingFlag == INVALID_FLAG) {
		printf("Cannot set open flags\n");
		return NULL;
	}

	int fileDescriptor = open(pathname, openingFlag, 0644);
	if(fileDescriptor < 0) {
		printf("Cannot open file\n");
		return NULL;
	}

	SO_FILE *f = (SO_FILE*)malloc(sizeof(SO_FILE));
	if(f == NULL) {
		printf("Cannot allocate memory\n");
		f->_errorFile = 1;
		free(f);
		return NULL;
	}
	f->_openingMode = (char*)malloc((strlen(mode) + 1) * sizeof(char));
	if(f->_openingMode == NULL) {
		printf("Cannot allocate memory\n");
		f->_errorFile = 1;
		free(f->_openingMode);
		free(f);
		return NULL;
	}

	f->_fileDescriptor = fileDescriptor;
	strcpy(f->_openingMode, mode);
	strcpy(f->_buffer, "");
	strcpy(f->_lastOperation, "");
	f->_actualPosInFile = 0;
	f->_endOfFile = 0;
	f->_actualPosInBuffer = 0;
	f->_errorFile = 0;
	f->_nrBytesRead = 0;
	f->_childPid = -1;
	return f;
}

int so_fclose(SO_FILE *stream) {
	
	if(isWriteOperation(stream->_lastOperation) == 1) {
		int ok = so_fflush(stream);
		if(ok == SO_EOF) {
			stream->_errorFile = 1;
			if(stream->_openingMode != NULL) {
				free(stream->_openingMode);
				stream->_openingMode = NULL;
			}
			if(stream != NULL) {
				free(stream);
				stream = NULL;
			}
		}
	}

	int bRet = close(stream->_fileDescriptor);
	if(bRet < 0) {
		stream->_errorFile = 1;
		if(stream->_openingMode != NULL) {
			free(stream->_openingMode);
			stream->_openingMode = NULL;
		}
		if(stream != NULL) {
			free(stream);
			stream = NULL;
		}
	}
	if(stream->_openingMode != NULL) {
		free(stream->_openingMode);
		stream->_openingMode = NULL;
	}
	if(stream != NULL) {
		free(stream);
		stream = NULL;
	}

	return 0;
}

int so_fileno(SO_FILE *stream) {
	return stream->_fileDescriptor;
}

long so_ftell(SO_FILE *stream) {
	return stream->_actualPosInFile;
}

int so_fflush(SO_FILE *stream) {
	
	if(isWriteOperation(stream->_lastOperation) == 1) {
		int currentPos = stream->_actualPosInBuffer;
		int bRet = write(stream->_fileDescriptor, stream->_buffer, currentPos);
		if(bRet == -1) {
			stream->_errorFile = 1;
			return SO_EOF;
		}
		strcpy(stream->_buffer, "");
		stream->_actualPosInBuffer = 0;
		return 0;
	}
	else {
		return SO_EOF;
	}
}

int so_fseek(SO_FILE *stream, long offset, int whence) {

	if(isWriteOperation(stream->_lastOperation) == 1) {
		int bRet = so_fflush(stream);
		if(bRet == SO_EOF) {
			return SO_EOF;
		}
	}
	else if(isReadOperation(stream->_lastOperation) == 1) {
		stream->_actualPosInBuffer = 0;
		strcpy(stream->_buffer, "");
	}
	int position = lseek(stream->_fileDescriptor, offset, whence);
	if(position < 0) {
		return SO_EOF;
	}
	stream->_actualPosInFile = position;
	return 0;
	
}

int isReading(SO_FILE* stream, int total, const void *ptr) {
	int i = 0;
	int cnt = 0;
	while(i < total) {
		char* aux = (char*)ptr;
		char c = so_fgetc(stream);
		aux[i] = (char)c;
		if(stream->_errorFile == 1) {
			break;
		}
		i++;
	}
	cnt = i;
	return cnt;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {

	
	stream->_nrBytesRead = 0;
	int total = size * nmemb;
	int reading = isReading(stream, total, ptr);
	strcpy(stream->_lastOperation, "read_bunch_of_bytes");
	int nrBytesRead = reading / size;
	return nrBytesRead;
}

int isWriting(SO_FILE* stream, int total, const void *ptr) {
	int i = 0;
	int cnt = 0;
	char* aux = (char*)ptr;
	while(i < total) {
		so_fputc((int)aux[i], stream);
		if(stream->_errorFile == 1 || stream->_endOfFile) {
			break;
		}
		i++;
	}
	cnt = i;
	return cnt;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {

	int total = size * nmemb;
	int ok = isWriting(stream, total, ptr);
	strcpy(stream->_lastOperation, "write_bunch_of_bytes");
	int nrBytesWritten = nmemb;
	return nrBytesWritten;
}


int so_fgetc(SO_FILE *stream) {

	if(canRead(stream->_openingMode) == 1)
	{
		int currentPos = stream->_actualPosInBuffer;
		if( (currentPos == 0) || (currentPos == BUFFER_SIZE) ||
			(isWriteOperation(stream->_lastOperation) == 1)) {
			int bRet = read(stream->_fileDescriptor, stream->_buffer, BUFFER_SIZE);
			if(bRet == 0) {
				stream->_endOfFile = 1;
				return SO_EOF;
				}
			else if (bRet == -1) {
				stream->_errorFile = 1;
				return SO_EOF;
			}
			strcpy(stream->_lastOperation, "read_a_char");
			stream->_actualPosInBuffer = 0;
			stream->_nrBytesRead = bRet;
		}
			
		currentPos = stream->_actualPosInBuffer;
		if(currentPos == stream->_nrBytesRead) {
			stream->_endOfFile = 1;
			return SO_EOF;
		}
		int position = stream->_actualPosInBuffer;
		stream->_actualPosInBuffer++;
		stream->_actualPosInFile++;
		return (int)stream->_buffer[position];
	}
	else {
		stream->_errorFile = 1;
		return SO_EOF;
	}
}


int so_fputc(int c, SO_FILE *stream) {
	if(canWrite(stream->_openingMode) == 1)
	{
		int currentPos = stream->_actualPosInBuffer;
		if(currentPos == BUFFER_SIZE) {
			int bRet = so_fflush(stream);
			if(bRet == SO_EOF) {
				stream->_errorFile = 1;
				return SO_EOF;
			}
		}
		
		stream->_buffer[stream->_actualPosInBuffer] = (char)c;
		stream->_actualPosInBuffer++;
		stream->_actualPosInFile++;
		strcpy(stream->_lastOperation, "write_a_char");
		return c;
	}
	else {
		stream->_errorFile = 1;
		return SO_EOF;
	}
}

int so_feof(SO_FILE *stream) {
	return stream->_endOfFile;
}

int so_ferror(SO_FILE *stream) {
	return stream->_errorFile;
}

SO_FILE *so_popen(const char *command, const char *type) {
	
	int fds[2];
	int bRet = pipe(fds);
	if(bRet < 0) {
		printf("Cannot create a pipe!");
		return NULL;
	}
	pid_t pid = fork();
	int fileDescriptor = 0;
    if(pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return NULL;
	}
	else if (pid == 0) {
		if(strcmp(type, "r") == 0) {
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			close(fds[1]);
		}
		else { 
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO);
			close(fds[0]);
		}
		int ok;
		ok = execlp("sh", "sh", "-c", command, NULL);
		exit(1);
	}
	else {
		if(strcmp(type, "r") == 0) {
			close(fds[1]);
			fileDescriptor = fds[0];
		}
		else { 
			close(fds[0]);
			fileDescriptor = fds[1];
		}
	}
	SO_FILE* f = (SO_FILE*)malloc(sizeof(SO_FILE));
	if(f == NULL) {
		printf("Cannot allocate memory\n");
		f->_errorFile = 1;
		free(f);
		return NULL;
	}
	f->_openingMode = (char*)malloc((strlen(type) + 1) * sizeof(char));
	if(f->_openingMode == NULL) {
		printf("Cannot allocate memory\n");
		f->_errorFile = 1;
		free(f->_openingMode);
		free(f);
		return NULL;
	}

	f->_fileDescriptor = fileDescriptor;
	strcpy(f->_openingMode, type);
	strcpy(f->_buffer, "");
	strcpy(f->_lastOperation, "");
	f->_actualPosInFile = 0;
	f->_endOfFile = 0;
	f->_actualPosInBuffer = 0;
	f->_errorFile = 0;
	f->_nrBytesRead = 0;
	f->_childPid = pid;
	return f;
}
int so_pclose(SO_FILE *stream) {

	pid_t pid = stream->_childPid;
	if(pid == -1) {
		return SO_EOF;
	}
	else {
		int ok = so_fclose(stream);
		if(ok < 0) {
			return SO_EOF;
		}
		int status;
		ok = waitpid(pid, &status, 0);
		if(ok < 0) {
			return ok;
		}
		return status;
	}
}
