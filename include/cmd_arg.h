/*
 * cmd args parser
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _CMD_ARG_H_
#define _CMD_ARG_H_

typedef struct cmd_s {
	const char *cmd;
	const char *help;
	int (*handle)(void * /* context */, const char * /* argumets */);
	const struct cmd_s *sub;
} cmd_t;

int cmd_name_copy(const char *arg, char *name, int len);

int cmd_arg_copy(const char *buf, char *cmd, int len, char *arg, int arg_len);

int cmd_arg_to_int(const char *arg, int *val, int len);

int cmd_arg_handle(void *env, const cmd_t *ctab, const char *cmd, const char *arg);

int cmd_arg_exec(void *env, const char *args, const cmd_t *ctab, int (*help_cb)(void *, const char *));

int cmd_arg_run(void *env, const char *run, const cmd_t *ctab, int (*help_cb)(void *, const char *));

const char *cmd_arg_next(const char *arg);

#endif
