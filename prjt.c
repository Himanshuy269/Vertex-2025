#include<stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Replace with your actual Gemini API key from Google AI Studio
#define API_KEY "YOUR_GEMINI_API_KEY_HERE"
#define API_URL "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash-latest:generateContent?key=" API_KEY

// Struct for curl response
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback for curl to write response
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

// Function to call Gemini API
char* call_gemini(const char* job_desc, const char* resume_text) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char *prompt = malloc(1024 + strlen(job_desc) + strlen(resume_text));
    sprintf(prompt, "Score this resume for the job: %s\nResume: %s\nGive score 0-100 and reasons in JSON: {\"score\": int, \"reasons\": \"string\"}", job_desc, resume_text);

    char *post_data = malloc(1024 + strlen(prompt));
    sprintf(post_data, "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}", prompt);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, API_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        free(post_data);
        free(prompt);
    }

    // Parse JSON response (simplified; assumes structure)
    json_object *json = json_tokener_parse(chunk.memory);
    json_object *candidates, *content, *parts, *text;
    json_object_object_get_ex(json, "candidates", &candidates);
    if (json_object_is_type(candidates, json_type_array)) {
        json_object *first = json_object_array_get_idx(candidates, 0);
        json_object_object_get_ex(first, "content", &content);
        json_object_object_get_ex(content, "parts", &parts);
        if (json_object_is_type(parts, json_type_array)) {
            json_object *part = json_object_array_get_idx(parts, 0);
            json_object_object_get_ex(part, "text", &text);
            char *result = strdup(json_object_get_string(text));
            json_object_put(json);
            free(chunk.memory);
            return result;
        }
    }
    free(chunk.memory);
    return strdup("Error: Could not parse response");
}

// Function to read file content
char* read_file(const char* filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, fp);
    fclose(fp);
    string[fsize] = 0;
    return string;
}

int main() {
    initscr();  // Start ncurses
    raw();
    keypad(stdscr, TRUE);
    noecho();

    // Main menu
    char job_desc[1024] = {0};
    char resume_file[256] = {0};
    char *result = NULL;

    int choice;
    do {
        clear();
        printw("Smart Resume Screening System\n");
        printw("1. Enter Job Description\n");
        printw("2. Load Resume File (txt)\n");
        printw("3. Screen Resume\n");
        printw("4. View Results\n");
        printw("5. Exit\n");
        printw("Choice: ");
        refresh();
        choice = getch() - '0';

        switch (choice) {
            case 1:
                clear();
                printw("Enter Job Description: ");
                echo();
                getstr(job_desc);
                noecho();
                break;
            case 2:
                clear();
                printw("Enter Resume Filename: ");
                echo();
                getstr(resume_file);
                noecho();
                break;
            case 3:
                if (strlen(job_desc) > 0 && strlen(resume_file) > 0) {
                    char *resume_text = read_file(resume_file);
                    if (resume_text) {
                        result = call_gemini(job_desc, resume_text);
                        free(resume_text);
                    } else {
                        result = strdup("Error: Could not read file");
                    }
                } else {
                    result = strdup("Error: Missing job desc or file");
                }
                break;
            case 4:
                clear();
                if (result) printw("%s\n", result);
                else printw("No results yet\n");
                printw("Press any key...");
                getch();
                break;
        }
    } while (choice != 5);

    if (result) free(result);
    endwin();  // End ncurses
    return 0;
}