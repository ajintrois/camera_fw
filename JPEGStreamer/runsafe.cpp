/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libgen.h> // for dirname()
#include <limits.h>

#include <errno.h>

#define MAX_ARGS 64
#define MAX_ENV 24

// usage:  replace the system call with this

/*
//...............
// Call this instead of base-system call
nRet = run_command_parsed(onvifcmdbuff);
// You can safely continue your code here
if (nRet != 0) 
{
    fprintf(stderr, "onvif command failed with status %d\n", nRet);
} 
else 
{
    printf("onvif command ran successfully.\n");
}
// Your next logic continues...
//..................
*/

// --- Argument parser: safe, quoted, no expansion ---
int parse_args(const char *input, char **argv) 
{
    int argc = 0;
    const char *p = input;
    char token[1024];
    int i = 0;
    int in_quote = 0;
    char quote_char = '\0';

    while (*p) 
	{
        if (isspace(*p) && !in_quote) 
		{
            if (i > 0) 
			{
                token[i] = '\0';
                argv[argc++] = strdup(token);
                i = 0;
                if (argc >= MAX_ARGS - 1) break;
            }
            p++;
        } 
		else if ((*p == '"' || *p == '\'') && !in_quote) 
		{
            in_quote = 1;
            quote_char = *p++;
        } 
		else if (*p == quote_char && in_quote) 
		{
            in_quote = 0;
            p++;
        } 
		else 
		{
            token[i++] = *p++;
        }
    }

    if (i > 0) 
	{
        token[i] = '\0';
        argv[argc++] = strdup(token);
    }

    argv[argc] = NULL;
    return argc;
}

int run_command_parsed(const char *cmd_str) 
{
    char *argv[MAX_ARGS];
    int argc = parse_args(cmd_str, argv);
    if (argc == 0) 
    {
        fprintf(stderr, "No command to run.\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) 
	{
        // Child process
        execvp(argv[0], argv);
        perror("execvp failed");
        _exit(127);
    } 
	else if (pid < 0) 
	{
        perror("fork failed");
        return -1;
    }

    // Parent process
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);  // non-blocking wait

    int ret;
    if (result == 0) 
    {
        // Child still running (non-blocking wait)
        ret = 0;  // Success in launching, but still running
    } 
    else if (result == pid) 
    {
        // Child exited quickly
        ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } 
    else 
    {
        perror("waitpid failed");
        ret = -1;
    }

    // Cleanup
    for (int i = 0; i < argc; i++) 
    {
        free(argv[i]);
    }

    return ret;
}

char* get_current_app_dir() {
    static char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count == -1) return NULL;
    
    path[count] = '\0';
    return dirname(path); // Returns the directory containing the app
}

int run_safe_command_parsed(const char* appname, const char *arguments, char *const safe_params[]) 
{
    char* app_dir = get_current_app_dir();
    // GUARD: Ensure appname is valid
    if (appname == NULL) 
    {
        fprintf(stderr, "Error: No app specified.\n");
        return -1;
    }

    // Static buffer avoids ISO C++ warning for string constant conversion
    static char default_path[] = "PATH=/usr/bin:/bin";

    pid_t pid = fork();
    if (pid < 0) 
    {
        return -1; 
    }

    int argc = 0;
    char *argv[MAX_ARGS];

    if (pid == 0) 
    { 
        char full_app_path[PATH_MAX];

        // --- CHILD PROCESS ---
        char *envp[MAX_ENV];
        int i = 0, j = 0;

        // 1. ARGUMENT PARSING
        //argv[i++] = (char *)appname; 
        if (arguments != NULL) 
        {
            argc = parse_args(arguments, argv);
            if (argc == 0) 
            {
                fprintf(stderr, "No command to run.\n");
            }
            i = argc;
        }
        argv[i] = NULL;



        // 2. ENVIRONMENT CONSTRUCTION
        if (safe_params != NULL) 
        {
            while (safe_params[j] != NULL && j < MAX_ENV - 2) 
            {
                envp[j] = safe_params[j];
                j++;
            }
        }
        envp[j++] = default_path;
        envp[j] = NULL;

        // 2. Change the working directory to the app's directory
        if (chdir(app_dir) != 0) 
        {
            fprintf(stderr, "Error: Unable to enter: %s\n", app_dir);
            _exit(EXIT_FAILURE);
        }

        if (app_dir) 
        {
            // Combine directory and appname into one absolute string
            snprintf(full_app_path, sizeof(full_app_path), "%s/%s", app_dir, appname);

            printf("Activating:: %s\n", full_app_path);                        
            execve(full_app_path, argv, envp);
        }
        else
        {        
            printf("Activating:: %s\n", appname);
            execve(appname, argv, envp);
        }

        // This only runs if execve fails (e.g. file not found/permissions)
        _exit(EXIT_FAILURE); 
    } 

    // Parent process
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);  // non-blocking wait

    int ret = -1;
    if (result == 0) 
    {
        // Child still running (non-blocking wait)
        ret = 0;  // Success in launching, but still running
    } 
    else if (result == pid) 
    {
        // Child exited quickly
        ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } 
    else 
    {
        perror("waitpid failed");
        ret = -1;
    }

    // Cleanup
    for (int i = 0; i < argc; i++) 
    {
        free(argv[i]);
    }

    return ret;
    /*
    else 
    { 
        // --- PARENT PROCESS ---
        int status;
        // WNOHANG makes the call non-blocking
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == 0) 
        {
            // Child is still running (most likely case for successful launch)
            return 0; 
        } 
        else if (result == pid) 
        {
            // Child failed immediately (the _exit(EXIT_FAILURE) was triggered)
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        } 
        else 
        {
            // Error in waitpid itself
            return -1;
        }
    }
    */
}