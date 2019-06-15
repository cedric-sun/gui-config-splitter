#include <stdio.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Messages */
#define USAGE_INFO "Usage: %s PATH_TO_GUI_CONFIG_JSON\n" 
#define IO_ERROR_INFO "Failed to load file: %s, please check if it exists and its read permission\n"
#define JSON_PARSE_ERROR_INFO "Json parse error: on line %d: %s\n"
#define ROOT_NOT_OBJECT_INFO "Error: json root is not a json object.\n"
#define CONFIGS_KEY_NOT_FOUND "Error: can't find key \"configs\" under root object.\n"
#define CONFIGS_NOT_ARRAY "Error: \"configs\" is not a json array.\n"
#define CONFIG_NOT_OBJECT "Error: element at position %lu in json array \"configs\" is not an json object\n"
#define MSG_FAIL_WRITE "Error: cannot write to file \"%s\", please check permission\n"

/* buffer */
#define LINE_SZ 0x400
#define BUF_SZ 0x5000

/* json object keys */
#define KEY_CONFIGS "configs"
#define KEY_SERVER "server"

/* auxiliary */
#define READONLY "r"
#define COVERWRITE "w"

void get_sanitized_json_str(char *const buffer, FILE *file) {
	char *const line = malloc(LINE_SZ);
	while ( fgets(line, LINE_SZ, file)!= NULL) {
		int newlen=0;
		for (int i=0;line[i]!='\0';i++) {
			if (isgraph(line[i])) {
				line[newlen++] = line[i];
			}
		}
		line[newlen] = '\0';
		strcat(buffer,line);
	}
	free(line);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, USAGE_INFO, argv[0]);
		return -1;
	}

	const char *gc_file_path = argv[1];

	FILE *gc_file = fopen(gc_file_path,READONLY);

	if (!gc_file) {
		fprintf(stderr,IO_ERROR_INFO,gc_file_path);
		return -1;
	}

	char *const buffer = malloc(BUF_SZ);
	if (!buffer) {
		fputs("Fatal: fail to allocate memory on heap.\n",stderr);
		return -1;
	}

	buffer[0]='\0';
	get_sanitized_json_str(buffer, gc_file);

	json_error_t error;
	json_t *root = json_loads(buffer, 0x0, &error);
	free(buffer);

	if (!root) {
		fprintf(stderr, JSON_PARSE_ERROR_INFO,error.line, error.text);
		return -1;
	}

	if (!json_is_object(root)) {
		fputs(ROOT_NOT_OBJECT_INFO,stderr);
		return -1;
	}

	/* root is a json object from here*/

	json_t *configs = json_object_get(root, KEY_CONFIGS);

	if (!configs) {
		fputs(CONFIGS_KEY_NOT_FOUND, stderr);
		return -1;
	}

	if (!json_is_array(configs)) {
		fputs(CONFIGS_NOT_ARRAY, stderr);
		return -1;
	}

	size_t len = json_array_size(configs);

	for (size_t i =0;i<len;i++) {
		json_t *config = json_array_get(configs,i);
		if (!json_is_object(config)) {
			fprintf(stderr,CONFIG_NOT_OBJECT,i);
			return -1;
		}
		json_t *server= json_object_get(config, KEY_SERVER);
		const char *server_str = json_string_value(server);
		FILE *result_file = fopen(server_str, COVERWRITE);
		if (!result_file){
			fprintf(stderr, MSG_FAIL_WRITE,server_str);
			return -1;
		}
		/* TODO: json_decref */
		char *const config_json = json_dumps(config,JSON_INDENT(4));
		fputs(config_json,result_file);
		free(config_json);
		fclose(result_file);
	}
}
