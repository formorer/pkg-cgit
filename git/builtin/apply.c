static int unsafe_paths;
	N_("git apply [<options>] [<patch>...]"),
static struct strbuf root = STRBUF_INIT;
	struct object_id threeway_stage[3];
	if (root.len)
		strbuf_insert(&name, 0, root.buf, root.len);
	if (root.len) {
		char *ret = xstrfmt("%s%.*s", root.buf, len, start);
 * Does the ---/+++ line have the POSIX timestamp after the last HT?
	if (patch->def_name && root.len) {
		char *s = xstrfmt("%s%s", root.buf, patch->def_name);
			if (!apply_in_reverse &&
			    ws_error_action == correct_ws_error)
				check_whitespace(line, len, patch->ws_rule);
	if (!deleted && !added)
		return -1;

			applied_pos = -1;
			goto out;
out:
	if (cached || check_index) {
		} else if (has_symlink_leading_path(name, strlen(name))) {
			return error(_("reading from '%s' beyond a symbolic link"), name);
			oidclr(&patch->threeway_stage[0]);
			hashcpy(patch->threeway_stage[0].hash, pre_sha1);
		hashcpy(patch->threeway_stage[1].hash, our_sha1);
		hashcpy(patch->threeway_stage[2].hash, post_sha1);
/*
 * We need to keep track of how symlinks in the preimage are
 * manipulated by the patches.  A patch to add a/b/c where a/b
 * is a symlink should not be allowed to affect the directory
 * the symlink points at, but if the same patch removes a/b,
 * it is perfectly fine, as the patch removes a/b to make room
 * to create a directory a/b so that a/b/c can be created.
 */
static struct string_list symlink_changes;
#define SYMLINK_GOES_AWAY 01
#define SYMLINK_IN_RESULT 02

static uintptr_t register_symlink_changes(const char *path, uintptr_t what)
{
	struct string_list_item *ent;

	ent = string_list_lookup(&symlink_changes, path);
	if (!ent) {
		ent = string_list_insert(&symlink_changes, path);
		ent->util = (void *)0;
	}
	ent->util = (void *)(what | ((uintptr_t)ent->util));
	return (uintptr_t)ent->util;
}

static uintptr_t check_symlink_changes(const char *path)
{
	struct string_list_item *ent;

	ent = string_list_lookup(&symlink_changes, path);
	if (!ent)
		return 0;
	return (uintptr_t)ent->util;
}

static void prepare_symlink_changes(struct patch *patch)
{
	for ( ; patch; patch = patch->next) {
		if ((patch->old_name && S_ISLNK(patch->old_mode)) &&
		    (patch->is_rename || patch->is_delete))
			/* the symlink at patch->old_name is removed */
			register_symlink_changes(patch->old_name, SYMLINK_GOES_AWAY);

		if (patch->new_name && S_ISLNK(patch->new_mode))
			/* the symlink at patch->new_name is created or remains */
			register_symlink_changes(patch->new_name, SYMLINK_IN_RESULT);
	}
}

static int path_is_beyond_symlink_1(struct strbuf *name)
{
	do {
		unsigned int change;

		while (--name->len && name->buf[name->len] != '/')
			; /* scan backwards */
		if (!name->len)
			break;
		name->buf[name->len] = '\0';
		change = check_symlink_changes(name->buf);
		if (change & SYMLINK_IN_RESULT)
			return 1;
		if (change & SYMLINK_GOES_AWAY)
			/*
			 * This cannot be "return 0", because we may
			 * see a new one created at a higher level.
			 */
			continue;

		/* otherwise, check the preimage */
		if (check_index) {
			struct cache_entry *ce;

			ce = cache_file_exists(name->buf, name->len, ignore_case);
			if (ce && S_ISLNK(ce->ce_mode))
				return 1;
		} else {
			struct stat st;
			if (!lstat(name->buf, &st) && S_ISLNK(st.st_mode))
				return 1;
		}
	} while (1);
	return 0;
}

static int path_is_beyond_symlink(const char *name_)
{
	int ret;
	struct strbuf name = STRBUF_INIT;

	assert(*name_ != '\0');
	strbuf_addstr(&name, name_);
	ret = path_is_beyond_symlink_1(&name);
	strbuf_release(&name);

	return ret;
}

static void die_on_unsafe_path(struct patch *patch)
{
	const char *old_name = NULL;
	const char *new_name = NULL;
	if (patch->is_delete)
		old_name = patch->old_name;
	else if (!patch->is_new && !patch->is_copy)
		old_name = patch->old_name;
	if (!patch->is_delete)
		new_name = patch->new_name;

	if (old_name && !verify_path(old_name))
		die(_("invalid path '%s'"), old_name);
	if (new_name && !verify_path(new_name))
		die(_("invalid path '%s'"), new_name);
}

	if (!unsafe_paths)
		die_on_unsafe_path(patch);

	/*
	 * An attempt to read from or delete a path that is beyond a
	 * symbolic link will be prevented by load_patch_target() that
	 * is called at the beginning of apply_data() so we do not
	 * have to worry about a patch marked with "is_delete" bit
	 * here.  We however need to make sure that the patch result
	 * is not deposited to a path that is beyond a symbolic link
	 * here.
	 */
	if (!patch->is_delete && path_is_beyond_symlink(patch->new_name))
		return error(_("affected file '%s' is beyond a symbolic link"),
			     patch->new_name);

	prepare_symlink_changes(patch);
		if (is_null_oid(&patch->threeway_stage[stage - 1]))
		hashcpy(ce->sha1, patch->threeway_stage[stage - 1].hash);
	strbuf_reset(&root);
	strbuf_addstr(&root, arg);
	strbuf_complete(&root, '/');
		OPT_BOOL(0, "unsafe-paths", &unsafe_paths,
			N_("accept a patch that touches outside the working area")),
	if (check_index)
		unsafe_paths = 0;
