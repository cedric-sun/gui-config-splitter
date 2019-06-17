#include <stdio.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

/* Messages */
#define MSG_USAGE \
"Usage: %s [options] PATH_TO_GUI_CONFIG_JSON\n"\
"Available options are:\n"\
"	-C <directory>		Put the output files in <directory>.\n"\
"	-h			Print this help info.\n"

#define MSG_ARG_ERR "Argument error.\n"
#define MSG_UNK_OPT "Unknown option `-%c'\n"
#define MSG_NOT_DIR_ERR "%s is not a directory\n"
#define MSG_READ_ERR "Failed to read file: %s, please check if it exists and its read permission\n"
#define MSG_JSON_PARSE_ERR "Json parse error: on line %d: %s\n"
#define MSG_RT_NOT_OBJ_ERR "Error: json root is not a json object.\n"
#define MSG_CONFIGS_KEY_NOT_FOUND_ERR "Error: can't find key \"configs\" under root object.\n"
#define MSG_CONFIGS_NOT_ARRAY_ERR "Error: \"configs\" is not a json array.\n"
#define MSG_CONFIG_NOT_OBJ_ERR "Error: element at position %lu in json array \"configs\" is not an json object\n"
#define MSG_FAIL_WRITE "Error: cannot write to file \"%s\", please check permission\n"
#define MSG_LONG_LINE_ERR "Line of too long length exist in %s\n"
#define MSG_BUF_OVFL_ERR "File %s is too large.\n"

/* buffer */

/* json object keys */
#define KEY_CONFIGS "configs"
#define KEY_SERVER "server"
#define KEY_SERVER_PORT "server_port"

/* auxiliary */
#define READONLY "r"
#define COVERWRITE "w"
#define OPTSTRING "C:h"

static const char *progname;

#define LINE_SZ 1024
#define ERR_LONG_LINE 1
#define ERR_BUF_OVFL 2
int get_sanitized_json_str(char *const buffer, size_t size, FILE *file) {
	char line[LINE_SZ];
	int cnt=0;
	buffer[0] = '\0';
	while ( fgets(line, LINE_SZ, file)!= NULL) {
		int newlen=0;
		int i;
		for (i=0;line[i]!='\0';i++) {
			if (isgraph(line[i])) {
				line[newlen++] = line[i];
			}
		}
		if (line[i-1] != '\n') {
			return ERR_LONG_LINE;
		}
		line[newlen] = '\0';
		cnt +=newlen;
		if (cnt>=size) {
			return ERR_BUF_OVFL;
		}
		strcat(buffer,line);
	}
	return 0;
}
#undef LINE_SZ

void print_usage() {
	fprintf(stderr,MSG_USAGE,progname);
}

int main(int argc, char *argv[]) {
	progname = argv[0];
	char c;
	const char *outd_path = ".";
	while ( (c=getopt(argc,argv,OPTSTRING)) != -1) {
		switch(c) {
			case 'C':
				outd_path = optarg;
				break;
			case 'h':
				print_usage();
				return 0;
			case '?':
				fprintf(stderr,MSG_UNK_OPT, optopt);
				print_usage();
				return -1;
			default:
				return -1;
		}
	}

	struct stat sb;
	if (stat(outd_path, &sb) != 0 ) {
		perror(outd_path);
		return -1;
	}

	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, MSG_NOT_DIR_ERR,outd_path);
		return -1;
	}

	if (argc-optind !=1) {
		fputs(MSG_ARG_ERR,stderr);
		print_usage();
		return -1;
	}

	const char *gc_file_path = argv[optind];

	FILE *gc_file = fopen(gc_file_path,READONLY);

	if (!gc_file) {
		fprintf(stderr,MSG_READ_ERR,gc_file_path);
		return -1;
	}

#define BUF_SZ 0x5000
	char *const buffer = malloc(BUF_SZ);
	if (!buffer) {
		fputs("Fatal: fail to allocate memory on heap.\n",stderr);
		return -1;
	}

	int res = get_sanitized_json_str(buffer, BUF_SZ,gc_file);
#undef BUF_SZ

	switch (res) {
		case ERR_LONG_LINE:
			fprintf(stderr, MSG_LONG_LINE_ERR, gc_file_path);
			break;
		case ERR_BUF_OVFL:
			fprintf(stderr, MSG_BUF_OVFL_ERR, gc_file_path);
			break;
	}

	json_error_t error;
	json_t *root = json_loads(buffer, 0x0, &error);
	free(buffer);

	if (!root) {
		fprintf(stderr, MSG_JSON_PARSE_ERR,error.line, error.text);
		return -1;
	}

	if (!json_is_object(root)) {
		fputs(MSG_RT_NOT_OBJ_ERR,stderr);
		return -1;
	}

	/* root is a json object from here*/

	json_t *configs = json_object_get(root, KEY_CONFIGS);

	if (!configs) {
		fputs(MSG_CONFIGS_KEY_NOT_FOUND_ERR, stderr);
		return -1;
	}

	if (!json_is_array(configs)) {
		fputs(MSG_CONFIGS_NOT_ARRAY_ERR, stderr);
		return -1;
	}

	size_t len = json_array_size(configs);

#define PATH_BUF_SZ 256
	for (size_t i =0;i<len;i++) {
		json_t *config = json_array_get(configs,i);
		if (!json_is_object(config)) {
			fprintf(stderr,MSG_CONFIG_NOT_OBJ_ERR,i);
			return -1;
		}
		json_t *server= json_object_get(config, KEY_SERVER);
		json_t *server_port= json_object_get(config, KEY_SERVER_PORT);
		char outf_name[50];
		sprintf(outf_name, "%s:%d",
				json_string_value(server),
				(int)json_integer_value(server_port));
		char outf_path[PATH_BUF_SZ];
		size_t outd_path_len = strlen(outd_path);
		memcpy(outf_path,outd_path,outd_path_len);
		if (outf_path[outd_path_len-1] != '/') {
			outf_path[outd_path_len++] = '/';
		}
		outf_path[outd_path_len] = '\0';
		if (outd_path_len + strlen(outf_name) >= PATH_BUF_SZ) {
			fprintf(stderr, "Path: %s%s is too long",outf_path,outf_name);
			return -1;
		}
		strcat(outf_path, outf_name);
		FILE *result_file = fopen(outf_path, COVERWRITE);
		if (!result_file){
			fprintf(stderr, MSG_FAIL_WRITE,outf_path);
			return -1;
		}
		char *const config_json = json_dumps(config,JSON_INDENT(4));
		fputs(config_json,result_file);
		free(config_json);
		fclose(result_file);

		json_decref(config);
		json_decref(server);
		json_decref(server_port);
	}
#undef PATH_BUF_SZ

	json_decref(root);
	json_decref(configs);
}
