#define _GNU_SOURCE
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#define PATH "PATH OF FOLDER/archive.txt"
#define SHPATH "PATH OF FOLDER/sound.sh"

const char *sysname = "SHELL";

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};
/*Executes specific voice for commands*/
void play_sound(char *string)
{
	int pid_sound = fork();
	if (pid_sound < 0)
		printf("----Unable to fork---- \n");
	else if (pid_sound == 0)
	{
		execlp(SHPATH, "sound.sh", string, NULL);
		printf("-----Unable to execute-----\n");
	}
	else
	{
		wait(NULL);
	}
}

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command, int input)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	//FIXME: backspace is applied before printing chars
	if (input == 0)
		show_prompt();
	int multicode_state = 0;
	buf[0] = 0;
	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}
int process_command(struct command_t *command);
int main()
{
	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command, 0);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0)
		return EXIT;

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			FILE *fPtr;

			r = chdir(command->args[0]);

			/*Append current directory to archive.txt*/
			fPtr = fopen(PATH, "a");
			char cwd[255];
			if (getcwd(cwd, sizeof(cwd)) == NULL)
			{
				printf("Error getting current working directory\n");
				return 1;
			}
			/*Create an alias for the current working directory*/
			char *token = strrchr(cwd, '/'); //Returns a pointer to the last occurrence of character in the C string str. In our case, current folder.
			token++;						 // token is something like /Comp304 , by adding 1 we escape '/'

			fputs(token, fPtr);
			fputs(" ", fPtr);
			fputs(cwd, fPtr);
			fputs("\n", fPtr);
			fclose(fPtr);

			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	// increase args size by 2
	command->args = (char **)realloc(
		command->args, sizeof(char *) * (command->arg_count += 2));

	// shift everything forward by 1
	for (int i = command->arg_count - 2; i > 0; --i)
	{
		command->args[i] = command->args[i - 1];
	}
	// set args[0] as a copy of name
	command->args[0] = strdup(command->name);
	// set args[arg_count-1] (last) to NULL
	command->args[command->arg_count - 1] = NULL;

	// TODO: your implementation here
	if (strcmp(command->name, "tp") == 0)
	{
		if (command->arg_count > 0)
		{
			FILE *fPtr;

			/*Teleporting to a directory, e.g. tp comp304 */
			if (command->arg_count == 3)
			{
				/*If argument is list, without any word specification, just print all alias-path combinarions*/
				if (strcmp(command->args[1], "-l") == 0 || strcmp(command->args[1], "-list") == 0)
				{
					/*Open file in read mode*/
					fPtr = fopen(PATH, "r");

					char path_from_alias[255];

					/*Search for the word*/
					char line[512];

					int found = 0;
					int index = 1;
					while (fgets(line, sizeof(line), fPtr))
					{
						char *token;
						token = strtok(line, " \n\r");
						token = strtok(NULL, "\n\r");
						token = strchr(token, '/');
						printf("%dth:\t%s\n", index, token);
						index++;
						found = 1;
					}
					if (!found)
					{
						printf("----------------------------------There is no \"%s\" in the history.----------------------------------\n", command->args[1]);
						play_sound("noHistory");
						return SUCCESS;
					}

					fflush(fPtr);
					fclose(fPtr);
					return SUCCESS;
				}
				/*Open file in read mode*/
				fPtr = fopen(PATH, "r");

				/*If aliases contains the word, get from alias.*/
				/*If word is comp304 and our aliases do not contain comp304 but we have a path /home/comp304/project , this path should be used. But priority is on the aliases. */
				char path_from_alias[255];
				char path_from_line[255];
				/*Search for the word*/
				char line[512];

				int found_from_alias = 0; //If we find folder among aliases, we use that path. Prior
				int found_from_path = 0;  // If our pattern is not in aliases but in paths, we use that path.

				while (fgets(line, sizeof(line), fPtr))
				{
					char *token;
					token = strtok(line, " \n\r");
					if (strcasestr(token, command->args[1]))
					{
						//printf("Found inside alias \n");
						token = strtok(NULL, "\n\r");
						token = strchr(token, '/');
						strcpy(path_from_alias, token);
						found_from_alias = 1;
						break;
					}
					else
					{
						token = strtok(NULL, "\n\r");
						if (strcasestr(token, command->args[1]))
						{
							//printf("Found inside path \n");
							token = strchr(token, '/');
							strcpy(path_from_line, token);
							found_from_path = 1;
						}
					}
				}
				if (!found_from_path && !found_from_alias)
				{
					printf("----------------------------There is no \"%s\" in the history.-------------------------\n", command->args[1]);
					play_sound("noHistory");
					return SUCCESS;
				}
				else
				{
					if (found_from_alias)
					{
						char *path_token = strtok(path_from_alias, "\n\r");
						int r = chdir(path_token);
						if (r == -1)
						{
							printf("------------------------Unsuccesful directory change to %s ---------------------\n", path_from_alias);
							play_sound("unsuccesfulDirectory");
							return EXIT;
						}
						//printf("Alias has been used \n");
					}
					else if (found_from_path)
					{
						char *path_token = strtok(path_from_line, "\n\r");
						int r = chdir(path_token);
						if (r == -1)
						{
							printf("-------------------Unsuccesful directory change to------------------ \n%s \n", path_from_line);
							play_sound("unsuccesfulDirectory");
							return EXIT;
						}
						//printf("Path has been used\n");
					}
					play_sound("succesful");
					printf("-------------------Directory has been changed succesfully-------------------- \n");
				}

				fflush(fPtr);
				fclose(fPtr);
				return SUCCESS;
			}

			else if (command->arg_count > 3 && strcmp(command->args[2], "-l") == 0 || strcmp(command->args[2], "-list") == 0)
			{
				/*Open file in read mode*/
				fPtr = fopen(PATH, "r");

				int found_from_alias = 0; //If we find folder among aliases, we use that path. Prior
				int found_from_path = 0;  // If our pattern is not in aliases but in paths, we use that path.
				int index = 1;			  //in order to determine index of each occurence

				/*Search for the word*/
				char line[512];
				while (fgets(line, sizeof(line), fPtr))
				{
					char *token;
					token = strtok(line, " \n\r");
					if (strcasestr(token, command->args[1]))
					{
						//printf("Found inside alias \n");
						token = strtok(NULL, "\n\r");
						token = strchr(token, '/');
						printf("%dth:\t%s\n", index, token);
						index++;
						found_from_alias = 1;
					}
					else
					{
						token = strtok(NULL, "\n\r");
						token = strchr(token, '/');

						if (strcasestr(token, command->args[1]))
						{
							//printf("Found inside path \n");
							printf("%dth:\t%s\n", index, token);
							index++;
							found_from_path = 1;
						}
					}
				}
				if (!found_from_path && !found_from_alias)
				{
					printf("---------------There is no \"%s\" in the history.----------------\n", command->args[1]);
					play_sound("noHistory");
					return SUCCESS;
				}

				fflush(fPtr);
				fclose(fPtr);
				return SUCCESS;
			}
			/*Goes to a directory using an index from the list i.e. tp comp304 -3 */
			else if (command->arg_count > 3 && isdigit(command->args[2][1]))
			{
				/*Open file in read mode*/
				fPtr = fopen(PATH, "r");

				char *tokenize = command->args[2];
				tokenize++; //pointer arithmetic. will neglect '-'

				int index_input = atoi(tokenize);

				int index = 1; //iterator, counts the occurences until index_input

				/*Search for the word*/
				char line[512];
				while (fgets(line, sizeof(line), fPtr))
				{
					char *token;
					token = strtok(line, " \n\r");
					if (strcasestr(token, command->args[1]))
					{
						token = strtok(NULL, "\n\r");
						token = strchr(token, '/'); //points to the starting point of the path
						if (index == index_input)
						{
							char *path_token = strtok(token, "\n\r");
							int r = chdir(path_token);
							if (r == -1)
							{
								printf("-------------Unsuccesful directory change to %s -------------\n", path_token);
								play_sound("unsuccesfulDirectory");
								return EXIT;
							}
							printf("-----------Directory sucesfully changed.----------- \n");
							play_sound("succesful");
							return SUCCESS;
						}
						index++;
					}
					else
					{
						token = strtok(NULL, "\n\r");
						token = strchr(token, '/');

						if (strcasestr(token, command->args[1]))
						{
							if (index == index_input)
							{
								char *path_token = strtok(token, "\n\r");
								int r = chdir(path_token);
								if (r == -1)
								{
									printf("------------Unsuccesful directory change to %s ---------------\n", path_token);
									play_sound("unsuccesfulDirectory");
									return EXIT;
								}
								printf("-----------Directory sucesfully changed.----------- \n");
								play_sound("succesful");
								return SUCCESS;
							}
							index++;
						}
					}
				}

				printf("There is no \"%s\" with index %d in the history.\n", command->args[1], index);
				play_sound("noHistory");
				fflush(fPtr);
				fclose(fPtr);
				return SUCCESS;
			}

			return SUCCESS;
		}
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	play_sound("notFound");
	return UNKNOWN;
}
