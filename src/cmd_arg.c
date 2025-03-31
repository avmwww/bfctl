/*
 * Command parser
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cmd_arg.h"
#include "dump_hex.h"

//#define DEBUG_CMD_PARSE

int cmd_arg_to_int(const char *arg, int *val, int len)
{
	int num = 0;

	while (*arg && num < len) {
		char *end;
		val[num++] = strtol(arg, &end, 0);
		while (*end == ' ' && *end != '\0') end++;
		arg = end;
	}
#if 0
	dump_hex_prefix("args", val, num * sizeof(int), 0);
#endif
	return num;
}

/*
 *  Format: <cmd><spaces><args and space delimitters><, or ;>
 */
#define ARG_EQ_DELIM(c)		((c) == ';' || (c) == ' ' || (c) == ',' || (c) == '=')
#define ARG_NEQ_DELIM(c)	((c) != ';' && (c) != ' ' && (c) != ',' &&  (c) != '=')
#define ARG_END(c)		((c) == ';' && (c) != ',')

int cmd_arg_copy(const char *buf, char *cmd, int len, char *arg, int arg_len)
{
	int i = 0;
	char delim = ' ';
	char end = ';';

#ifdef DEBUG_CMD_PARSE
	char *c = cmd, *a = arg;
	printf("Parse <%s>\n", buf);
#endif

	/* skip spaces and delimiters */
	while (ARG_EQ_DELIM(*buf)) {
		buf++;
		i++;
	}

	/* store cmd */
	while (!ARG_EQ_DELIM(*buf) && *buf != '\0' && len) {
		*cmd++ = *buf++;
		i++;
		len--;
	}
	*cmd = '\0';

	if (*buf == '=') {
		delim = '=';
		end = ' ';
	}

	/* skip delim (' ' or '=') */
	while (*buf == delim) {
		buf++;
		i++;
	}

	/* store args */
	while (!ARG_END(*buf) && *buf != end && *buf != '\0' && arg_len) {
		*arg++ = *buf++;
		i++;
		arg_len--;
	}
	*arg = '\0';

#ifdef DEBUG_CMD_PARSE
	printf("CMD: <%s>\n", c);
	printf("ARG: <%s>\n", a);
	printf("LEN: %d\n", i);
#endif
	return i;
}

#if 0
static int cmd_usage(const cmd_t *cmds, const char *arg)
{
	printf("%s commands:\n", cmds->name);
	while (cmds->cmd) {
		printf("\t%-10s   %s\n", cmds->cmd, cmds->help);
		cmds++;
	}
	exit(EXIT_SUCCESS);
}
#endif

static const cmd_t *cmd_arg_search(const cmd_t *ctab, const char *cmd)
{
	while (ctab->cmd) {
		if (!strcmp(cmd, ctab->cmd))
			return ctab;
		ctab++;
	}
	return NULL;
}

/*
 *
 */
const char *cmd_arg_next(const char *arg)
{
	while (*arg != ' ' && *arg != '\0') arg++;
	if (*arg == '\0')
		return NULL;

	while (*arg == ' ' && *arg != '\0') arg++;
	if (*arg == '\0')
		return NULL;

	return arg;
}

/*
 *
 */
int cmd_arg_handle(void *env, const cmd_t *ctab, const char *cmd, const char *arg)
{
	int err;
	const cmd_t *ct;

	ct = cmd_arg_search(ctab, cmd);
	if (!ct)
		return 0;

	if (ct->sub) {
		if (cmd_arg_run(env, arg, ct->sub, NULL) < 0)
			return -1;
	}
	err = ct->handle(env, arg);
	if (err < 0)
		return err;

	return 1;
}

/*
 * Run command
 */
int cmd_arg_exec(void *env, const char *args, const cmd_t *ctab, int (*help_cb)(void *, const char *))
{
	char cmd[256];
	char arg[256];
	int len, err;

	len = cmd_arg_copy(args, cmd, sizeof(cmd), arg, sizeof(arg));

	err = cmd_arg_handle(env, ctab, cmd, arg);
	if (err > 0) {
		err = len;
	} else if (err == 0) {
		printf("Unknown command: %s\n", cmd);
		if (help_cb)
			help_cb(env, arg);
	}

	return err;
}

/*
 *  run all commands
 */
int cmd_arg_run(void *env, const char *run, const cmd_t *ctab, int (*help_cb)(void *, const char *))
{
	int len = 0;
	const char *p = run;

	while (*p) {
		len = cmd_arg_exec(env, p, ctab, help_cb);
		if (len < 1)
			break;

		p += len;
	}
	return len;
}

