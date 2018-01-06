#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "./jobs.h"
#include "jobs.c"
#define INPUT_BUF_LEN 1024
pid_t jpid_shell;
job_list_t* job_list;
int jid = 1;

/*
 * This function takes in a string and writes
 * the string in the file corresponding to that file descriptor.
 * I use this function to write error messages in stderr. It then cleans
 * up the job list and exits with flag 1.
 * 
 * err_message - the message to be written in the file corresponding 
 * to the passed in fd.
 * return - nothing.
 */
void err_and_ex(char* err_message){
  fprintf(stderr, err_message);
  cleanup_job_list(job_list);
  exit(1);
}

/*
 * This function takes in 3 pointers to arrays storing strings. It 
 * stores strings contained in arg in either cmd_arg or redir_arg.
 * It stores a string (contained in arg) in cmd_arg if the string in the 
 * previous index of arg does not contain a redirection character (> or < or >>).
 * Conversely, it stores a string in redir_arg if it contains a (valid)
 * redirection character or if the string itself does not contain any redirection
 * character but the string in the previous index of arg does.
 * fd - file descriptor of the file err_message is to be written
 * onto
 * arg - pointer to an array of strings. It is from this array of strings
 * that the strings to be stored in cmd_arg or redir_arg are retrieved.
 * cmd_arg - pointer to an array of strings. It is this array strings from
 * arg are stored in if they are not redirection files.
 * redir_arg - pointer to an array of strings. It is this array strings from
 * arg are stored in if either they contain redirection characters or are 
 * redirection files.
 * returns 0 if user has entered a valid command, 1 if the user has entered an invalid 
 * command.
 */
int parse(char** arg, char** cmd_arg, char** redir_arg){
  // I use prev_direction_char as a way to distinguish between strings that are stored
  // right after strings containing redirection characters (redirection files) in cmd 
  // and those that are not.
  char prev_direction_char = 0;
  // A counter which I use to detect multiple instances of the input redirection character
  // in the same input command. This is illegal and must raise and error accordingly.
  int redirect_in = 0;
  // A counter which I use to detect multiple instances of output redirection characters
  // in the same input command. This is illegal and must raise and error accordingly.
  int redirect_out = 0;
  // The index at which I store strings in cmd_arg.
  int cmd_arg_i = 0;
  // The index at which I store strings in redir_arg.
  int redir_arg_i = 0;
  for (int i = 0; arg[i] != 0; i++){
    // Retrieving the string at the current index of arg.
    char* string = arg[i];
    // Retrieving the first character of the string at the current index of arg.
    char c = string[0];
    if (c == '>' || c == '<'){
      // If control flow reaches here, it means the current string is meant to be a redirection
      // token.
      if (!prev_direction_char){
        // If control flow reaches here, then it means there are no successive strings in arg
        // that are meant to be redirection tokens. This only guarantees that in the command 
        // entered by the user, there are no two redirection tokens with only whitespace in 
        // between.
        if (c == '<'){
          if (redirect_in){
            // If control flow reaches here, then it means one of the strings that came before 
            // the current one in the command line was also < like the current token, which is 
            // illegal.
            fprintf(stderr, "syntax error: multiple input files.\n");
            return 1;               
          }
          // If control flow reaches here, then it means the current string is the first instance 
          // of the < redirection token in the string, in which case the counter is incremented 
          // accordingly.
          redirect_in++;
          } else {
            if (redirect_out){
              // If control flow reaches here, then it means one of the strings that came before 
              // the current one in the command line was also > like the current token, which is 
              // illegal.
              fprintf(stderr, "syntax error: multiple output files.\n");
              return 1;               
            }
            // If control flow reaches here, then it means the current string is the first instance 
            // of the > redirection token in the string, in which case the counter is incremented 
            // accordingly.
            redirect_out++;
          } 
          if (string[1] == '\0'){
            // If control flow reaches here, then the current string consists of a single redirection 
            // character, either '>' or '<', and there is no previous instance of the same redirection 
            // token in the command line, in which case the prev_direction_char must be set equal to this 
            // character. Since that the flow reaches here necessarily means that the current string is 
            // valid, and a redirection token, it is added to redir_arg, whose index is incremented 
            // accordingly. 
            prev_direction_char = c;
            redir_arg[redir_arg_i] = string;
            redir_arg_i++;
          } else {
            // Indicates that the string consists of 2 redirection characters. The only case in which this 
            // is legal is the string ">>".
            if (c == '>' && string[1] == '<'){
              // String >< is illegal. Since > comes first in this case, I interpret this as the error 
              // that the user has failed to interpret an output file and print the following message 
              // to stdout.
              fprintf(stderr, "syntax error: no output file.\n");
              return 1;
            } else if (c == '<'){
              // Strings << and <> are illegal. Since < comes first, I interpret this as a failure 
              // by the user to specify an input file before the other token. I therefore print the
              // following message to stdout.
              fprintf(stderr, "syntax error: no input file.\n");
              return 1;
            } 
            if (string[2] == 0){
              // Control only reaches here if the user has entered the string ">>" and has not entered > 
              // before this string which is legal. In this case, I must set prev_direction_char equal to 
              // ">>", add the current string to redir_arg and increment redir_arg's index accordingly. 
              prev_direction_char = c;
              redir_arg[redir_arg_i] = string;
              redir_arg_i++;
            } else {
              // If control reaches here, the string consists of more than 2 redirection characters, 
              // the first 2 of which are both '>', which is illegal. I interpret this as a failure 
              // by the user to specify an output file since the first two chars are output redirection 
              // characters.
              fprintf(stderr, "syntax error: no output file.\n");
              return 1;
            }
          }
      } else {
        // If control flow reaches here, then it means there are no successive strings in arg
        // that are meant to be redirection tokens. This only guarantees that in the command 
        // entered by the user, there are no two redirection tokens with only whitespace in between.
        if (prev_direction_char == '>'){
          fprintf(stderr, "syntax error: no output file.\n");
          return 1;
        }
        fprintf(stderr, "syntax error: no input file.\n");
        return 1;
      }
    } else {
      // The current string does not contain a redirection character. In this case, I use 
      // prev_direction_char to see if the current word is a redirection file or not.
      if (!prev_direction_char){
        // If prev_direction_char is 0, then it means this word does not indicate a redirection 
        // file and therefore must be a command or an argument to a command, in which case it is 
        // added to cmd_arg, whose index is then incremented.
        cmd_arg[cmd_arg_i] = string;
        cmd_arg_i++;
      } else {
        // If prev_direction_char is not 0, then it means this word is a redirection file and 
        // therefore must be added to redir_arg, whose index is incremented. If our current word
        // is a redirection file, then prev_direction_char must be zeroed for the next words, if 
        // there are any. If the next word does not consist of redirection characters, then, we 
        // have necessarily reached a command, which needs prev_redirection_char to be zero to 
        // be correctly added to cmd_arg. If the next word is a redirection symbol, then it needs 
        // prev_redirection_char not to incorrectly print error messages and return.
        redir_arg[redir_arg_i] = string;
        redir_arg_i++;
        prev_direction_char = 0;
      }
    }
  }
  // If the control flow reaches here, then the user did not mess up their redirection symbols, or 
  // did not forget to specify a file for their first redirection symbol if they entered 2 redirection 
  // symbols. But the user might have forgot to specify a file for their last (if they have specified 2)
  // redirection symbol. We also still do not have any guarantee that the user has specified a command.

  // Adding null character to the cap the arrays off.
  redir_arg[redir_arg_i] = 0;
  cmd_arg[cmd_arg_i] = 0;
  // If prev_direction_char is still not null once we are out of our for loop, then it means the last 
  // non-whitespace character entered by the user was a redirection symbol, in which case the user has
  // failed to provide the necessary redirection file. 
  if (prev_direction_char == '>'){
    // Implies that the user did not specify an output file after > or >>.
    fprintf(stderr, "syntax error: no output file.\n");
    return 1;
  } else if (prev_direction_char == '<'){
    // Implies that the user did not specify an output file after <.
    fprintf(stderr, "syntax error: no input file.\n");
    return 1;
  }
  // Even if the user might not have made any errors in their redirection words/symbols, 
  // they might still have forgotten to enter a command, in which case I write a "no command"
  // error message to stdout.
  if (!cmd_arg_i){
    fprintf(stderr, "Error- no command.\n");
    return 1;
  }
  // Finally, if the parsing worked correctly, parse returns 0.
  return 0;
}
/*
 * This function takes in a pointer to an array of strings, specifically cmd_arg 
 * in the main. Compares the string contained in the first index of cmd_arg, which
 * is necessarily the command, with four built commands, and if there is a match, 
 * executes the command with the corresponding syscall. Also does error handling 
 * of these system calls.
 * 
 * returns 0 in 2 cases: case 1: if there is a match and the
 * corresponding system call is executed successfully. case 2: if there is a match,
 * but the corresponding system call cannot be executed successfully, perhaps due to
 * arguments, in which case we want our program to continue running. Returns 1 if there
 * is no match, because I want my run_cmd to be executed only if there entered command
 * is not a built-in command. (I call run_cmd inside a conditional into which flow enters)
 * only if run_built_in_cmd returns 1)
 */
int run_built_in_cmd(char** argv){
  if (!strcmp(argv[0], "cd")){
    if ((chdir(argv[1])) == -1){
      // Error handling chdir().
      fprintf(stderr, "cd: syntax error\n");
      return 0;
    }
    return 0;
  } else if (!strcmp(argv[0], "rm")){
    if ((unlink(argv[1])) == -1){
      // Error handling unlink().
      fprintf(stderr, "rm: syntax error\n");
      return 0;
    }
    return 0;
  } else if (!strcmp(argv[0], "ln")){
    if ((link(argv[1], argv[2])) == -1){
      // Error handling link(), which takes in 2 arguments.
      fprintf(stderr, "cd: syntax error\n");
      return 0;
    }
    return 0;
  } else if (!strcmp(argv[0], "jobs")){
    jobs(job_list);
    return 0;
  } else if (!strcmp(argv[0], "exit")){
    // No necessity for error handling exit().
    // Need to clean job list before exiting.
    cleanup_job_list(job_list);
    exit(0);
  } else if (!strcmp(argv[0], "fg")){
    // The next string in argv after the command fg must start with %.
    if (argv[1][0] != '%'){
      fprintf(stderr, "syntax error: character must be percentage sign!\n");
      return 0;
    } else {
      pid_t jpid;
      char c;
      // The second character of the string starting with % must be a integer 
      // representing jid, else, user has entered illegal syntax.
      if (!(c = argv[1][1])){
        fprintf(stderr, "fg: syntax error\n");
        return 0;
      }
      // Checking if the job exists. If it does not, looking for its pid via get_job_pid will
      // return -1, in which case the following error message is printed.
      if ((jpid = get_job_pid(job_list, c - 48)) == -1){
        fprintf(stderr, "job not found\n");
        return 0;
      } else {
        // If user has entered this command, there should not be any more input after %x, where, x
        // is a jid. If they have, a message indicating illegal syntax is printed.
        if (argv[2]){
          fprintf(stderr, "fg: syntax error\n");
          return 0;
        } else {
          // If the control reaches here, then the user has their syntax correct and the job specified
          // by the passed in jid is in the jobs list, in which case the job is brought to the foreground.
          int status;
          if (tcsetpgrp(STDIN_FILENO, jpid) == -1){
            // Error handling syscall tcsetpgrp.
            err_and_ex("tcsetpgrp failed\n");
          }
          // Paused jobs must be resumed, which is what the conditional below does.
          if (kill(-jpid, SIGCONT) == -1){
            // Error handling syscall kill.
            err_and_ex("kill failed\n");
          }
          // Since the job is brought to the foreground, the shell must not do anything else
          // before it terminates/stops.
          if (waitpid(jpid, &status, WUNTRACED) == -1){
            // Error handling syscall waitpid.
            err_and_ex("waitpid failed\n");
          } else {
            if (WIFEXITED(status)){
              // If the job has ended normally, in which case we come here, it must be removed from
              // the jobs list, without any message being printed out to the stdout.
              remove_job_pid(job_list, jpid);
            } else if (WIFSIGNALED(status)){
              // Control reaches here if job brought to the fg has terminated due to a signal.
              if (printf("[%d] (%d) terminated by signal %d\n", get_job_jid(job_list, jpid), jpid, WTERMSIG(status)) < 0){
                // Error handling syscall printf.
                err_and_ex("printf failed\n");
              }
              // Since terminated by a signal, job must be removed from the jobs list.
              remove_job_pid(job_list, jpid);
            } else if (WIFSTOPPED(status)){
              // The job has stopped.
              if (printf("[%d] (%d) suspended by signal %d\n", get_job_jid(job_list, jpid), jpid, WSTOPSIG(status)) < 0){
                // Error handling printf.
                err_and_ex("printf failed\n");
              }
              // Process state must be changed from running to stopped.
              update_job_jid(job_list, get_job_jid(job_list, jpid), _STATE_STOPPED);
            }
          }
        }
      }
      // command fg successfully executed if we have reached here.
      return 0;
    }
  } else if (!strcmp(argv[0], "bg")){
    // Checking for illegal syntax.
    if (argv[1][0] != '%'){
      fprintf(stderr, "syntax error: character must be percentage sign!\n");
      return 0;
    } else {
      char c;
      // The second character of the string starting with % must be a integer 
      // representing jid, else, user has entered illegal syntax.
      if (!(c = argv[1][1])){
        fprintf(stderr, "bg: syntax error\n");
        return 0;
      }
      // Is job in the list?
      pid_t jpid;
      if ((jpid = (get_job_pid(job_list, c - 48))) == -1){
        fprintf(stderr, "job not found\n");
        return 0;
      } else {
        if (argv[2]){
          return 0;
        } else {
          if (tcsetpgrp(STDIN_FILENO, jpid_shell) == -1){
            // Error handling syscall tcsetpgrp
            err_and_ex("tcsetpgrp failed\n");
          }
          // Must resume job if stopped.
          if (kill(-jpid, SIGCONT) == -1){
            // Error handling syscall kill.
            err_and_ex("kill failed\n");
          }
        }
      }
    }
    // If control has reached here, then we have successfully executed command bg.
    return 0;
  }
  return 1; 
}
/*
 * This function takes in 2 pointers to arrays of strings. Changes the string in the
 * first index of cmd_arg to the last part of that string after the last instance of
 * the char '/' if there is any instances of '/' in the string. Creates a child process
 * by calling fork. Inside the child process, denoted by the conditional (!fork()), changes
 * the string stored at the first index of cmd_arg if it contains '/', stores the full path
 * in a local variable, and then opens files if redir_arg is not empty. 
 * 
 * arguments: cmd_arg and redir_arg.
 *
 * return value of 0 indicates success, 1 indicates failure.
 */

int run_cmd(char** cmd_arg, char** redir_arg){
  pid_t pid = fork();
  if (pid == -1){
    err_and_ex("fork error!\n");
  } else if (!pid){
    // Creating the child process its own process group ID.
    if (setpgid(0 , 0) == -1){
      // Error handling syscall setpgid.
      err_and_ex("setpgid failed\n");
    }
    int i = 0;
    while (cmd_arg[i] != 0){
      i++;
    }
    // If the user has specified &, and at the correct place of the command (at the end),
    // that process must be run in the background, which is what I do below.
    if (*cmd_arg[i - 1] != '&'){
      // Could have alternatively used pid as pgid, because we assing the child process
      // a process group id that is equal to its pid.
      pid_t pgid = getpgid(getpid());
      if (pgid == -1){
        err_and_ex("getpgid failed\n");
      }
      if (tcsetpgrp(STDIN_FILENO, pgid) == -1){
        err_and_ex("tcsetpgrp failed\n");
      }
    } else {
      // Otherwise, the process must run in the foreground.
      if (tcsetpgrp(STDIN_FILENO, jpid_shell) == -1){
        err_and_ex("tcsetpgrp failed\n");
      }
      cmd_arg[i - 1] = 0;
    }
    char* full_path = cmd_arg[0];
    char* argv_token = strrchr(cmd_arg[0], 47);
    if (argv_token != NULL){
      argv_token = &argv_token[1];
      cmd_arg[0] = argv_token;
    }
    // If the first string in redir_arg is not null, then the user has entered at least one
    // redirection symbol and one redirection file.
    if (redir_arg[0] != NULL){
      for (int i = 0; redir_arg[i] != 0; i += 2){
        if (redir_arg[i][0] == '>'){
          // Must assign the file stored in the one index after i of redir_arg the fd of stdout.
          if (close(1) == -1){
            // Error handling close.
            err_and_ex("close error!\n");
          }
          if (!redir_arg[i][1]){
            // The redirection character is >. So if the file does not exist, it must be created. 
            // If it does exist, then it must be truncated. The lowest fd, currently 1 since stdout 
            // was closed just above, is assigned to the file            
            if (open(redir_arg[i + 1], O_CREAT | O_TRUNC | O_WRONLY, 0666) == -1){
              err_and_ex("open error!\n");
            }
          } else {
            // The redirection character is >>. So if the file does not exist, it must be created. If it does
            // exist, then writing my take place from seek.
            if (open(redir_arg[i + 1], O_CREAT | O_APPEND | O_WRONLY, 0666) == -1){
              err_and_ex("open error!\n");
            }
          }
        } else {
          // Redirection character is <. Must assign the file descriptor 0 to the specified input file. 
          // Must close stdin so its file descriptor would be assigned to the specified input file as desired.
          if (close(0) == -1){
            err_and_ex("close error!\n");
          }
          if (open(redir_arg[i + 1], O_RDONLY, 0666) == -1){
            err_and_ex("write error!\n");
          }
        }
      }
    }
    // Must restore the handlers of the following signals.
    if (signal(SIGINT, SIG_DFL) == SIG_ERR){
      // Error handling syscall signal.
      err_and_ex("signal failed\n");
    }
    if (signal(SIGTSTP, SIG_DFL) == SIG_ERR){
      err_and_ex("signal failed\n");
    }
    if (signal(SIGQUIT, SIG_DFL) == SIG_ERR){
      err_and_ex("signal failed\n");
    }
    // Contents of the child's process are replaced by the contents of the process of the program
    // indicated by full_path.
    if(execv(full_path, cmd_arg) == -1){
      err_and_ex("execv error!\n");
    }
  }
  // Checking whether there is an & at the end of the command entered by the user.
  int j = 0;
  while (cmd_arg[j] != 0){
      j++;
  }
  int wstatus;
  if (*cmd_arg[j - 1] != '&'){
    // Must only wait for foreground processes.
    if (waitpid(pid, &wstatus, WUNTRACED) == -1){
      err_and_ex("wait error!\n");
    } else if (WIFSIGNALED(wstatus)){
      // Foreground process terminated by a signal.
      if (printf("[%d] (%d) terminated by signal %d\n", jid, pid, WTERMSIG(wstatus)) < 0){
        err_and_ex("printf error!\n");
      }
      // Must be removed from jobs list.
      remove_job_pid(job_list, pid);
    } else if (WIFSTOPPED(wstatus)){
      // Foreground process stopped by a signal.
      if (printf("[%d] (%d) suspended by signal %d\n", jid, pid, WSTOPSIG(wstatus)) < 0){
        err_and_ex("printf error!\n");
      }
      // Process state must be updated and added to jobs list.
      add_job(job_list, jid, pid, _STATE_STOPPED, cmd_arg[0]);
      jid++;
    } else if (WIFCONTINUED(wstatus)){
      if (printf("[%d] (%d) resumed\n", jid, pid) < 0){
        err_and_ex("printf error!\n");
      }
      // Process state must be updated.
      update_job_jid(job_list, get_job_jid(job_list, pid), _STATE_RUNNING);
    }
  } else {
    // Background processes must be added to jobs list right away.
    add_job(job_list, jid, pid, _STATE_RUNNING, cmd_arg[0]);
    if (printf("[%d] (%d)\n", jid, pid) < 0){
      err_and_ex("printf error!\n");
    }
    jid++;
  }
  // Must restore control back to the shell before returning.
  if (tcsetpgrp(STDIN_FILENO, jpid_shell) == -1){
    err_and_ex("tcsetgprg failed\n");
  }
  return 0;
}
/*
 * My implementation of REPL. Prints a prompt on stdout if the macro PROMPT
 * is defined. Reads command from stdin, parses the command, either performs built-in
 * commands or commands that are not built in.
 * 
 * arguments: no arguments
 *
 * returns 1 upon success.
 */
int repl(){
  pid_t pid;
  int wstatus;
  // Reaping.
  while ((pid = waitpid(-1, &wstatus, WNOHANG|WUNTRACED|WCONTINUED)) > 0){
    if (WIFEXITED(wstatus)){
      if (printf("[%d] (%d) terminated with exit status %d\n", get_job_jid(job_list, pid), pid, WEXITSTATUS(wstatus)) < 0){
        err_and_ex("printf error!\n");
      }
      remove_job_pid(job_list, pid);
    } else if (WIFSIGNALED(wstatus)){
      if (printf("[%d] (%d) terminated by signal %d\n", get_job_jid(job_list, pid), pid, WTERMSIG(wstatus)) < 0){
        err_and_ex("printf error!\n");
      }
      remove_job_pid(job_list, pid);
    } else if (WIFSTOPPED(wstatus)){
      if (printf("[%d] (%d) suspended by signal %d\n", get_job_jid(job_list, pid), pid, WSTOPSIG(wstatus)) < 0){
        err_and_ex("printf error!\n");
      }
      update_job_jid(job_list, get_job_jid(job_list, pid), _STATE_STOPPED);
    } else if (WIFCONTINUED(wstatus)){
      if (printf("[%d] (%d) resumed\n", get_job_jid(job_list, pid), pid) < 0){
        err_and_ex("printf error!\n");
      }
      if (fflush(stdout) != 0){
        err_and_ex("fflush error!\n");
      }
      update_job_jid(job_list, get_job_jid(job_list, pid), _STATE_RUNNING);
    } 
  }
  // Error handling waitpid.
  if (job_list->head != NULL){
    if (pid == -1){
      err_and_ex("waitpid error!\n");
    }
  }
  #ifdef PROMPT
  if (printf("33sh> ") < 0){
    err_and_ex("printf error!\n");
  }
  // Need fflush(stdout) since there is no new line at the end of the prompt.
  if (fflush(stdout) != 0){
    err_and_ex("fflush error!\n");
  }
  #endif
  char p[INPUT_BUF_LEN];
  ssize_t input_len = read(STDIN_FILENO, p, INPUT_BUF_LEN);
  // If there is an error executing read, -1 is returned to input_len, in which case
  // the program must exit() with 1 passed to exit to indicate error.
  if (input_len < 0){
    // Error handling read()
    err_and_ex("read error!\n");
  } else if (input_len == 1){
    // If user presses enter, must return (returns 0 since this is not an error)
    // because we want our prompt to still be displayed and our program to continue
    // running.
    return 0;
  } else if (input_len == 0){
    cleanup_job_list(job_list);
    exit(0);
  } else {
    // In any other cases, our repl should function as desired. Since the user finishes
    // entering command by pressing enter, a newline is added to the end of our buffer,
    // which we replace with the null character below.
    p[input_len - 1] = 0;
    int i = 0;
    // I use strtok to get rid of all whitespace in the command words entered by the user
    // which I store in my buffer.
    char* arg[input_len];
    char* token;
    token = strtok(p, " \t\n");
    arg[i] = token;
    i++;
    while (token != NULL){
      token = strtok(NULL, " \t\n");
      arg[i] = token;
      i++;
    }
    if (i == 1){
      return 0;
    }
    // I add this 0 for my loops in other functions. (I stop executing these loops when I reach
    // the null character in arg)
    arg[i] = 0;
    char* cmd_arg[input_len];
    char* redir_arg[input_len];
    // If parse is successful, meaning the command entered by the user is valid, parse returns 0,
    // in which case we want to execute the commands specified by the user. If not, we want to
    // print the necessary error message to stdout and start from the beginning.
    if (!parse(arg, cmd_arg, redir_arg)){
      // If the user enters a built-in command, it is executed, and 0 is returned because we do not want
      // run_cmd to be executed if a built-in command is executed.
      if (run_built_in_cmd(cmd_arg)){
        run_cmd(cmd_arg, redir_arg);
      }
      if (tcsetpgrp(STDIN_FILENO, jpid_shell) == -1){
        err_and_ex("tcsetgprg failed\n");
      }
    }
  }
  // Clearing memory to avoid data left over from one read call from messing up data from the current 
  // read call.
  memset(p, 0, INPUT_BUF_LEN);
  return 0;
}
/*
 * My main method
 * 
 * arguments: no arguments
 *
 * returns 0.
 */
int main(){
  //Initializing jobs list.
  job_list = init_job_list();
  //Initializing the job id of shell. 
  jpid_shell = getpgid(getpid());
  if (jpid_shell == -1){
    err_and_ex("getpgid failed\n");
  }
  // Want to ignore the following signals when there is no
  // foreground jobs.
  if (jpid_shell == -1){
    err_and_ex("getpgid failed\n");
  }
  if (signal(SIGINT, SIG_IGN) == SIG_ERR){
    err_and_ex("signal failed\n");
  }
  if (signal(SIGTSTP, SIG_IGN) == SIG_ERR){
    err_and_ex("signal failed\n");
  }
  if (signal(SIGQUIT, SIG_IGN) == SIG_ERR){
    err_and_ex("signal failed\n");
  }
  if (signal(SIGTTOU, SIG_IGN) == SIG_ERR){
    err_and_ex("signal failed\n");
  }
  while (!repl());
  return 0;
}



