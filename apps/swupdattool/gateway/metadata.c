#define _POSIX_C_SOURCE 200809L
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv)
{
    if (argc != 4) return 2;
    struct json_object *obj = json_object_from_file(argv[1]);
    if (!obj || !json_object_is_type(obj, json_type_object)) {
        json_object_put(obj);
        fprintf(stderr, "Invalid existing device version JSON\n"); return 1;
    }
    json_object_object_add(obj, "app_bundle_version", json_object_new_string(argv[3]));
    const char *text = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY);
    FILE *fp = fopen(argv[2], "w");
    if (!fp) { json_object_put(obj); return 1; }
    int failed = fputs(text, fp) == EOF;
    if (fflush(fp) || fsync(fileno(fp))) failed = 1;
    if (fclose(fp)) failed = 1;
    json_object_put(obj); return failed;
}
