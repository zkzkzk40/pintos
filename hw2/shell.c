#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);
/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;
struct zk_token
{
    /* data */
    int tokenLen;
    int nextToken;
    char **args;
};
void parse_args(struct zk_token *ch, struct tokens *tokens)
{
	char *token;
	while (ch->nextToken < ch->nextToken) {
		token = tokens_get_token(tokens, ch->nextToken);
		ch->args[ch->nextToken++] = token;
	}
	ch->args[ch->nextToken] = NULL;
}
/* start a child process to execute program */
int run_program(struct tokens *tokens)
{
	int tokens_len = tokens_get_length(tokens);
	if (tokens_len == 0)	/* no input */
		exit(0);

	char *args[tokens_len + 1];
	struct zk_token  child = { 0 };
	child.tokenLen = tokens_len;
	child.nextToken = 0;
	child.args = args;

	parse_args(&child, tokens);

	pid_t chpid = fork();
	if (chpid < 0) {	/* fork error */
		shell_msg("fork : %s\n", strerror(errno));
		return -1;
	} else if (chpid == 0) {
		execv(args[0], args);
	}
	if (wait(NULL) == -1) {	/* wait until child process done */
		shell_msg("wait: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}
fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print file path"},
    {cmd_cd,"cd","changes the current working directory to new directory."}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* 输出工作当前路径*/
int cmd_pwd(struct tokens* tokens){
    char buffer[100];
    getcwd(buffer, sizeof(buffer));
    printf("The current directory is: %s\n", buffer);
    return 2;
}
/*改变当前工作路径*/
int cmd_cd(struct tokens* tokens){
  int i=-1;
  switch (tokens_get_length(tokens))
  {
  case 1:
    chdir ("/home");
    printf("change work file to /home\n");
    break;
  case 2:
    i = chdir (tokens_get_token(tokens,1));
    if(i==-1){
      printf("no such file!\n");
      return -1;
    }
    break;
  default:
    printf("too many input!\n");
    break;
  }
  return 3;
}
/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}
struct pathProgress{
    int token_len;
    int token_next;
    char** arg;
};
void programExecution(struct tokens *tokens){

}
int main(unused int argc, unused char* argv[]) {
  init_shell();
  
  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
        programExecution(tokens);
//      /* REPLACE this to run commands as programs. */
//      fprintf(stdout, "This shell doesn't know how to run programs.\n");
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
