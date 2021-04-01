#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#define PATH "PATH OF FOLDER" //It is the path where shortdir.txt is placed

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
int prompt(struct command_t *command,int input)
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
	if(input==0)show_prompt();
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
		code = prompt(command,0);
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
			r = chdir(command->args[0]);
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
	if (strcmp(command->name, "shortdir") == 0)
	{
		/*Current Working Directory will be used frequently*/
		char cwd[255];
		if (getcwd(cwd, sizeof(cwd)) == NULL)
		{
			printf("Error getting current working directory\n");
			return 1;
		}
		/*Since our terminal can go across folders, we need a definite place for our text file*/
		char txtLocation[255];
		strcpy(txtLocation, PATH);
		strcat(txtLocation, "/shortdir.txt");

		if (command->arg_count > 0)
		{
			FILE *fPtr;

			/*Append alias with its path to text file */
			if (strcmp(command->args[1], "set") == 0)
			{
				/*Control | Notify | Delete the duplicate alias if exists*/
				char temp_location[255];
				strcpy(temp_location, PATH);
				strcat(temp_location, "/shortdirtemp.txt");
				FILE *temp_file = fopen(temp_location, "wb");
				fPtr = fopen(txtLocation, "r+");
				int instance_found = 0;

				char get_lines[255];
				char copy_line[255]; //strtok messes up get_lines so it is temp variable
				while (fgets(get_lines, sizeof(get_lines), fPtr) != NULL)
				{
					strcpy(copy_line, get_lines);
					char *split_lines;
					split_lines = strtok(get_lines, " ");

					if (strcmp(split_lines, command->args[2]) != 0)
					{
						fprintf(temp_file, "%s", copy_line);
					}
					else
					{

						instance_found++;
						split_lines = strtok(NULL, "\n\r");
						/*Ask user whether they want to change the existing alias.*/
						if (instance_found == 1)
						{
							printf("Type yes if you want to change the existing alias:\nAlias:%s\tPath:%s\n", command->args[2], split_lines);
							char answer[15];
							struct command_t *command_2 = malloc(sizeof(struct command_t));
							memset(command_2, 0, sizeof(struct command_t)); // set all bytes to 0

							int code;
							code = prompt(command_2,1); //we don't want host name to be prompted again "1" satisfies this purpose
							if (code == EXIT)
								break;

							if(strcasecmp(command_2->name,"yes")!=0)
							{
								printf("No changes has been made \n");
								fclose(fPtr);
								fclose(temp_file);
								return SUCCESS;
							}
							else
							{
								printf("Alias has been changed \n");
							}
							
						}
					}
				}

				fclose(fPtr);
				fclose(temp_file);

				/*Replace old file with the new one*/
				remove(txtLocation);
				rename(temp_location, txtLocation);

				/*Set Alias Part*/

				/*Open file in append mode*/
				fPtr = fopen(txtLocation, "a");

				/* Create the file if it does not exist */
				if (fPtr == NULL)
				{
					printf("Shortdir file is created\n"); //debug purpose
					fPtr = fopen(txtLocation, "wb");
				}

				if (fputs(command->args[2], fPtr) == -1)
				{
					printf("Error appending alias");
					return 1;
				}
				if (fputs(" ", fPtr) == -1)
				{
					printf("Error appending blank space");
					return 1;
				}
				if (fputs(cwd, fPtr) == -1)
				{
					printf("Error appending current directory");
					return 1;
				}
				if (fputs("\n", fPtr) == -1)
				{
					printf("Error appending current new line");
					return 1;
				}
				printf("%s is set as an alias for %s \n", command->args[2], cwd);
				fflush(fPtr);
				fclose(fPtr);
			}
			
			
			/*Go to directory with the given alias */
			else if (strcmp(command->args[1], "jump") == 0)
			{
				/*Open file in read mode*/
				fPtr = fopen(txtLocation, "r+");
				char get_lines[255];
				/*Get lines of the text file*/
				while (fgets(get_lines, sizeof(get_lines), fPtr) != NULL)
				{
					/*Split lines : alias | path*/
					char *split_lines;
					split_lines = strtok(get_lines, " ");
					/*If alias found, split again to the end of line to obtain path, then change directory*/
					if (strcmp(split_lines, command->args[2]) == 0)
					{
						split_lines = strtok(NULL, "\n\r");
						if (chdir(split_lines) == -1)
							printf("Cannot go to short directory\n");
						fclose(fPtr);
						return SUCCESS;
					}
				}
				printf("Could not find a short directory with %s alias\n", command->args[2]);
				fclose(fPtr);
			}

			else if (strcmp(command->args[1], "del") == 0)
			{
				char temp_location[255];
				strcpy(temp_location, PATH);
				strcat(temp_location, "/shortdirtemp.txt");
				FILE *temp_file = fopen(temp_location, "wb");
				fPtr = fopen(txtLocation, "r+");
				int instance_found = 0;

				char get_lines[255];
				char copy_line[255]; //strtok messes up get_lines so it is temp variable
				while (fgets(get_lines, sizeof(get_lines), fPtr) != NULL)
				{
					strcpy(copy_line, get_lines);
					char *split_lines;
					split_lines = strtok(get_lines, " ");

					if (strcmp(split_lines, command->args[2]) != 0)
					{
						fprintf(temp_file, "%s", copy_line);
					}
					else
						instance_found++;
					split_lines = strtok(NULL, "\n\r");
				}
				if (instance_found == 0)
				{
					printf("Alias '%s' not found. \n", command->args[2]);
				}
				else
				{
					printf("Alias %s is deleted.\n", command->args[2]);
				}
				fclose(fPtr);
				fclose(temp_file);

				/*Replace old file with the new one*/
				remove(txtLocation);
				rename(temp_location, txtLocation);
			}
			else if (strcmp(command->args[1], "clear") == 0)
			{
				/*Using fopen with 'w' flag on existing file: deletes all contents inside the file*/
				fPtr = fopen(txtLocation, "w");
				if (fPtr != NULL)
					printf("%s\n", "All short directories are deleted");
				fclose(fPtr);
			}
			else if (strcmp(command->args[1], "list") == 0)
			{
				fPtr = fopen(txtLocation, "r");
				int line_count = 0;
				char get_lines[255];
				while (fgets(get_lines, sizeof(get_lines), fPtr) != NULL)
				{
					line_count++;
					char *split_lines;
					split_lines = strtok(get_lines, " ");
					printf("Alias: %s\t", split_lines);
					split_lines = strtok(NULL, "\n\r");
					printf("Path: %s\n", split_lines);
				}
				if (line_count == 0)
				{
					printf("\nThere is no alias to list.\n");
				}

				fclose(fPtr);
			}
			return SUCCESS;
		}
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}
