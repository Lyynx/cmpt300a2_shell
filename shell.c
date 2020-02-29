#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define HISTORY_DEPTH 10
#define NUM_CMDS 4

// Global Variables
char history[HISTORY_DEPTH][COMMAND_LENGTH];
char previous_dir[COMMAND_LENGTH];

/**
 * Command Input and Processing
 */

/*
 * Tokenize the string in 'buff' into 'tokens'.
 * buff: Character array containing string to tokenize.
 *       Will be modified: all whitespace replaced with '\0'
 * tokens: array of pointers of size at least COMMAND_LENGTH/2 + 1.
 *       Will be modified so tokens[i] points to the i'th token
 *       in the string buff. All returned tokens will be non-empty.
 *       NOTE: pointers in tokens[] will all point into buff!
 *       Ends with a null pointer.
 * returns: number of tokens.
 */
int tokenize_command(char *buff, char *tokens[])
{
	int token_count = 0;
	_Bool in_token = false;
	int num_chars = strnlen(buff, COMMAND_LENGTH);
	for (int i = 0; i < num_chars; i++) {
		switch (buff[i]) {
		// Handle token delimiters (ends):
		case ' ':
		case '\t':
		case '\n':
			buff[i] = '\0';
			in_token = false;
			break;

		// Handle other characters (may be start)
		default:
			if (!in_token) {
				tokens[token_count] = &buff[i];
				token_count++;
				in_token = true;
			}
		}
	}
	tokens[token_count] = NULL;
	return token_count;
}

/**
 * Read a command from the keyboard into the buffer 'buff' and tokenize it
 * such that 'tokens[i]' points into 'buff' to the i'th token in the command.
 * buff: Buffer allocated by the calling code. Must be at least
 *       COMMAND_LENGTH bytes long.
 * tokens[]: Array of character pointers which point into 'buff'. Must be at
 *       least NUM_TOKENS long. Will strip out up to one final '&' token.
 *       tokens will be NULL terminated (a NULL pointer indicates end of tokens).
 * in_background: pointer to a boolean variable. Set to true if user entered
 *       an & as their last token; otherwise set to false.
 */
int read_command(char *buff, char *tokens[], _Bool *in_background)
{
	*in_background = false;

	// Read input
	int length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);

	while( ( length < 0 ) && ( errno == EINTR ) ){ // While read-in is interrupted by signal, retry read-in
		length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);
	}

	if( length < 0 ){
		perror("Unable to read command from keyboard. Terminating.\n");
		exit(-1); /* terminate with error */
	}

	// Null terminate and strip \n.
	buff[length] = '\0';
	if (buff[strlen(buff) - 1] == '\n') {
		buff[strlen(buff) - 1] = '\0';
	}

	// Tokenize (saving original command string)
	int token_count = tokenize_command(buff, tokens);
	if (token_count == 0) {
		return 0;
	}

	// Extract if running in background:
	if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
		*in_background = true;
		tokens[token_count - 1] = 0;
		token_count--;
	}

	return token_count;
}

void exit_cmd( int token_count ){
	if( token_count > 1 ){ // If exit command has arguments
		write( STDOUT_FILENO, "Error: exit does not accept any arguments", strlen("Error: exit does not accept any arguments") );
		write( STDOUT_FILENO, "\n", strlen("\n"));
	} else{
		write(STDOUT_FILENO, "Exiting...", strlen("Exiting..."));
		write(STDOUT_FILENO, "\n", strlen("\n"));
		exit( 0 );
	}

	return;
}

void pwd_cmd( int token_count ){
	if( token_count > 1 ){ // If pwd command has arguments
		write( STDOUT_FILENO, "Error: pwd does not accept any arguments", strlen("Error: pwd does not accept any arguments") );
		write( STDOUT_FILENO, "\n", strlen("\n"));
	} else{

		char cwd[COMMAND_LENGTH];
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			write(STDOUT_FILENO, cwd, strlen(cwd));
			write( STDOUT_FILENO, "\n", strlen("\n"));
		} else {
			write(STDOUT_FILENO, "Error: getcwd() error", strlen("Error: getcwd() error"));
			write( STDOUT_FILENO, "\n", strlen("\n"));
			return;
		}
	}

	return;
}

void save_cwd( char *cwd ){
	char str[COMMAND_LENGTH];
	if (getcwd(str, sizeof(str)) != NULL) {
		strcpy( cwd, str );

	} else {
		write(STDOUT_FILENO, "Error: save_cwd() error", strlen("Error: save_cwd() error"));
		return;
	}

	return;
}

void get_home( char *dir_str ){ // https://stackoverflow.com/questions/2910377/get-home-directory-in-linux
	struct passwd *pw = getpwuid( getuid() );
	const char *home_dir = pw->pw_dir;

	strcpy( dir_str, home_dir );
}

void cd_cmd( int token_count, char *tokens[] ){
	char dir_str[COMMAND_LENGTH];
	char cwd[COMMAND_LENGTH];

	save_cwd( cwd );

	if( token_count == 1 ){ // If no arguments entered, target dir is /home
		get_home( dir_str );

	} else if( token_count != 2 ){ // If cd command has unexpected number of arguments, print error
		write( STDOUT_FILENO, "Error: cd only accepts 0 or 1 arguments", strlen("Error: cd only accepts 0 or 1 arguments") );
		write( STDOUT_FILENO, "\n", strlen("\n"));
		return;

	} else if( strcmp(tokens[1], "-" ) == 0 ){ // Check to see if argument is -
		if( previous_dir == NULL ){ // If no previous directory, print error
			write( STDOUT_FILENO, "Error: No previous directory", strlen("Error: No previous directory") );
			write( STDOUT_FILENO, "\n", strlen("\n"));
			return;

		} else{ // If there is a previous directory, target directory is previous directory
			strcpy( dir_str, previous_dir );
		}

	} else if( strcmp(tokens[1], "~" ) == 0 ){ // Check to see if argument is ~, target directory is /home
		get_home( dir_str );
		
	} else if( tokens[1][0] == '~' ){ // Check to see if argument uses ~, target directory starts with /home
		get_home( dir_str );
		strcat( dir_str, tokens[1] + 1 );

	} else{ // Otherwise target directory is first argument
		strcpy( dir_str, tokens[1] );

	}

	if( chdir( dir_str ) == 0 ) { // Change to target directory
		write(STDOUT_FILENO, "Directory changed to \'", strlen("Directory changed to \'"));
		write(STDOUT_FILENO, dir_str, strlen(dir_str));
		write(STDOUT_FILENO, "\'", strlen("\'"));
		write(STDOUT_FILENO, "\n", strlen("\n"));

		strcpy( previous_dir, cwd ); // Set previous directory to cwd when changing directory

	} else{ // If change directory fails, print error
		write(STDOUT_FILENO, "Error: Directory \'", strlen("Error: Directory \'"));
		write(STDOUT_FILENO, dir_str, strlen(dir_str));
		write(STDOUT_FILENO, "\' does not exist", strlen("\' does not exist"));
		write( STDOUT_FILENO, "\n", strlen("\n"));
	}

	return;
}

void help_cmd( int token_count, char *tokens[] ){
	char *help_array[NUM_CMDS];

	char exit_help[] = "\'exit\'\tis a builtin command which exits the Linux shell";
	char pwd_help[] = "\'pwd\'\tis a builtin command which displays the current working directory";
	char cd_help[] = "\'cd\'\tis a builtin command which changes the current working directory";
	char help_help[] = "\'help\'\tis a builtin command which display information about the internal commands";

	help_array[0] = exit_help;
	help_array[1] = pwd_help;
	help_array[2] = cd_help;
	help_array[3] = help_help;

	char ext_help[] = "\' is an external command or application";


	if( token_count > 2 ){ // If help command has more than 1 argument
		write( STDOUT_FILENO, "Error: help accepts only 0 or 1 arguments", strlen("Error: help accepts only 0 or 1 arguments") );
		write( STDOUT_FILENO, "\n", strlen("\n"));
	} else if( token_count > 1 ){ // If help command has 1 argument
		if( strcmp(tokens[1], "exit") == 0 ){
			write(STDOUT_FILENO, exit_help, strlen(exit_help));
			write(STDOUT_FILENO, "\n", strlen("\n"));

		} else if( strcmp(tokens[1], "pwd") == 0 ){
			write(STDOUT_FILENO, pwd_help, strlen(pwd_help));
			write(STDOUT_FILENO, "\n", strlen("\n"));
			
		} else if( strcmp(tokens[1], "cd") == 0 ){
			write(STDOUT_FILENO, cd_help, strlen(cd_help));
			write(STDOUT_FILENO, "\n", strlen("\n"));
			
		} else if( strcmp(tokens[1], "help") == 0 ){
			write(STDOUT_FILENO, help_help, strlen(help_help));
			write(STDOUT_FILENO, "\n", strlen("\n"));
			
		} else{
			write(STDOUT_FILENO, "\'", strlen("\'"));
			write(STDOUT_FILENO, tokens[1], strlen(tokens[1]));
			write(STDOUT_FILENO, ext_help, strlen(ext_help));
			write(STDOUT_FILENO, "\n", strlen("\n"));
		}
		
	} else{ // If help command has 0 arguments
		for (int i = 0; i < NUM_CMDS; ++i)
		{
			write(STDOUT_FILENO, help_array[i], strlen(help_array[i]));
			write(STDOUT_FILENO, "\n", strlen("\n"));
		}
	}

	return;
}

void history_cmd( int token_count, int history_count ){
	if( token_count > 1 ){ // If history command has arguments
		write( STDOUT_FILENO, "Error: history does not accept any arguments", strlen("Error: history does not accept any arguments") );
		write( STDOUT_FILENO, "\n", strlen("\n"));
	} else{

		char count_str[COMMAND_LENGTH]; // Change to different number later?
		for( int i = 0; i < HISTORY_DEPTH && (history_count >= 0); i++ ){
			sprintf( count_str, "%d", history_count );

			write( STDOUT_FILENO, count_str, strlen(count_str));
			write( STDOUT_FILENO, "\t", strlen("\t"));
			write( STDOUT_FILENO, history[(history_count % HISTORY_DEPTH)], strlen(history[(history_count % HISTORY_DEPTH)]));
			write( STDOUT_FILENO, "\n", strlen("\n"));

			history_count--;

		}
	}
	return;
}

void add_to_history( int *history_count, char *tokens[], const _Bool in_background ){
	char str[COMMAND_LENGTH];
	strcpy( str, tokens[0] );

	for( int i = 1; tokens[i] != NULL; i++ ){
		strcat( str, " " );
		strcat( str, tokens[i] );
	}

	if( in_background ){
		strcat( str, " &" );
	}

	strcpy( history[ *history_count % HISTORY_DEPTH ], str );

	*history_count = *history_count + 1;

	return;
}

void retrieve_cmd( int cmd_index, char *buff, _Bool *in_background, int *token_count, char *tokens[] ){
	strcpy( buff, history[ cmd_index % HISTORY_DEPTH ] ); // Copy cmd from history to buff

	// Tokenize (saving original command string)
	*token_count = tokenize_command( buff, tokens );
	if( *token_count == 0 ){
		write(STDOUT_FILENO, "Error: Command not retrieved", strlen("Error: Command not retrieved"));
		write(STDOUT_FILENO, "\n", strlen("\n"));
	}

	// Extract if running in background:
	if( *token_count > 0 && strcmp( tokens[ *token_count - 1 ], "&" ) == 0 ){
		*in_background = true;
		tokens[ *token_count - 1 ] = 0;
		*token_count = *token_count - 1;
	}

	return;
}

int is_digits( char *num ){
	for( int i = 0; num[i] != '\0'; i++ ){
		if( !isdigit( num[i] ) ){
			return 0;
		}
	}

	return 1;
}

void handle_SIGINT(){
	char *empty[0];
	
	write(STDOUT_FILENO, "\n", strlen("\n"));
	help_cmd( 1, empty );

	char cwd[COMMAND_LENGTH];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		write(STDOUT_FILENO, cwd, strlen(cwd));
	} else {
		write(STDOUT_FILENO, "Error: getcwd() error", strlen("Error: getcwd() error"));
		return;
	}

	write(STDOUT_FILENO, "$ ", strlen("$ "));

}

void history_select( char *num, int *token_count, int *history_count, char *input_buffer, _Bool *in_background, char *tokens[] ){
	if( !is_digits( num ) || strcmp( num, "" ) == 0 ){ // Input is not a digit or is empty
		write(STDOUT_FILENO, "Error: ! must be immediately followed by a number", strlen("Error: ! must be immediately followed by a number"));
		write(STDOUT_FILENO, "\n", strlen("\n"));

	} else if( *history_count == 0 ){ // No history
		write(STDOUT_FILENO, "Error: No previous command", strlen("Error: No previous command"));
		write(STDOUT_FILENO, "\n", strlen("\n"));

	} else{
		int cmd_index = atoi( num );
		if( ( cmd_index < *history_count ) && ( cmd_index >= *history_count - HISTORY_DEPTH ) ){ // Index within history range
			retrieve_cmd( cmd_index, input_buffer, in_background, token_count, tokens );

		} else{ // Index not within history range
			write(STDOUT_FILENO, "Error: History index not found", strlen("Error: History index not found"));
			write(STDOUT_FILENO, "\n", strlen("\n"));

		}
	}
}

/**
 * Main and Execute Commands
 */
int main(int argc, char* argv[]){
	char input_buffer[COMMAND_LENGTH];
	char *tokens[NUM_TOKENS];
	int history_count = 0;
	int token_count = 0;

	struct sigaction handler;
	handler.sa_handler = handle_SIGINT;
	handler.sa_flags = 0;
	sigemptyset( &handler.sa_mask );
	sigaction( SIGINT, &handler, NULL );

	while( true ){
		// Get command
		// Display current working directory
		char cwd[COMMAND_LENGTH];
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			write(STDOUT_FILENO, cwd, strlen(cwd));
		} else {
			write(STDOUT_FILENO, "Error: getcwd() error", strlen("Error: getcwd() error"));
			return 1;
		}

		write(STDOUT_FILENO, "$ ", strlen("$ "));

		_Bool in_background = false;
		token_count = read_command(input_buffer, tokens, &in_background);

		if( token_count > 0 ){ // If there is at least 1 token
			if( ( tokens[0][0] == '!' ) && ( tokens[0][1] != '!' ) ){ // Check if first token is !x
				history_select( tokens[0] + 1 , &token_count, &history_count, input_buffer, &in_background, tokens );
				
			} else if( strcmp( tokens[0], "!!" ) == 0 ){ // Check if first token is !!
				if( history_count == 0 ){ // No history
					write(STDOUT_FILENO, "Error: No previous command", strlen("Error: No previous command"));
					write(STDOUT_FILENO, "\n", strlen("\n"));

				} else{
					retrieve_cmd( ( history_count - 1 ), input_buffer, &in_background, &token_count, tokens );

				}
			}

			// DEBUG: Dump out arguments:
			// for (int i = 0; tokens[i] != NULL; i++) {
			// 	write(STDOUT_FILENO, "   Token: ", strlen("   Token: "));
			// 	write(STDOUT_FILENO, tokens[i], strlen(tokens[i]));
			// 	write(STDOUT_FILENO, "\n", strlen("\n"));
			// }

			if (in_background) {
				write(STDOUT_FILENO, "Running in background...", strlen("Running in background..."));
				write(STDOUT_FILENO, "\n", strlen("\n"));
			}

			add_to_history( &history_count, tokens, in_background ); // COMMAND SAVED TO HISTORY

			if( strcmp( tokens[0], "exit" ) == 0 ){ // Check if first token is exit
				exit_cmd( token_count );

			} else if( strcmp( tokens[0], "pwd" ) == 0 ){
				pwd_cmd( token_count );

			} else if( strcmp( tokens[0], "cd" ) == 0 ){
				cd_cmd( token_count, tokens );

			} else if( strcmp( tokens[0], "help" ) == 0 ){
				help_cmd( token_count, tokens );

			} else if( strcmp( tokens[0], "history" ) == 0 ){
				history_cmd( token_count, (history_count - 1) );

			} else{ // Create fork to execute external cmd
				pid_t var_pid;
				int status;
				var_pid = fork(); //Returns process id of the child process

				if( var_pid < 0 ){
					write( STDOUT_FILENO, "Process Fork Failed", strlen("Process Fork Failed") );
					write(STDOUT_FILENO, "\n", strlen("\n"));
					exit( 1 );
				} else if( var_pid == 0 ){ // Child process
					if( execvp( tokens[0], tokens ) < 0 ){
						write( STDOUT_FILENO, "Execution Failed", strlen("Execution Failed") );
						write(STDOUT_FILENO, "\n", strlen("\n"));
						exit( 1 );
					}
				} else{ // Parent process
					if( !in_background ){ // Wait for process
						waitpid( var_pid, &status, 0 );
					}
					// Else clean up and loop pack to read_command
				}

				// Clean up any previously exited background child processes (zombies)
				while (waitpid(-1, NULL, WNOHANG) > 0); // do nothing
			}
		}
	}

	return 0;
}