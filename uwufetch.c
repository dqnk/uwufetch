/*
 *  UwUfetch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UWUFETCH_VERSION
	#define UWUFETCH_VERSION "unkown" // needs to be changed by the build script
#endif

#define _GNU_SOURCE // for strcasestr

#include "fetch.h"
#include <getopt.h>
#include <stdbool.h>

// COLORS
#define NORMAL "\x1b[0m"
#define BOLD "\x1b[1m"
#define BLACK "\x1b[30m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define SPRING_GREEN "\x1b[38;5;120m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[0;35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define PINK "\x1b[38;5;201m"
#define LPINK "\x1b[38;5;213m"

#ifdef _WIN32
	#define BLOCK_CHAR "\xdb"			// block char for colors
char* MOVE_CURSOR = "\033[21C"; // moves the cursor after printing the image or the ascii logo
#else
	#define BLOCK_CHAR "\u2587"
char* MOVE_CURSOR = "\033[18C";
#endif // _WIN32

#ifdef __DEBUG__
static bool* verbose_enabled = NULL;
#endif

// all configuration flags available
struct configuration {
	struct flags show; // all true by default
	bool show_image,	 // false by default
			show_colors;	 // true by default
	int show_gpu[256]; // if show_gpu[0] == -2, all gpus are shown, if == -3 no gpu is shown
};

// user's config stored on the disk
struct user_config {
	char *config_directory, // configuration directory name
			*cache_content;			// cache file content
	int read_enabled, write_enabled;
};

// reads the config file
struct configuration parse_config(struct info* user_info, struct user_config* user_config_file) {
	char buffer[256]; // buffer for the current line
	// enabling all flags by default
	struct configuration config_flags;
	memset(&config_flags, 1, sizeof(config_flags));
	memset(&config_flags.show_gpu, -1, 256 * sizeof(int)); // -1 means 'undefined'
	config_flags.show_gpu[0] = -2;												 // show all gpus
	config_flags.show_image	 = 0;

	FILE* config = NULL; // config file pointer
	if (user_config_file->config_directory ==
			NULL) { // if config directory is not set, try to open the default
		if (getenv("HOME") != NULL) {
			char homedir[512];
			sprintf(homedir, "%s/.config/uwufetch/config", getenv("HOME"));
			config = fopen(homedir, "r");
			if (!config) {
				if (getenv("PREFIX") != NULL) {
					char prefixed_etc[512];
					sprintf(prefixed_etc, "%s/etc/uwufetch/config", getenv("PREFIX"));
					config = fopen(prefixed_etc, "r");
				} else
					config = fopen("/etc/uwufetch/config", "r");
			}
		}
	} else
		config = fopen(user_config_file->config_directory, "r");
	if (config == NULL) return config_flags; // if config file does not exist, return the defaults

	int gpu_cfg_count = 0;

	// reading the config file
	while (fgets(buffer, sizeof(buffer), config)) {
		sscanf(buffer, "distro=%s", user_info->os_name);
		if (sscanf(buffer, "image=\"%[^\"]\"", user_info->image_name)) {
			if (user_info->image_name[0] == '~') { // replacing the ~ character with the home directory
				memmove(&user_info->image_name[0], &user_info->image_name[1],
								strlen(user_info->image_name)); // remove the first char
				char temp[128] = "/home/";
				strcat(temp, user_info->user);
				strcat(temp, user_info->image_name);
				sprintf(user_info->image_name, "%s", temp);
			}
			config_flags.show_image = 1; // enable the image flag
		}

		// reading other values
		if (sscanf(buffer, "user=%[truefalse]", buffer))
			config_flags.show.user = !strcmp(buffer, "true");
		if (sscanf(buffer, "os=%[truefalse]", buffer)) config_flags.show.os = strcmp(buffer, "false");
		if (sscanf(buffer, "host=%[truefalse]", buffer))
			config_flags.show.model = strcmp(buffer, "false");
		if (sscanf(buffer, "kernel=%[truefalse]", buffer))
			config_flags.show.kernel = strcmp(buffer, "false");
		if (sscanf(buffer, "cpu=%[truefalse]", buffer)) config_flags.show.cpu = strcmp(buffer, "false");
		if (sscanf(buffer, "gpu=%d",
							 &config_flags.show_gpu[gpu_cfg_count])) // enabling single gpu
			gpu_cfg_count++;
		if (sscanf(buffer, "gpu=%[truefalse]", buffer)) {									 // enabling all gpus
			if (strcmp(buffer, "false") == 0) config_flags.show_gpu[0] = -3; // disable all gpus
		}
		if (sscanf(buffer, "ram=%[truefalse]", buffer)) config_flags.show.ram = strcmp(buffer, "false");
		if (sscanf(buffer, "resolution=%[truefalse]", buffer))
			config_flags.show.resolution = strcmp(buffer, "false");
		if (sscanf(buffer, "shell=%[truefalse]", buffer))
			config_flags.show.shell = strcmp(buffer, "false");
		if (sscanf(buffer, "pkgs=%[truefalse]", buffer))
			config_flags.show.pkgs = strcmp(buffer, "false");
		if (sscanf(buffer, "uptime=%[truefalse]", buffer))
			config_flags.show.uptime = strcmp(buffer, "false");
		if (sscanf(buffer, "colors=%[truefalse]", buffer))
			config_flags.show_colors = strcmp(buffer, "false");
	}
	fclose(config);
	config_flags.show.gpu = (config_flags.show_gpu[0] == -3);
	return config_flags;
}

// prints logo (as an image) of the given system.
void print_image(struct info* user_info) {
#ifndef __IPHONE__
	char command[256];
	if (strlen(user_info->image_name) < 1) {
		char* repl_str = strcmp(user_info->os_name, "android") == 0
												 ? "/data/data/com.termux/files/usr/lib/uwufetch/%s.png"
										 : strcmp(user_info->os_name, "macos") == 0 ? "/usr/local/lib/uwufetch/%s.png"
																																: "/usr/lib/uwufetch/%s.png";
		sprintf(user_info->image_name, repl_str,
						user_info->os_name); // image command for android
	}
	sprintf(command, "viu -t -w 18 -h 8 %s 2> /dev/null",
					user_info->image_name); // creating the command to show the image
	printf("%s\n\n", user_info->image_name);
	if (system(command) != 0) // if viu is not installed or the image is missing
		printf("\033[0E\033[3C%s\n"
					 "   There was an\n"
					 "    error: viu\n"
					 "  is not installed\n"
					 " or the image file\n"
					 "   was not found\n"
					 "   see IMAGES.md\n"
					 "   for more info.\n\n",
					 RED);
#else
	// unfortunately, the iOS stdlib does not have system(); because it reports that it is not available under iOS during compilation
	printf("\033[0E\033[3C%s\n"
				 "   There was an\n"
				 "   error: images\n"
				 "   are currently\n"
				 "  disabled on iOS.\n\n",
				 RED);
#endif
}

// Replaces all terms in a string with another term.
void replace(char* original, char* search, char* replacer) {
	char* ch;
	char buffer[1024];
	while ((ch = strstr(original, search))) {
		ch = strstr(original, search);
		strncpy(buffer, original, ch - original);
		buffer[ch - original] = 0;
		sprintf(buffer + (ch - original), "%s%s", replacer, ch + strlen(search));
		original[0] = 0;
		strcpy(original, buffer);
	}
}

// Replaces all terms in a string with another term, case insensitive
void replace_ignorecase(char* original, char* search, char* replacer) {
	char* ch;
	char buffer[1024];
#ifdef _WIN32
	#define strcasestr(o, s) strstr(o, s)
#endif
	while ((ch = strcasestr(original, search))) {
		strncpy(buffer, original, ch - original);
		buffer[ch - original] = 0;
		sprintf(buffer + (ch - original), "%s%s", replacer, ch + strlen(search));
		original[0] = 0;
		strcpy(original, buffer);
	}
}

#ifdef _WIN32
// windows sucks and hasn't a strstep, so I copied one from https://stackoverflow.com/questions/8512958/is-there-a-windows-variant-of-strsep-function
char* strsep(char** stringp, const char* delim) {
	char* start = *stringp;
	char* p;
	p = (start != NULL) ? strpbrk(start, delim) : NULL;
	if (p == NULL)
		*stringp = NULL;
	else {
		*p			 = '\0';
		*stringp = p + 1;
	}
	return start;
}
#endif

// uwufies distro name
void uwu_name(struct info* user_info) {
#define STRING_TO_UWU(original, uwufied) \
	if (strcmp(user_info->os_name, original) == 0) sprintf(user_info->os_name, "%s", uwufied)

	// linux
	STRING_TO_UWU("alpine", "Nyalpine");
	else STRING_TO_UWU("amogos", "AmogOwOS");
	else STRING_TO_UWU("android", "Nyandroid");
	else STRING_TO_UWU("arch", "Nyarch Linuwu");
	else STRING_TO_UWU("arcolinux", "ArcOwO Linuwu");
	else STRING_TO_UWU("artix", "Nyartix Linuwu");
	else STRING_TO_UWU("debian", "Debinyan");
	else STRING_TO_UWU("devuan", "Devunyan");
	else STRING_TO_UWU("deepin", "Dewepyn");
	else STRING_TO_UWU("endeavouros", "endeavOwO");
	else STRING_TO_UWU("EndeavourOS", "endeavOwO");
	else STRING_TO_UWU("fedora", "Fedowa");
	else STRING_TO_UWU("femboyos", "FemboyOWOS");
	else STRING_TO_UWU("gentoo", "GentOwO");
	else STRING_TO_UWU("gnu", "gnUwU");
	else STRING_TO_UWU("guix", "gnUwU gUwUix");
	else STRING_TO_UWU("linuxmint", "LinUWU Miwint");
	else STRING_TO_UWU("manjaro", "Myanjawo");
	else STRING_TO_UWU("manjaro-arm", "Myanjawo AWM");
	else STRING_TO_UWU("neon", "KDE NeOwOn");
	else STRING_TO_UWU("nixos", "nixOwOs");
	else STRING_TO_UWU("opensuse-leap", "OwOpenSUSE Leap");
	else STRING_TO_UWU("opensuse-tumbleweed", "OwOpenSUSE Tumbleweed");
	else STRING_TO_UWU("pop", "PopOwOS");
	else STRING_TO_UWU("raspbian", "RaspNyan");
	else STRING_TO_UWU("rocky", "Wocky Linuwu");
	else STRING_TO_UWU("slackware", "Swackwawe");
	else STRING_TO_UWU("solus", "sOwOlus");
	else STRING_TO_UWU("ubuntu", "Uwuntu");
	else STRING_TO_UWU("void", "OwOid");
	else STRING_TO_UWU("xerolinux", "xuwulinux");

	// BSD
	else STRING_TO_UWU("freebsd", "FweeBSD");
	else STRING_TO_UWU("openbsd", "OwOpenBSD");

	// Apple family
	else STRING_TO_UWU("macos", "macOwOS");
	else STRING_TO_UWU("ios", "iOwOS");

	// Windows
	else STRING_TO_UWU("windows", "WinyandOwOws");

	else sprintf(user_info->os_name, "%s", "unknown");
#undef STRING_TO_UWU
}

// uwufies kernel name
void uwu_kernel(char* kernel) {
#define KERNEL_TO_UWU(str, original, uwufied) \
	if (strcmp(str, original) == 0) sprintf(str, "%s", uwufied)

	char* temp_kernel = kernel;
	char* token;
	char splitted[16][128] = {};

	int count = 0;
	while ((token = strsep(&temp_kernel, " "))) { // split kernel name
		strcpy(splitted[count], token);
		count++;
	}
	strcpy(kernel, "");
	for (int i = 0; i < 16; i++) {
		// replace kernel name with uwufied version
		KERNEL_TO_UWU(splitted[i], "Linux", "Linuwu");
		else KERNEL_TO_UWU(splitted[i], "linux", "linuwu");
		else KERNEL_TO_UWU(splitted[i], "alpine", "Nyalpine");
		else KERNEL_TO_UWU(splitted[i], "amogos", "AmogOwOS");
		else KERNEL_TO_UWU(splitted[i], "android", "Nyandroid");
		else KERNEL_TO_UWU(splitted[i], "arch", "Nyarch Linuwu");
		else KERNEL_TO_UWU(splitted[i], "artix", "Nyartix Linuwu");
		else KERNEL_TO_UWU(splitted[i], "debian", "Debinyan");
		else KERNEL_TO_UWU(splitted[i], "deepin", "Dewepyn");
		else KERNEL_TO_UWU(splitted[i], "endeavouros", "endeavOwO");
		else KERNEL_TO_UWU(splitted[i], "EndeavourOS", "endeavOwO");
		else KERNEL_TO_UWU(splitted[i], "fedora", "Fedowa");
		else KERNEL_TO_UWU(splitted[i], "femboyos", "FemboyOWOS");
		else KERNEL_TO_UWU(splitted[i], "gentoo", "GentOwO");
		else KERNEL_TO_UWU(splitted[i], "gnu", "gnUwU");
		else KERNEL_TO_UWU(splitted[i], "guix", "gnUwU gUwUix");
		else KERNEL_TO_UWU(splitted[i], "linuxmint", "LinUWU Miwint");
		else KERNEL_TO_UWU(splitted[i], "manjaro", "Myanjawo");
		else KERNEL_TO_UWU(splitted[i], "manjaro-arm", "Myanjawo AWM");
		else KERNEL_TO_UWU(splitted[i], "neon", "KDE NeOwOn");
		else KERNEL_TO_UWU(splitted[i], "nixos", "nixOwOs");
		else KERNEL_TO_UWU(splitted[i], "opensuse-leap", "OwOpenSUSE Leap");
		else KERNEL_TO_UWU(splitted[i], "opensuse-tumbleweed", "OwOpenSUSE Tumbleweed");
		else KERNEL_TO_UWU(splitted[i], "pop", "PopOwOS");
		else KERNEL_TO_UWU(splitted[i], "raspbian", "RaspNyan");
		else KERNEL_TO_UWU(splitted[i], "rocky", "Wocky Linuwu");
		else KERNEL_TO_UWU(splitted[i], "slackware", "Swackwawe");
		else KERNEL_TO_UWU(splitted[i], "solus", "sOwOlus");
		else KERNEL_TO_UWU(splitted[i], "ubuntu", "Uwuntu");
		else KERNEL_TO_UWU(splitted[i], "void", "OwOid");
		else KERNEL_TO_UWU(splitted[i], "xerolinux", "xuwulinux");

		// BSD
		else KERNEL_TO_UWU(splitted[i], "freebsd", "FweeBSD");
		else KERNEL_TO_UWU(splitted[i], "openbsd", "OwOpenBSD");

		// Apple family
		else KERNEL_TO_UWU(splitted[i], "macos", "macOwOS");
		else KERNEL_TO_UWU(splitted[i], "ios", "iOwOS");

		// Windows
		else KERNEL_TO_UWU(splitted[i], "windows", "WinyandOwOws");

		if (i != 0) strcat(kernel, " ");
		strcat(kernel, splitted[i]);
	}
#undef KERNEL_TO_UWU
}

// uwufies hardware names
void uwu_hw(char* hwname) {
#define HW_TO_UWU(original, uwuified) replace_ignorecase(hwname, original, uwuified);
	HW_TO_UWU("lenovo", "LenOwO")
	HW_TO_UWU("cpu", "CC\bPUwU"); // for some reasons this caused a segfault, using a \b (backspace) char fixes it
	HW_TO_UWU("gpu", "GG\bPUwU")
	HW_TO_UWU("graphics", "Gwaphics")
	HW_TO_UWU("corporation", "COwOpowation")
	HW_TO_UWU("nvidia", "NyaVIDIA")
	HW_TO_UWU("mobile", "Mwobile")
	HW_TO_UWU("intel", "Inteww")
	HW_TO_UWU("radeon", "Radenyan")
	HW_TO_UWU("geforce", "GeFOwOce")
	HW_TO_UWU("raspberry", "Nyasberry")
	HW_TO_UWU("broadcom", "Bwoadcom")
	HW_TO_UWU("motorola", "MotOwOwa")
	HW_TO_UWU("proliant", "ProLinyant")
	HW_TO_UWU("poweredge", "POwOwEdge")
	HW_TO_UWU("apple", "Nyaa\bpple")
	HW_TO_UWU("electronic", "ElectrOwOnic")
#undef HW_TO_UWU
}

// uwufies everything
void uwufy_all(struct info* user_info) {
	if (strcmp(user_info->os_name, "windows"))
		MOVE_CURSOR = "\033[21C"; // to print windows logo on not windows systems
	uwu_kernel(user_info->kernel);
	for (int i = 0; user_info->gpu_model[i][0]; i++) uwu_hw(user_info->gpu_model[i]);
	uwu_hw(user_info->cpu_model);
	uwu_hw(user_info->model);
}

// prints all the collected info and returns the number of printed lines
int print_info(struct configuration* config_flags, struct info* user_info) {
	int line_count = 0;
#ifdef _WIN32
	// prints without overflowing the terminal width
	#define responsively_printf(buf, format, ...)     \
		{                                               \
			sprintf(buf, format, __VA_ARGS__);            \
			printf("%.*s\n", user_info->ws_col - 4, buf); \
			line_count++;                                 \
		}
#else // _WIN32
	// prints without overflowing the terminal width
	#define responsively_printf(buf, format, ...)         \
		{                                                   \
			sprintf(buf, format, __VA_ARGS__);                \
			printf("%.*s\n", user_info->win.ws_col - 4, buf); \
			line_count++;                                     \
		}
#endif									// _WIN32
	char print_buf[1024]; // for responsively print

	// print collected info - from host to cpu info
	printf("\033[9A"); // to align info text
	if (config_flags->show.user)
		responsively_printf(print_buf, "%s%s%s%s@%s", MOVE_CURSOR, NORMAL, BOLD, user_info->user,
												user_info->host);
	uwu_name(user_info);
	if (config_flags->show.os)
		responsively_printf(print_buf, "%s%s%sOWOS     %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
												user_info->os_name);
	if (config_flags->show.model)
		responsively_printf(print_buf, "%s%s%sMOWODEL  %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
												user_info->model);
	if (config_flags->show.kernel)
		responsively_printf(print_buf, "%s%s%sKEWNEL   %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
												user_info->kernel);
	if (config_flags->show.cpu)
		responsively_printf(print_buf, "%s%s%sCPUWU    %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
												user_info->cpu_model);

	if (config_flags->show_gpu[0] == -2) { // print all gpu models
		for (int i = 0; i < 256 && user_info->gpu_model[i][0]; i++) {
			responsively_printf(print_buf, "%s%s%sGPUWU    %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
													user_info->gpu_model[i]);
		}
	} else if (config_flags->show_gpu[0] != -3) { // print only the configured gpu models
		for (int i = 0; i < 256; i++) {
			if (config_flags->show_gpu[i] >= 0)
				if (user_info->gpu_model[config_flags->show_gpu[i]][0])
					responsively_printf(print_buf, "%s%s%sGPUWU    %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
															user_info->gpu_model[config_flags->show_gpu[i]]);
		}
	}

	if (config_flags->show.ram) // print ram
		responsively_printf(print_buf, "%s%s%sMEMOWY   %s%i MiB/%i MiB", MOVE_CURSOR, NORMAL, BOLD,
												NORMAL, (user_info->ram_used), user_info->ram_total);
	if (config_flags->show.resolution) // print resolution
		if (user_info->screen_width != 0 || user_info->screen_height != 0)
			responsively_printf(print_buf, "%s%s%sWESOWUTION%s  %dx%d", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
													user_info->screen_width, user_info->screen_height);
	if (config_flags->show.shell) // print shell name
		responsively_printf(print_buf, "%s%s%sSHEWW    %s%s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
												user_info->shell);
	if (config_flags->show.pkgs) // print pkgs
		responsively_printf(print_buf, "%s%s%sPKGS     %s%d: %s", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
												user_info->pkgs, user_info->pkgman_name);
	// #endif
	if (config_flags->show.uptime) {
		switch (user_info->uptime) { // formatting the uptime which is store in seconds
		case 0 ... 3599:
			responsively_printf(print_buf, "%s%s%sUWUPTIME %s%lim", MOVE_CURSOR, NORMAL, BOLD, NORMAL,
													user_info->uptime / 60 % 60);
			break;
		case 3600 ... 86399:
			responsively_printf(print_buf, "%s%s%sUWUPTIME %s%lih, %lim", MOVE_CURSOR, NORMAL, BOLD,
													NORMAL, user_info->uptime / 3600, user_info->uptime / 60 % 60);
			break;
		default:
			responsively_printf(print_buf, "%s%s%sUWUPTIME %s%lid, %lih, %lim", MOVE_CURSOR, NORMAL, BOLD,
													NORMAL, user_info->uptime / 86400, user_info->uptime / 3600 % 24,
													user_info->uptime / 60 % 60);
		}
	}
	if (config_flags->show_colors) // print colors
		printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", MOVE_CURSOR, BOLD, BLACK,
					 BLOCK_CHAR, BLOCK_CHAR, RED, BLOCK_CHAR, BLOCK_CHAR, GREEN, BLOCK_CHAR, BLOCK_CHAR,
					 YELLOW, BLOCK_CHAR, BLOCK_CHAR, BLUE, BLOCK_CHAR, BLOCK_CHAR, MAGENTA, BLOCK_CHAR,
					 BLOCK_CHAR, CYAN, BLOCK_CHAR, BLOCK_CHAR, WHITE, BLOCK_CHAR, BLOCK_CHAR, NORMAL);
	return line_count;
}

// writes cache to cache file
void write_cache(struct info* user_info) {
	char cache_file[512];
	sprintf(cache_file, "%s/.cache/uwufetch.cache",
					getenv("HOME")); // default cache file location
	FILE* cache_fp = fopen(cache_file, "w");
	if (cache_fp == NULL) {
		fprintf(stderr, "Failed to write to %s!", cache_file);
		return;
	}
	// writing all info to the cache file
	fprintf( // writing most of the values to config file
			cache_fp,
			"user=%s\nhost=%s\nversion_name=%s\nhost_model=%s\nkernel=%s\ncpu=%"
			"s\nscreen_width=%d\nscreen_height=%d\nshell=%s\npkgs=%d\npkgman_name=%"
			"s\n",
			user_info->user, user_info->host, user_info->os_name, user_info->model, user_info->kernel,
			user_info->cpu_model, user_info->screen_width, user_info->screen_height, user_info->shell,
			user_info->pkgs, user_info->pkgman_name);

	for (int i = 0; user_info->gpu_model[i][0]; i++) // writing gpu names to file
		fprintf(cache_fp, "gpu=%s\n", user_info->gpu_model[i]);

	fclose(cache_fp);
	return;
}

// reads cache file if it exists
int read_cache(struct info* user_info) {
	char cache_file[512];
	sprintf(cache_file, "%s/.cache/uwufetch.cache",
					getenv("HOME")); // cache file location
	FILE* cache_fp = fopen(cache_file, "r");
	if (cache_fp == NULL) return 0;
	char buffer[256];																	// line buffer
	int gpuc = 0;																			// gpu counter
	while (fgets(buffer, sizeof(buffer), cache_fp)) { // reading the file
		sscanf(buffer, "user=%99[^\n]", user_info->user);
		sscanf(buffer, "host=%99[^\n]", user_info->host);
		sscanf(buffer, "version_name=%99[^\n]", user_info->os_name);
		sscanf(buffer, "host_model=%99[^\n]", user_info->model);
		sscanf(buffer, "kernel=%99[^\n]", user_info->kernel);
		sscanf(buffer, "cpu=%99[^\n]", user_info->cpu_model);
		if (sscanf(buffer, "gpu=%99[^\n]", user_info->gpu_model[gpuc]) != 0) gpuc++;
		sscanf(buffer, "screen_width=%i", &user_info->screen_width);
		sscanf(buffer, "screen_height=%i", &user_info->screen_height);
		sscanf(buffer, "shell=%99[^\n]", user_info->shell);
		sscanf(buffer, "pkgs=%i", &user_info->pkgs);
		sscanf(buffer, "pkgman_name=%99[^\n]", user_info->pkgman_name);
	}

	fclose(cache_fp);
	return 1;
}

// prints logo (as ascii art) of the given system.
void print_ascii(struct info* user_info) {
	printf("\n");
	FILE* file;
	char ascii_file[1024];
	// First tries to get ascii art file from local directory. Useful for debugging
	sprintf(ascii_file, "./res/ascii/%s.txt", user_info->os_name);
	file = fopen(ascii_file, "r");
	if (!file) { // if the file does not exist in the local directory, open it
							 // from the installation directory
		if (strcmp(user_info->os_name, "android") == 0)
			sprintf(ascii_file, "/data/data/com.termux/files/usr/lib/uwufetch/ascii/%s.txt",
							user_info->os_name);
		else if (strcmp(user_info->os_name, "macos") == 0)
			sprintf(ascii_file, "/usr/local/lib/uwufetch/ascii/%s.txt", user_info->os_name);
		else
			sprintf(ascii_file, "/usr/lib/uwufetch/ascii/%s.txt", user_info->os_name);

		file = fopen(ascii_file, "r");
		if (!file) {
			// Prevent infinite loops
			if (strcmp(user_info->os_name, "unknown") == 0) {
				printf("No\nunknown\nascii\nfile\n\n\n\n");
				return;
			}
			sprintf(user_info->os_name, "unknown"); // current os is not supported
			return print_ascii(user_info);
		}
	}
	char buffer[256];									 // line buffer
	while (fgets(buffer, 256, file)) { // replacing color placecholders
		replace(buffer, "{NORMAL}", NORMAL);
		replace(buffer, "{BOLD}", BOLD);
		replace(buffer, "{BLACK}", BLACK);
		replace(buffer, "{RED}", RED);
		replace(buffer, "{GREEN}", GREEN);
		replace(buffer, "{SPRING_GREEN}", SPRING_GREEN);
		replace(buffer, "{YELLOW}", YELLOW);
		replace(buffer, "{BLUE}", BLUE);
		replace(buffer, "{MAGENTA}", MAGENTA);
		replace(buffer, "{CYAN}", CYAN);
		replace(buffer, "{WHITE}", WHITE);
		replace(buffer, "{PINK}", PINK);
		replace(buffer, "{LPINK}", LPINK);
		replace(buffer, "{BLOCK}", BLOCK_CHAR);
		replace(buffer, "{BLOCK_VERTICAL}", BLOCK_CHAR);
		replace(buffer, "{BACKGROUND_GREEN}", "\e[0;42m");
		replace(buffer, "{BACKGROUND_RED}", "\e[0;41m");
		replace(buffer, "{BACKGROUND_WHITE}", "\e[0;47m");
		printf("%s", buffer); // print the line after setting the color
	}
	// Always set color to NORMAL, so there's no need to do this in every ascii file.
	printf(NORMAL);
	fclose(file);
}

/* prints distribution list
	 distributions are listed by distribution branch
	 to make the output easier to understand by the user.*/
void list(char* arg) {
	// clang-format off
	printf("%s -d <options>\n"
				 "  Available distributions:\n"
				 "    "BLUE"Arch linux "NORMAL"based:\n"
				 "      "BLUE"arch, arcolinux, "MAGENTA"artix, endeavouros "GREEN"manjaro, manjaro-arm, "BLUE"xerolinux\n\n"
				 "    "RED"Debian/"YELLOW"Ubuntu "NORMAL"based:\n"
				 "      "RED"amogos, debian, deepin, "GREEN"linuxmint, neon, "BLUE"pop, "RED"raspbian "YELLOW"ubuntu\n\n"
				 "    "RED"BSD "NORMAL"based:\n"
				 "      "RED"freebsd, "YELLOW"openbsd, "GREEN"m"YELLOW"a"RED"c"PINK"o"BLUE"s, "WHITE"ios\n\n"
				 "    "RED"RHEL "NORMAL"based:\n"
				 "      "BLUE"fedora, "GREEN"rocky\n\n"
				 "    "NORMAL"Other/spare distributions:\n"
				 "      "BLUE"alpine, "PINK"femboyos, gentoo, "MAGENTA"slackware, "WHITE"solus, "GREEN"void, opensuse-leap, android, "YELLOW"gnu, guix, "BLUE"windows, "WHITE"unknown\n\n",
				 arg); // Other/spare distributions colors
	// clang-format on
}

// prints the usage
void usage(char* arg) {
	printf("Usage: %s <args>\n"
				 "    -c  --config        use custom config path\n"
				 "    -d, --distro        lets you choose the logo to print\n"
				 "    -h, --help          prints this help page\n"
#ifndef __IPHONE__
				 "    -i, --image         prints logo as image and use a custom image "
				 "if provided\n"
				 "                        %sworks in most terminals\n"
#else
				 "    -i, --image         prints logo as image and use a custom image "
				 "if provided\n"
				 "                        %sdisabled under iOS\n"
#endif
				 "                        read README.md for more info%s\n"
				 "    -l, --list          lists all supported distributions\n"
				 "    -V, --version       prints the current uwufetch version\n"
#ifdef __DEBUG__
				 "    -v, --verbose       logs everything (not yet fully implemented)\n"
#endif
				 "    -w, --write-cache   writes to the cache file "
				 "(~/.cache/uwufetch.cache)\n"
				 "    -r, --read-cache    reads from the cache file "
				 "(~/.cache/uwufetch.cache)\n",
				 arg,
#ifndef __IPHONE__
				 BLUE,
#else
				 RED,
#endif
				 NORMAL);
}

// the main function is on the bottom of the file to avoid double function declarations
int main(int argc, char* argv[]) {
	struct user_config user_config_file = {0};
	struct info user_info								= {0};
	struct configuration config_flags		= parse_config(&user_info, &user_config_file);
	char* custom_distro_name						= NULL;
	char* custom_image_name							= NULL;

#ifdef _WIN32
	// packages disabled by default because chocolatey is too slow
	config_flags.show_pkgs = 0;
#endif

	int opt											 = 0;
	struct option long_options[] = {{"config", required_argument, NULL, 'c'},
																	{"distro", required_argument, NULL, 'd'},
																	{"help", no_argument, NULL, 'h'},
																	{"image", optional_argument, NULL, 'i'},
																	{"list", no_argument, NULL, 'l'},
																	{"read-cache", no_argument, NULL, 'r'},
																	{"version", no_argument, NULL, 'V'},
#ifdef __DEBUG__
																	{"verbose", no_argument, NULL, 'v'},
#endif
																	{"write-cache", no_argument, NULL, 'w'},
																	{NULL, 0, NULL, 0}};
#ifdef __DEBUG__
	#define OPT_STRING "c:d:hi::lrVvw"
	verbose_enabled = get_verbose_handle();
#else
	#define OPT_STRING "c:d:hi::lrVw"
#endif

	// reading cmdline options
	while ((opt = getopt_long(argc, argv, OPT_STRING, long_options, NULL)) != -1) {
		switch (opt) {
		case 'c': // set the config directory
			user_config_file.config_directory = optarg;
			config_flags											= parse_config(&user_info, &user_config_file);
			break;
		case 'd': // set the distribution name
			custom_distro_name = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		case 'i': // set ascii logo as output
			config_flags.show_image = true;
			if (argv[optind]) custom_image_name = argv[optind];
			break;
		case 'l':
			list(argv[0]);
			return 0;
		case 'r':
			user_config_file.read_enabled = true;
			break;
		case 'V':
			printf("UwUfetch version %s\n", UWUFETCH_VERSION);
			return 0;
#ifdef __DEBUG__
		case 'v':
			*verbose_enabled = true;
			LOG_I("version %s", UWUFETCH_VERSION);
			break;
#endif
		case 'w':
			user_config_file.write_enabled = true;
			break;
		default:
			return 1;
		}
	}

	if (user_config_file.read_enabled) {
		LOG_I("reading cache");
		// if no cache file found write to it
		if (!read_cache(&user_info)) {
			LOG_E("reading cache failed, preparing to generate new cache");
			user_config_file.read_enabled	 = false;
			user_config_file.write_enabled = true;
		} else {
			int buf_sz = 256;
			char buffer[buf_sz]; // line buffer
			struct thread_varg vargp = {
					buffer, buf_sz, &user_info, NULL, {true, true, true, true, true, true, true}};
			if (config_flags.show.ram) get_ram(&vargp);
			if (config_flags.show.uptime) {
				LOG_I("getting additional not-cached info");
				get_sys(&user_info);
				get_upt(&vargp);
			}
		}
	}
	if (!user_config_file.read_enabled) {
		LOG_I("reading config");
#ifdef _WIN32
		user_info = get_info(&config_flags); // get the info to modify it with cmdline options
#else
		get_info(config_flags.show, &user_info);
#endif
	}

	if (user_config_file.write_enabled) {
		LOG_I("writing cache");
		write_cache(&user_info);
	}
	if (custom_distro_name) sprintf(user_info.os_name, "%s", custom_distro_name);
	if (custom_image_name) sprintf(user_info.image_name, "%s", custom_image_name);

	uwufy_all(&user_info);

	LOG_V(config_flags.show_image);
	if (config_flags.show_image)
		print_image(&user_info);
	else
		print_ascii(&user_info);

	// move cursor down if the number of printed lines is smaller that the default image height
	printf("\033[%dB", 9 - print_info(&config_flags, &user_info));
	return 0;
}
