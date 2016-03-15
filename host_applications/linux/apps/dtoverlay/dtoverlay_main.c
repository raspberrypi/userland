/*
Copyright (c) 2016 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <glob.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <libfdt.h>

#include "dtoverlay.h"
#include "utils.h"


#define CFG_DIR_1 "/sys/kernel/config"
#define CFG_DIR_2 "/config"
#define WORK_DIR "/tmp/.dtoverlays"
#define OVERLAY_SRC_SUBDIR "overlays"
#define README_FILE "README"
#define DT_OVERLAYS_SUBDIR "/device-tree/overlays"
#define DTOVERLAY_PATH_MAX 128


enum {
    OPT_ADD,
    OPT_REMOVE,
    OPT_LIST,
    OPT_LIST_ALL,
    OPT_HELP
};

static const char *boot_dirs[] =
{
#ifdef FORCE_BOOT_DIR
    FORCE_BOOT_DIR,
#else
    "/boot",
    "/flash",
#ifdef OTHER_BOOT_DIR
    OTHER_BOOT_DIR,
#endif
#endif
    NULL /* Terminator */
};

typedef struct state_struct
{
    int count;
    struct dirent **namelist;
} STATE_T;

static int dtoverlay_add(STATE_T *state, const char *overlay,
                         int argc, const char **argv);
static int dtoverlay_remove(STATE_T *state, const char *overlay);
static int dtoverlay_list(STATE_T *state);
static int dtoverlay_list_all(STATE_T *state);
static void usage(void);
static void overlay_help(const char *overlay);

static int apply_overlay(const char *overlay_file, const char *overlay);
static int overlay_applied(const char *overlay_dir);

static STATE_T *read_state(const char *dir);
static void free_state(STATE_T *state);

const char *cmd_name;

const char *work_dir = WORK_DIR;
const char *overlay_src_dir;
const char *dt_overlays_dir;

int main(int argc, const char **argv)
{
    int argn = 1;
    int opt = OPT_ADD;
    const char *overlay = NULL;
    int ret = 0;
    STATE_T *state = NULL;
    const char *cfg_dir;

    cmd_name = argv[0];
    if (strrchr(cmd_name, '/'))
        cmd_name = strrchr(cmd_name, '/') + 1;

    while ((argn < argc) && (argv[argn][0] == '-'))
    {
        const char *arg = argv[argn++];
        if (strcmp(arg, "-r") == 0)
        {
            if (opt != OPT_ADD)
                usage();
            opt = OPT_REMOVE;
        }
        else if ((strcmp(arg, "-l") == 0) ||
                 (strcmp(arg, "--list") == 0))
        {
            if (opt != OPT_ADD)
                usage();
            opt = OPT_LIST;
        }
        else if ((strcmp(arg, "-a") == 0) ||
                 (strcmp(arg, "--listall") == 0) ||
                 (strcmp(arg, "--all") == 0))
        {
	    if (opt != OPT_ADD)
		usage();
	    opt = OPT_LIST_ALL;
	}
	else if (strcmp(arg, "-d") == 0)
	{
	    if (argn == argc)
		usage();
	    overlay_src_dir = argv[argn++];
	}
	else if (strcmp(arg, "-n") == 0)
	{
	    opt_dry_run = 1;
	}
	else if (strcmp(arg, "-v") == 0)
	{
	    opt_verbose = 1;
	}
	else if (strcmp(arg, "-h") == 0)
	{
	    opt = OPT_HELP;
	}
	else
	{
	    fprintf(stderr, "* unknown option '%s'\n", arg);
	    usage();
	}
    }

    if ((opt == OPT_ADD) || (opt == OPT_REMOVE) || (opt == OPT_HELP))
    {
	if (argn == argc)
	    usage();
	overlay = argv[argn++];
    }

    if ((opt != OPT_ADD) && (argn != argc))
	usage();

    if (!overlay_src_dir)
    {
	/* Find the overlays and README */
	int i;

	for (i = 0; boot_dirs[i]; i++)
	{
	    overlay_src_dir = sprintf_dup("%s/" OVERLAY_SRC_SUBDIR,
					  boot_dirs[i]);
	    if (dir_exists(overlay_src_dir))
		break;
	    free_string(overlay_src_dir);
	    overlay_src_dir = NULL;
	}

	if (!overlay_src_dir)
	    fatal_error("Failed to find overlays directory");
    }

    if (opt == OPT_HELP)
    {
	overlay_help(overlay);
	goto orderly_exit;
    }

    if (!dir_exists(work_dir))
    {
	if (run_cmd("mkdir %s", work_dir) != 0)
	    fatal_error("Failed to create '%s' - %d", work_dir, errno);
    }

    cfg_dir = CFG_DIR_1;
    if (!dir_exists(cfg_dir))
    {
	cfg_dir = CFG_DIR_2; 
	if (!dir_exists(cfg_dir))
	{
	    if (run_cmd("sudo mkdir %s", cfg_dir) != 0)
		fatal_error("Failed to create '%s' - %d", cfg_dir, errno);
	}
    }

    if (!dir_exists(cfg_dir) &&
	(run_cmd("sudo mount -t configfs none %s", cfg_dir) != 0))
	    fatal_error("Failed to mount configfs - %d", errno);

    dt_overlays_dir = sprintf_dup("%s/%s", cfg_dir, DT_OVERLAYS_SUBDIR);
    if (!dir_exists(dt_overlays_dir))
	fatal_error("configfs overlays folder not found - incompatible kernel");

    state = read_state(work_dir);
    if (!state)
	fatal_error("Failed to read state");

    switch (opt)
    {
    case OPT_ADD:
	ret = dtoverlay_add(state, overlay, argc - argn, argv + argn);
	break;
    case OPT_REMOVE:
	ret = dtoverlay_remove(state, overlay);
	break;
    case OPT_LIST:
	ret = dtoverlay_list(state);
	break;
    case OPT_LIST_ALL:
	ret = dtoverlay_list_all(state);
	break;
    default:
	ret = 1;
	break;
    }

orderly_exit:
    if (state)
	free_state(state);
    free_strings();

    return ret;
}

static int dtoverlay_add(STATE_T *state, const char *overlay,
			 int argc, const char **argv)
{
    const char *overlay_file;
    char *param_string = NULL;
    void *overlay_fdt;
    int len;
    int i;

    len = strlen(overlay) - 5;
    if (strcmp(overlay + len, ".dtbo") == 0)
    {
	const char *p;
	overlay_file = overlay;
	p = strrchr(overlay, '/');
	if (p)
	{
	    overlay = p;
	    len = strlen(p) - 5;
	}

	overlay = sprintf_dup("%*s", len, overlay);
    }
    else
    {
	overlay_file = sprintf_dup("%s/%s.dtbo", overlay_src_dir, overlay);
    }

    overlay_fdt = dtoverlay_load_dtb(overlay_file, DTOVERLAY_PADDING(4096));
    if (!overlay_fdt)
	return error("Failed to read '%s'", overlay_file);

    /* Apply any parameters next */
    for (i = 0; i < argc; i++)
    {
	const char *arg = argv[i];
	const char *param_val = strchr(arg, '=');
	const char *param, *override;
	char *p = NULL;
	int override_len;
	if (param_val)
	{
	    int len = (param_val - arg);
	    p = sprintf_dup("%.*s", len, arg);
	    param = p;
	    param_val++;
	}
	else
	{
	    /* Use the default parameter value - true */
	    param = arg;
	    param_val = "true";
	}

	override = dtoverlay_find_override(overlay_fdt, param, &override_len);

	if (override)
	{
	    if (dtoverlay_apply_override(overlay_fdt, param,
					 override, override_len,
					 param_val) != 0)
		return error("Failed to set %s=%s", param, param_val);
	}
	else
	{
	    return error("Unknown parameter '%s'", param);
	}

	param_string = sprintf_dup("%s %s=%s",
				   param_string ? param_string : "",
				   param, param_val);

	free_string(p);
    }

    if (param_string)
	dtoverlay_dtb_set_trailer(overlay_fdt, param_string,
				  strlen(param_string) + 1);

    /* Create a filename with the sequence number */
    overlay_file = sprintf_dup("%s/%d_%s.dtbo", work_dir, state->count, overlay);

    /* then write the overlay to the file */
    dtoverlay_save_dtb(overlay_fdt, overlay_file);
    free(overlay_fdt);

    if (!apply_overlay(overlay_file, overlay))
    {
	unlink(overlay_file);
	return 1;
    }

    return 0;
}

static int dtoverlay_remove(STATE_T *state, const char *overlay)
{
    const char *overlay_dir;
    int overlay_len;
    int rmpos;
    int i;

    chdir(work_dir);

    overlay_dir = sprintf_dup("%s/%s", dt_overlays_dir, overlay);
    if (!dir_exists(overlay_dir))
	return error("Overlay '%s' is not loaded", overlay);

    overlay_len = strlen(overlay);

    /* Locate the (earliest) reference to the overlay */
    for (rmpos = 0; rmpos < state->count; rmpos++)
    {
	const char *left, *right;
	left = strchr(state->namelist[rmpos]->d_name, '_');
	if (!left)
	    return error("Internal error");
	left++;
	right = strchr(left, '.');
	if (!right)
	    return error("Internal error");
	if (((right - left) == overlay_len) &&
	    (memcmp(overlay, left, overlay_len) == 0))
	    break;
    }

    if (rmpos < state->count)
    {
	int fillpos;

	/* Unload it and all subsequent overlays in reverse order */
	for (i = state->count - 1; i >= rmpos; i--)
	{
	    const char *left, *right;
	    left = strchr(state->namelist[i]->d_name, '_');
	    if (!left)
		return error("Internal error");
	    left++;
	    right = strrchr(left, '.');
	    if (!right)
		return error("Internal error");

	    run_cmd("sudo rmdir %s/%.*s", dt_overlays_dir, right - left, left);
	}

	/* Replay the sequence, deleting files for the specified overlay,
	   and renumbering and reloading all other overlays. */
	for (i = rmpos, fillpos = rmpos; i < state->count; i++)
	{
	    const char *left, *right;
	    const char *filename = state->namelist[i]->d_name;
	    left = strchr(filename, '_');
	    if (!left)
		return error("Internal error");
	    left++;
	    right = strchr(left, '.');
	    if (!right)
		return error("Internal error");

	    if (((right - left) == overlay_len) &&
		(memcmp(overlay, left, overlay_len) == 0))
	    {
		/* Matches - delete it */
		unlink(filename);
	    }
	    else
	    {
		/* Doesn't match - renumber and reload */
		char *new_file = sprintf_dup("%d_%s", fillpos, left);
		char *new_name = sprintf_dup("%.*s", right - left, left);
		rename(filename, new_file);
		if (!apply_overlay(new_file, new_name))
		    return 1;
		fillpos++;
	    }
	}
    }

    return 0;
}

static int dtoverlay_list(STATE_T *state)
{
    if (state->count == 0)
    {
	printf("No overlays loaded\n");
    }
    else
    {
	int i;
	printf("Overlays (in load order):\n");
	for (i = 0; i < state->count; i++)
	{
	    const char *name, *left, *right;
	    const char *saved_overlay;
	    DTBLOB_T *dtb;
	    name = state->namelist[i]->d_name;
	    left = strchr(name, '_');
	    if (!left)
		return error("Internal error");
	    left++;
	    right = strchr(left, '.');
	    if (!right)
		return error("Internal error");

	    saved_overlay = sprintf_dup("%s/%s", work_dir, name);
	    dtb = dtoverlay_load_dtb(saved_overlay, 0);

	    if (dtoverlay_dtb_trailer(dtb))
		printf("  %.*s %.*s\n", (int)(right - left), left,
		       dtoverlay_dtb_trailer_len(dtb),
		       (char *)dtoverlay_dtb_trailer(dtb));
	    else
		printf("  %.*s\n", (int)(right - left), left);

	    dtoverlay_free_dtb(dtb);
	}
    }

    return 0;
}

static int dtoverlay_list_all(STATE_T *state)
{
    int i;
    DIR *dh;
    struct dirent *de;
    STRING_VEC_T strings;

    string_vec_init(&strings);

    /* Enumerate .dtbo files in the /boot/overlays directory */
    dh = opendir(overlay_src_dir);
    while ((de = readdir(dh)) != NULL)
    {
	int len = strlen(de->d_name) - 5;
	if (strcmp(de->d_name + len, ".dtbo") == 0)
	{
	    string_vec_add(&strings, de->d_name, len);
	}
    }
    closedir(dh);

    /* Merge in active overlays, appending load order, taking care to
     * avoid duplication */
    for (i = 0; i < state->count; i++)
    {
	const char *left, *right;
	char suffix[16];
	char *str;
	int idx;

	sprintf(suffix, " [%d]", i + 1);
	left = strchr(state->namelist[i]->d_name, '_');
	if (!left)
	    return error("Internal error");
	left++;
	right = strchr(left, '.');
	if (!right)
	    return error("Internal error");

	idx = string_vec_find(&strings, left, right - left);
	if (idx >= 0)
	{
	    str = strings.strings[idx];
	    str = realloc(str, strlen(str) + 1 + strlen(suffix));
	    strings.strings[idx] = str;
	}
	else
	    str = string_vec_add(&strings, left,
				 (right - left) + strlen(suffix));
	strcat(str, suffix);
    }

    if (strings.num_strings == 0)
    {
	printf("No overlays found\n");
    }
    else
    {
	/* Sort */
	string_vec_sort(&strings);

	/* Display */
	printf("All overlays - [n]=load order:\n");

	for (i = 0; i < strings.num_strings; i++)
	{
	    printf("  %s\n", strings.strings[i]);
	}
    }

    string_vec_uninit(&strings);

    return 0;
}

static void usage(void)
{
    printf("Usage:\n");
    printf("  %s <overlay> [<param>=<val>...]\n", cmd_name);
    printf("  %*s                Add an overlay (with parameters)\n", (int)strlen(cmd_name), "");
    printf("  %s -r <overlay>   Remove an overlay\n", cmd_name);
    printf("  %s -l             List active overlays\n", cmd_name);
    printf("  %s -a             List all overlays (marking the active)\n", cmd_name);
    printf("  %s -h             Show this usage message\n", cmd_name);
    printf("  %s -h <overlay>   Display help on an overlay\n", cmd_name);
    printf("Options applicable to most variants:\n");
    printf("    -d <dir>    Specify an alternate location for the overlays\n");
    printf("                (defaults to /boot/overlays or /flash/overlays)\n");
    printf("    -n          Dry run - show what would be executed\n");
    printf("    -v          Verbose operation\n");

    exit(1);
}

static void overlay_help(const char *overlay)
{
    OVERLAY_HELP_STATE_T *state;
    const char *readme_path = sprintf_dup("%s/%s", overlay_src_dir,
					  README_FILE);

    state = overlay_help_open(readme_path);
    free_string(readme_path);

    if (state)
    {
	if (overlay_help_find(state, overlay))
	{
	    printf("Name:   %s\n\n", overlay);
	    overlay_help_print_field(state, "Info", "Info:", 8, 0);
	    overlay_help_print_field(state, "Load", "Usage:", 8, 0);
	    overlay_help_print_field(state, "Params", "Params:", 8, 0);
	}
	else
	{
	    fatal_error("No help found for overlay '%s'", overlay);
	}
	overlay_help_close(state);
    }
    else
    {
	fatal_error("Help file not found");
    }
}

static int apply_overlay(const char *overlay_file, const char *overlay)
{
    const char *overlay_dir = sprintf_dup("%s/%s", dt_overlays_dir, overlay);
    int ret = 1;
    if (dir_exists(overlay_dir))
    {
	error("Overlay '%s' is already loaded", overlay);
	ret = 0;
    }
    else if (run_cmd("sudo mkdir %s", overlay_dir) == 0)
    {
	run_cmd("sudo cp %s %s/dtbo", overlay_file, overlay_dir);

	if (!overlay_applied(overlay_dir))
	{
	    run_cmd("sudo rmdir %s", overlay_dir);
	    error("Failed to apply overlay '%s'", overlay);
	    ret = 0;
	}
    }
    else
    {
	error("Failed to create overlay directory");
	ret = 0;
    }

    return ret;
}

static int overlay_applied(const char *overlay_dir)
{
    char status[7] = { '\0' };
    const char *status_path = sprintf_dup("%s/status", overlay_dir);
    FILE *fp = fopen(status_path, "r");
    if (fp)
    {
	fread(status, sizeof(status), 1, fp);
	fclose(fp);
    }
    free_string(status_path);
    return memcmp(status, "applied", sizeof(status)) == 0;
}

int seq_filter(const struct dirent *de)
{
    int num;
    return (sscanf(de->d_name, "%d_", &num) == 1);
}

int seq_compare(const struct dirent **de1, const struct dirent **de2)
{
    int num1 = atoi((*de1)->d_name);
    int num2 = atoi((*de2)->d_name);
    if (num1 < num2)
    	return -1;
    else if (num1 == num2)
    	return 0;
    else
    	return 1;
}

static STATE_T *read_state(const char *dir)
{
    STATE_T *state = malloc(sizeof(STATE_T));
    int i;

    if (state)
	state->count = scandir(dir, &state->namelist, seq_filter, seq_compare);

    for (i = 0; i < state->count; i++)
    {
    	int num = atoi(state->namelist[i]->d_name);
    	if (i != num)
    	    error("Overlay sequence error");
    }
    return state;   
}

static void free_state(STATE_T *state)
{
    int i;
    for (i = 0; i < state->count; i++)
    {
    	free(state->namelist[i]);
    }
    free(state->namelist);
    free(state);
}
