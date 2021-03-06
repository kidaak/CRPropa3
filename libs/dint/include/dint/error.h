#ifndef DINT__ERROR_H
#define DINT__ERROR_H

typedef enum {NO_ERROR = 0, ARRAY_ERROR = 1, IO_ERROR = 2, PROGRAM_ERROR = 3} 
ErrorCode;

void Error(const char* errorMessage, const ErrorCode errorCode);

#endif
