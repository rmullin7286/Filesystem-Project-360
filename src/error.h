/*
* This file defines error types for various operations
*/

//an error type for returning from functions.
//OK means operation successful
typedef enum error
{
    OK,
    ERROR_WRONG_FILETYPE,
    ERROR_INVALID_PERMISSIONS,
    ERROR_PATH_ALREADY_EXISTS
}Error;