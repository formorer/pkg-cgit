#include "cache.h"
#include "refs.h"
#include "builtin.h"
#include "parse-options.h"
#include "quote.h"
#include "argv-array.h"

static const char * const git_update_ref_usage[] = {
	N_("git update-ref [options] -d <refname> [<oldval>]"),
	N_("git update-ref [options]    <refname> <newval> [<oldval>]"),
	N_("git update-ref [options] --stdin [-z]"),
	NULL
};

static char line_termination = '\n';
static int update_flags;
static const char *msg;

/*
 * Parse one whitespace- or NUL-terminated, possibly C-quoted argument
 * and append the result to arg.  Return a pointer to the terminator.
 * Die if there is an error in how the argument is C-quoted.  This
 * function is only used if not -z.
 */
static const char *parse_arg(const char *next, struct strbuf *arg)
{
	struct ref_update *update;

	/* Allocate and zero-init a struct ref_update */
	update = xcalloc(1, sizeof(*update));
	ALLOC_GROW(updates, updates_count + 1, updates_alloc);
	updates[updates_count++] = update;

	/* Store and reset accumulated options */
	update->flags = update_flags;
	update_flags = 0;

	return update;
}

static void update_store_ref_name(struct ref_update *update,
				  const char *ref_name)
{
	if (check_refname_format(ref_name, REFNAME_ALLOW_ONELEVEL))
		die("invalid ref format: %s", ref_name);
	update->ref_name = xstrdup(ref_name);
}

static void update_store_new_sha1(struct ref_update *update,
				  const char *newvalue)
{
	if (*newvalue && get_sha1(newvalue, update->new_sha1))
		die("invalid new value for ref %s: %s",
		    update->ref_name, newvalue);
}

static void update_store_old_sha1(struct ref_update *update,
				  const char *oldvalue)
{
	if (*oldvalue && get_sha1(oldvalue, update->old_sha1))
		die("invalid old value for ref %s: %s",
		    update->ref_name, oldvalue);

	/* We have an old value if non-empty, or if empty without -z */
	update->have_old = *oldvalue || line_termination;
}

static const char *parse_arg(const char *next, struct strbuf *arg)
{
	/* Parse SP-terminated, possibly C-quoted argument */
	if (*next != '"')
		while (*next && !isspace(*next))
			strbuf_addch(arg, *next++);
	else if (unquote_c_style(arg, next, &next))
		die("badly quoted argument: %s", next);

	/* Return position after the argument */
	return next;
}

static const char *parse_first_arg(const char *next, struct strbuf *arg)
{
	/* Parse argument immediately after "command SP" */
	strbuf_reset(arg);
	if (line_termination) {
		/* Without -z, use the next argument */
		next = parse_arg(next, arg);
	} else {
		/* With -z, use rest of first NUL-terminated line */
		strbuf_addstr(arg, next);
		next = next + arg->len;
	}
	return next;
}

static const char *parse_next_arg(const char *next, struct strbuf *arg)
{
	/* Parse next SP-terminated or NUL-terminated argument, if any */
	strbuf_reset(arg);
	if (line_termination) {
		/* Without -z, consume SP and use next argument */
		if (!*next)
			return NULL;
		if (*next != ' ')
			die("expected SP but got: %s", next);
		next = parse_arg(next + 1, arg);
	} else {
		/* With -z, read the next NUL-terminated line */
		if (*next)
			die("expected NUL but got: %s", next);
		if (strbuf_getline(arg, stdin, '\0') == EOF)
			return NULL;
		next = arg->buf + arg->len;
	}
	return next;
}


/*
 * The following five parse_cmd_*() functions parse the corresponding
 * command.  In each case, next points at the character following the
 * command name and the following space.  They each return a pointer
 * to the character terminating the command, and die with an
 * explanatory message if there are any parsing problems.  All of
 * these functions handle either text or binary format input,
 * depending on how line_termination is set.
 */

static const char *parse_cmd_update(struct ref_transaction *transaction,
				    struct strbuf *input, const char *next)
{
	struct strbuf err = STRBUF_INIT;
	char *refname;
	unsigned char new_sha1[20];
	unsigned char old_sha1[20];
	int have_old;

	refname = parse_refname(input, &next);
	if (!refname)
		die("update: missing <ref>");

	if (parse_next_sha1(input, &next, new_sha1, "update", refname,
			    PARSE_SHA1_ALLOW_EMPTY))
		die("update %s: missing <newvalue>", refname);

	update = update_alloc();

	if ((next = parse_first_arg(next, &ref)) != NULL && ref.buf[0])
		update_store_ref_name(update, ref.buf);
	else
		die("update line missing <ref>");

	if (ref_transaction_update(transaction, refname, new_sha1, old_sha1,
				   update_flags, have_old, msg, &err))
		die("%s", err.buf);

	update_flags = 0;
	free(refname);
	strbuf_release(&err);

	if (next && *next)
		die("update %s has extra input: %s", ref.buf, next);
}

static const char *parse_cmd_create(struct ref_transaction *transaction,
				    struct strbuf *input, const char *next)
{
	struct strbuf err = STRBUF_INIT;
	char *refname;
	unsigned char new_sha1[20];

	refname = parse_refname(input, &next);
	if (!refname)
		die("create: missing <ref>");

	if (parse_next_sha1(input, &next, new_sha1, "create", refname, 0))
		die("create %s: missing <newvalue>", refname);

	if (is_null_sha1(new_sha1))
		die("create %s: zero <newvalue>", refname);

	update = update_alloc();
	update->have_old = 1;

	if (ref_transaction_create(transaction, refname, new_sha1,
				   update_flags, msg, &err))
		die("%s", err.buf);

	update_flags = 0;
	free(refname);
	strbuf_release(&err);

	if (next && *next)
		die("create %s has extra input: %s", ref.buf, next);
}

static const char *parse_cmd_delete(struct ref_transaction *transaction,
				    struct strbuf *input, const char *next)
{
	struct strbuf err = STRBUF_INIT;
	char *refname;
	unsigned char old_sha1[20];
	int have_old;

	refname = parse_refname(input, &next);
	if (!refname)
		die("delete: missing <ref>");

	if (parse_next_sha1(input, &next, old_sha1, "delete", refname,
			    PARSE_SHA1_OLD)) {
		have_old = 0;
	} else {
		if (is_null_sha1(old_sha1))
			die("delete %s: zero <oldvalue>", refname);
		have_old = 1;
	}

	update = update_alloc();

	if (ref_transaction_delete(transaction, refname, old_sha1,
				   update_flags, have_old, msg, &err))
		die("%s", err.buf);

	update_flags = 0;
	free(refname);
	strbuf_release(&err);

	if (next && *next)
		die("delete %s has extra input: %s", ref.buf, next);
}

static const char *parse_cmd_verify(struct ref_transaction *transaction,
				    struct strbuf *input, const char *next)
{
	struct strbuf err = STRBUF_INIT;
	char *refname;
	unsigned char new_sha1[20];
	unsigned char old_sha1[20];

	refname = parse_refname(input, &next);
	if (!refname)
		die("verify: missing <ref>");

	if (parse_next_sha1(input, &next, old_sha1, "verify", refname,
			    PARSE_SHA1_OLD))
		hashclr(old_sha1);

	hashcpy(new_sha1, old_sha1);

	update = update_alloc();

	if (ref_transaction_update(transaction, refname, new_sha1, old_sha1,
				   update_flags, 1, msg, &err))
		die("%s", err.buf);

	update_flags = 0;
	free(refname);
	strbuf_release(&err);

	if (next && *next)
		die("verify %s has extra input: %s", ref.buf, next);
}

static void parse_cmd_option(const char *next)
{
	if (!strcmp(next, "no-deref"))
		update_flags |= REF_NODEREF;
	else
		die("option unknown: %s", next);
}

static void update_refs_stdin(struct ref_transaction *transaction)
{
	struct strbuf cmd = STRBUF_INIT;

	/* Read each line dispatch its command */
	while (strbuf_getline(&cmd, stdin, line_termination) != EOF)
		if (!cmd.buf[0])
			die("empty command in input");
		else if (isspace(*next))
			die("whitespace before command: %s", next);
		else if (starts_with(next, "update "))
			next = parse_cmd_update(transaction, &input, next + 7);
		else if (starts_with(next, "create "))
			next = parse_cmd_create(transaction, &input, next + 7);
		else if (starts_with(next, "delete "))
			next = parse_cmd_delete(transaction, &input, next + 7);
		else if (starts_with(next, "verify "))
			next = parse_cmd_verify(transaction, &input, next + 7);
		else if (starts_with(next, "option "))
			next = parse_cmd_option(&input, next + 7);
		else
			die("unknown command: %s", cmd.buf);

	strbuf_release(&cmd);
}

int cmd_update_ref(int argc, const char **argv, const char *prefix)
{
	const char *refname, *oldval;
	unsigned char sha1[20], oldsha1[20];
	int delete = 0, no_deref = 0, read_stdin = 0, end_null = 0, flags = 0;
	struct option options[] = {
		OPT_STRING( 'm', NULL, &msg, N_("reason"), N_("reason of the update")),
		OPT_BOOL('d', NULL, &delete, N_("delete the reference")),
		OPT_BOOL( 0 , "no-deref", &no_deref,
					N_("update <refname> not the one it points to")),
		OPT_BOOL('z', NULL, &end_null, N_("stdin has NUL-terminated arguments")),
		OPT_BOOL( 0 , "stdin", &read_stdin, N_("read updates from stdin")),
		OPT_END(),
	};

	git_config(git_default_config, NULL);
	argc = parse_options(argc, argv, prefix, options, git_update_ref_usage,
			     0);
	if (msg && !*msg)
		die("Refusing to perform update with empty message.");

	if (read_stdin) {
		struct strbuf err = STRBUF_INIT;
		struct ref_transaction *transaction;

		transaction = ref_transaction_begin(&err);
		if (!transaction)
			die("%s", err.buf);
		if (delete || no_deref || argc > 0)
			usage_with_options(git_update_ref_usage, options);
		if (end_null)
			line_termination = '\0';
		update_refs_stdin(transaction);
		if (ref_transaction_commit(transaction, &err))
			die("%s", err.buf);
		ref_transaction_free(transaction);
		strbuf_release(&err);
		return 0;
	}

	if (end_null)
		usage_with_options(git_update_ref_usage, options);

	if (delete) {
		if (argc < 1 || argc > 2)
			usage_with_options(git_update_ref_usage, options);
		refname = argv[0];
		oldval = argv[1];
	} else {
		const char *value;
		if (argc < 2 || argc > 3)
			usage_with_options(git_update_ref_usage, options);
		refname = argv[0];
		value = argv[1];
		oldval = argv[2];
		if (get_sha1(value, sha1))
			die("%s: not a valid SHA1", value);
	}

	hashclr(oldsha1); /* all-zero hash in case oldval is the empty string */
	if (oldval && *oldval && get_sha1(oldval, oldsha1))
		die("%s: not a valid old SHA1", oldval);

	if (no_deref)
		flags = REF_NODEREF;
	if (delete)
		return delete_ref(refname, oldval ? oldsha1 : NULL, flags);
	else
		return update_ref(msg, refname, sha1, oldval ? oldsha1 : NULL,
				  flags, DIE_ON_ERR);
}
