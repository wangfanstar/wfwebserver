#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <time.h>  
#include <sqlite3.h>  
#include <stdarg.h>  
#include <ctype.h>  
#include <sys/stat.h>  
#include <stdbool.h>  
#include <signal.h>  

#define MAX_CONF_LINE 1024  
#define MAX_PATH_LEN 1024  
#define MAX_QUERY_LEN 4096  
#define MAX_BOOKS 100  
#define MAX_BOOK_NAME_LEN 256  
#define MAX_HTML_BUFFER 10485760 // 10MB buffer  
#define TIMESTAMP_SIZE 30  

// Structure to hold configuration  
typedef struct {  
    int recycle_time;  
    char db_path[MAX_PATH_LEN];  
    char html_path[MAX_PATH_LEN];  
    char link_prefix[MAX_PATH_LEN];  
    char sort_books[MAX_BOOKS][MAX_BOOK_NAME_LEN];  
    int sort_books_count;  
} Config;  

// Structure for document data  
typedef struct {  
    int document_id;  
    char document_name[512];  
    char doc_identify[128];  
    char book_identify[128];  
    int book_id;  
    int member_id;  
    char modify_time[TIMESTAMP_SIZE];  
    int view_count;  
    char account[128];  
    char real_name[256];  
    char book_name[512];  
} Document;  

// Structure for author data  
typedef struct {  
    int member_id;  
    char account[128];  
    char real_name[256];  
    int doc_count;  
    Document *documents;  
    int docs_allocated;  
} Author;  

// Structure for book data  
typedef struct {  
    int book_id;  
    char book_name[512];  
    char identify[128];  
    int doc_count;  
    Document *documents;  
    int docs_allocated;  
} Book;  

// Global variables  
Config config;  
volatile sig_atomic_t keep_running = 1;  

// Function declarations  
void load_config(const char *config_path);  
void trim(char *str);  
void strip_quotes(char *str);  
void generate_html_report(sqlite3 *db);  
void signal_handler(int signum);  
char *str_replace(const char *orig, const char *rep, const char *with);  

// HTML template parts  
void write_html_header(FILE *f);  
void write_html_footer(FILE *f);  
void write_latest_docs(FILE *f, sqlite3 *db);  
void write_popular_docs(FILE *f, sqlite3 *db);  
void write_popular_docs_table(FILE *f, sqlite3 *db, const char *time_range);
void write_author_rankings(FILE *f, sqlite3 *db);  
void write_book_rankings(FILE *f, sqlite3 *db);  
void write_author_rankings_table(FILE *f, sqlite3 *db, const char *time_range);
void write_selected_book_rankings_table(FILE *f, sqlite3 *db);
void write_all_book_rankings_table(FILE *f, sqlite3 *db);
char* get_current_time();

// Load configuration from file  
void load_config(const char *config_path) {  
    FILE *f = fopen(config_path, "r");  
    if (!f) {  
        fprintf(stderr, "Error opening config file: %s\n", config_path);  
        exit(EXIT_FAILURE);  
    }  

    // Set defaults  
    config.recycle_time = 10;  
    strcpy(config.db_path, "mindoc.db");  
    strcpy(config.html_path, "mindocsort.html");  
    strcpy(config.link_prefix, "");  
    config.sort_books_count = 0;  

    char line[MAX_CONF_LINE];  
    char *key, *value;  

    while (fgets(line, sizeof(line), f)) {  
        // Skip comment lines and empty lines  
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')  
            continue;  

        // Find '=' separator  
        char *separator = strchr(line, '=');  
        if (!separator)  
            continue;  

        // Extract key and value  
        *separator = '\0';  
        key = line;  
        value = separator + 1;  

        // Trim whitespace  
        trim(key);  
        trim(value);  

        // Parse configuration values  
        if (strcmp(key, "recycle_time") == 0) {  
            config.recycle_time = atoi(value);  
        } else if (strcmp(key, "db_path") == 0) {  
            strncpy(config.db_path, value, MAX_PATH_LEN - 1);  
            config.db_path[MAX_PATH_LEN - 1] = '\0';  
        } else if (strcmp(key, "html_path") == 0) {  
            strncpy(config.html_path, value, MAX_PATH_LEN - 1);  
            config.html_path[MAX_PATH_LEN - 1] = '\0';  
        } else if (strcmp(key, "link_prefix") == 0) {  
            strncpy(config.link_prefix, value, MAX_PATH_LEN - 1);  
            config.link_prefix[MAX_PATH_LEN - 1] = '\0';  
        } else if (strcmp(key, "sort_books") == 0) {  
            // Parse comma-separated list of book names  
            char *token = strtok(value, ",");  
            while (token && config.sort_books_count < MAX_BOOKS) {  
                trim(token);  
                strip_quotes(token);  
                strncpy(config.sort_books[config.sort_books_count], token, MAX_BOOK_NAME_LEN - 1);  
                config.sort_books[config.sort_books_count][MAX_BOOK_NAME_LEN - 1] = '\0';  
                config.sort_books_count++;  
                token = strtok(NULL, ",");  
            }  
        }  
    }  

    fclose(f);  
}  

// Trim whitespace from string  
void trim(char *str) {  
    if (!str || !*str)  
        return;  

    // Trim leading spaces  
    char *p = str;  
    while (isspace((unsigned char)*p))  
        p++;  

    if (p != str) {  
        memmove(str, p, strlen(p) + 1);  
    }  

    // Trim trailing spaces  
    int len = strlen(str);  
    while (len > 0 && isspace((unsigned char)str[len - 1])) {  
        str[--len] = '\0';  
    }  
}  

// Remove quotes from string  
void strip_quotes(char *str) {  
    if (!str || !*str)  
        return;  

    int len = strlen(str);  
    if (len >= 2 && ((str[0] == '"' && str[len-1] == '"') ||   
                     (str[0] == '\'' && str[len-1] == '\''))) {  
        // Shift everything one character left  
        memmove(str, str + 1, len - 2);  
        str[len - 2] = '\0';  
    }  
}  

// Signal handler for graceful termination  
void signal_handler(int signum) {  
    keep_running = 0;  
}  

// String replacement function  
char *str_replace(const char *orig, const char *rep, const char *with) {  
    char *result;  
    char *ins;  
    char *tmp;  
    int len_rep;  
    int len_with;  
    int len_front;  
    int count;  

    if (!orig || !rep)  
        return NULL;  
    
    len_rep = strlen(rep);  
    if (len_rep == 0)  
        return NULL;  
    
    if (!with)  
        with = "";  
    
    len_with = strlen(with);  

    // Count occurrences of rep in orig  
    ins = (char *)orig;  
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {  
        ins = tmp + len_rep;  
    }  

    // Allocate for result  
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);  
    if (!result)  
        return NULL;  

    // Replace occurrences  
    while (count--) {  
        ins = strstr(orig, rep);  
        len_front = ins - orig;  
        tmp = strncpy(tmp, orig, len_front) + len_front;  
        tmp = strcpy(tmp, with) + len_with;  
        orig += len_front + len_rep;  
    }  
    strcpy(tmp, orig);  
    
    return result;  
}  

// Main generation function for the HTML report  
void generate_html_report(sqlite3 *db) {  
    FILE *html_file = fopen(config.html_path, "w");  
    if (!html_file) {  
        fprintf(stderr, "Error opening HTML output file: %s\n", config.html_path);  
        return;  
    }  

    // Write HTML structure  
    write_html_header(html_file);  
    write_latest_docs(html_file, db);  
    write_popular_docs(html_file, db);  
    write_author_rankings(html_file, db);  
    write_book_rankings(html_file, db);  
    write_html_footer(html_file);  

    fclose(html_file);  
    printf("Generated HTML report: %s\n", config.html_path);  
}  

// 写入HTML头部并固定导航栏  
void write_html_header(FILE *f) {  
    fprintf(f, "<!DOCTYPE html>\n");  
    fprintf(f, "<html lang=\"zh-CN\">\n");  
    fprintf(f, "<head>\n");  
    fprintf(f, "    <meta charset=\"UTF-8\">\n");  
    fprintf(f, "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");  
    fprintf(f, "    <title>MinDoc 文档统计</title>\n");  
    fprintf(f, "    <link href=\"tailwind.min.css\" rel=\"stylesheet\">\n");  
    fprintf(f, "    <script src=\"alpine.min.js\" defer></script>\n");  
    fprintf(f, "    <style>\n");  
    fprintf(f, "        [x-cloak] { display: none !important; }\n");  
    fprintf(f, "        body { padding-top: 160px; } /* 为固定头部预留空间 */\n");  
    fprintf(f, "        .fixed-header { background-color: white; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n");  
    fprintf(f, "    </style>\n");  
    fprintf(f, "</head>\n");  
    fprintf(f, "<body class=\"bg-gray-50\">\n");  
    
    // Alpine.js 状态容器  
    fprintf(f, "<div x-data=\"{ tab: 'latest' }\">\n");  
    
    // 固定在顶部的标题和导航栏  
    fprintf(f, "    <div class=\"fixed top-0 left-0 right-0 fixed-header z-50 border-b border-gray-200\">\n");  
    fprintf(f, "        <div class=\"container mx-auto px-4 py-3\">\n");  
    fprintf(f, "            <h1 class=\"text-3xl font-bold text-center text-blue-600 mb-2\">MinDoc 文档统计</h1>\n");  
    fprintf(f, "            <p class=\"text-center text-gray-600 mb-4\">生成时间: %s</p>\n",   
                get_current_time());  
    
    // 导航部分  
    fprintf(f, "            <nav class=\"bg-white rounded-lg p-2\">\n");  
    fprintf(f, "                <div class=\"flex flex-wrap justify-center space-x-2 md:space-x-4\">\n");  
    fprintf(f, "                    <button @click=\"tab = 'latest'\" :class=\"tab === 'latest' ? 'bg-blue-600 text-white' : 'bg-gray-200 text-gray-800'\" class=\"px-4 py-2 rounded-lg transition-colors\">\n");  
    fprintf(f, "                        最新更新文档\n");  
    fprintf(f, "                    </button>\n");  
    fprintf(f, "                    <button @click=\"tab = 'popular'\" :class=\"tab === 'popular' ? 'bg-blue-600 text-white' : 'bg-gray-200 text-gray-800'\" class=\"px-4 py-2 rounded-lg transition-colors\">\n");  
    fprintf(f, "                        最受欢迎文档\n");  
    fprintf(f, "                    </button>\n");  
    fprintf(f, "                    <button @click=\"tab = 'authors'\" :class=\"tab === 'authors' ? 'bg-blue-600 text-white' : 'bg-gray-200 text-gray-800'\" class=\"px-4 py-2 rounded-lg transition-colors\">\n");  
    fprintf(f, "                        个人文档排名\n");  
    fprintf(f, "                    </button>\n");  
    fprintf(f, "                    <button @click=\"tab = 'books'\" :class=\"tab === 'books' ? 'bg-blue-600 text-white' : 'bg-gray-200 text-gray-800'\" class=\"px-4 py-2 rounded-lg transition-colors\">\n");  
    fprintf(f, "                        组内文档排名\n");  
    fprintf(f, "                    </button>\n");  
    fprintf(f, "                </div>\n");  
    fprintf(f, "            </nav>\n");  
    fprintf(f, "        </div>\n");  
    fprintf(f, "    </div>\n");  
    
    // 主内容容器  
    fprintf(f, "    <div class=\"container mx-auto px-4 py-3\">\n");  
}  

void write_html_footer(FILE *f) {  
    fprintf(f, "    </div>\n"); // 关闭主内容容器  
    fprintf(f, "</div>\n");     // 关闭 Alpine.js 状态容器  
    fprintf(f, "</body>\n");  
    fprintf(f, "</html>\n");  
}  

// Get current formatted time  
char* get_current_time() {  
    static char buffer[64];  
    time_t now = time(NULL);  
    struct tm *tm_info = localtime(&now);  
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);  
    return buffer;  
}  

// Main execution function  
int main(int argc, char *argv[]) {  
    char config_path[MAX_PATH_LEN] = "mindocsort.conf";  
    
    // Check if config file path is provided  
    if (argc > 1) {  
        strncpy(config_path, argv[1], MAX_PATH_LEN - 1);  
        config_path[MAX_PATH_LEN - 1] = '\0';  
    }  
    
    // Load configuration  
    load_config(config_path);  
    
    // Set up signal handlers for graceful termination  
    signal(SIGINT, signal_handler);  
    signal(SIGTERM, signal_handler);  
    
    printf("MinDoc Statistics Generator started\n");  
    printf("  Database: %s\n", config.db_path);  
    printf("  HTML Output: %s\n", config.html_path);  
    printf("  Update Interval: %d minutes\n", config.recycle_time);  
    
    // Main processing loop  
    while (keep_running) {  
        sqlite3 *db;  
        int rc = sqlite3_open(config.db_path, &db);  
        
        if (rc) {  
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
            sqlite3_close(db);  
            sleep(60); // Wait a minute and try again  
            continue;  
        }  
        
        // Generate the HTML report  
        generate_html_report(db);  
        
        // Close database  
        sqlite3_close(db);  
        
        // Sleep until next update  
        printf("Next update in %d minutes\n", config.recycle_time);  
        for (int i = 0; i < config.recycle_time && keep_running; i++) {  
            sleep(60);  
        }  
    }  
    
    printf("MinDoc Statistics Generator stopped\n");  
    return 0;  
}  

// Function to write the latest documents section  
void write_latest_docs(FILE *f, sqlite3 *db) {  
    fprintf(f, "<div x-show=\"tab === 'latest'\" x-cloak>\n");  
    fprintf(f, "    <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">最新更新的文档</h2>\n");  
    fprintf(f, "    <div class=\"overflow-x-auto\">\n");  
    fprintf(f, "        <table class=\"min-w-full bg-white shadow-md rounded-lg overflow-hidden\">\n");  
    fprintf(f, "            <thead class=\"bg-gray-100\">\n");  
    fprintf(f, "                <tr>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">序号</th>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档名称</th>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">作者帐号</th>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">作者姓名</th>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">书籍名称</th>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">阅读次数</th>\n");  
    fprintf(f, "                    <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">更新时间</th>\n");  
    fprintf(f, "                </tr>\n");  
    fprintf(f, "            </thead>\n");  
    fprintf(f, "            <tbody class=\"divide-y divide-gray-200\">\n");  

    // Query for most recently updated documents  
    const char *query =   
        "SELECT d.document_id, d.document_name, d.identify as doc_identify, "  
        "d.book_id, d.member_id, d.modify_time, d.view_count, "  
        "m.account, m.real_name, "  
        "b.book_name, b.identify as book_identify "  
        "FROM md_documents d "  
        "LEFT JOIN md_members m ON d.member_id = m.member_id "  
        "LEFT JOIN md_books b ON d.book_id = b.book_id "  
        "ORDER BY d.modify_time DESC "  
        "LIMIT 100";  

    sqlite3_stmt *stmt;  
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);  
    
    if (rc != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        return;  
    }  

    int count = 1;  
    while (sqlite3_step(stmt) == SQLITE_ROW) {  
        const char *doc_name = (const char*)sqlite3_column_text(stmt, 1);  
        const char *doc_identify = (const char*)sqlite3_column_text(stmt, 2);  
        const char *account = (const char*)sqlite3_column_text(stmt, 7);  
        const char *real_name = (const char*)sqlite3_column_text(stmt, 8);  
        const char *book_name = (const char*)sqlite3_column_text(stmt, 9);  
        const char *book_identify = (const char*)sqlite3_column_text(stmt, 10);  
        int view_count = sqlite3_column_int(stmt, 6);  
        const char *modify_time = (const char*)sqlite3_column_text(stmt, 5);  

        // Generate document link  
        char doc_link[MAX_PATH_LEN];  
        snprintf(doc_link, MAX_PATH_LEN, "%s/%s/%s",   
                config.link_prefix,   
                book_identify ? book_identify : "",   
                doc_identify ? doc_identify : "");  

        // Ensure names aren't NULL for display  
        if (!doc_name) doc_name = "";  
        if (!account) account = "";  
        if (!real_name) real_name = "";  
        if (!book_name) book_name = "";  
        if (!modify_time) modify_time = "";  

        // Display row  
        fprintf(f, "                <tr class=\"hover:bg-gray-50\">\n");  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%d</td>\n", count++);  
        fprintf(f, "                    <td class=\"py-3 px-4 font-medium text-blue-600\">"  
                   "<a href=\"%s\" target=\"_blank\" class=\"hover:underline\">%s</a></td>\n",   
                   doc_link, doc_name);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%s</td>\n", account);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-800\">%s</td>\n",   
                    real_name[0] ? real_name : account);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%s</td>\n", book_name);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%d</td>\n", view_count);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%s</td>\n", modify_time);  
        fprintf(f, "                </tr>\n");  
    }  

    sqlite3_finalize(stmt);  
    
    fprintf(f, "            </tbody>\n");  
    fprintf(f, "        </table>\n");  
    fprintf(f, "    </div>\n");  
    fprintf(f, "</div>\n");  
}  

// Function to write the popular documents section  
void write_popular_docs(FILE *f, sqlite3 *db) {  
    fprintf(f, "<div x-show=\"tab === 'popular'\" x-cloak>\n");  
    fprintf(f, "    <div x-data=\"{ timeRange: 'all' }\">\n");  
    
    // Time range selector  
    fprintf(f, "        <div class=\"mb-6 flex justify-center\">\n");  
    fprintf(f, "            <div class=\"bg-white rounded-lg shadow-md p-1 inline-flex space-x-1\">\n");  
    fprintf(f, "                <button @click=\"timeRange = 'all'\" :class=\"timeRange === 'all' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    所有时间\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "                <button @click=\"timeRange = 'month'\" :class=\"timeRange === 'month' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    过去一个月\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "                <button @click=\"timeRange = 'week'\" :class=\"timeRange === 'week' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    过去一周\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "            </div>\n");  
    fprintf(f, "        </div>\n");  

    // All time popular documents  
    fprintf(f, "        <div x-show=\"timeRange === 'all'\">\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">最受欢迎文档 (所有时间)</h2>\n");  
    write_popular_docs_table(f, db, "all");  
    fprintf(f, "        </div>\n");  

    // Past month popular documents  
    fprintf(f, "        <div x-show=\"timeRange === 'month'\" x-cloak>\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">最受欢迎文档 (过去一个月)</h2>\n");  
    write_popular_docs_table(f, db, "month");  
    fprintf(f, "        </div>\n");  

    // Past week popular documents  
    fprintf(f, "        <div x-show=\"timeRange === 'week'\" x-cloak>\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">最受欢迎文档 (过去一周)</h2>\n");  
    write_popular_docs_table(f, db, "week");  
    fprintf(f, "        </div>\n");  

    fprintf(f, "    </div>\n");  
    fprintf(f, "</div>\n");  
}  

// Helper function to write popular documents table with time filters  
void write_popular_docs_table(FILE *f, sqlite3 *db, const char *time_range) {  
    fprintf(f, "        <div class=\"overflow-x-auto\">\n");  
    fprintf(f, "            <table class=\"min-w-full bg-white shadow-md rounded-lg overflow-hidden\">\n");  
    fprintf(f, "                <thead class=\"bg-gray-100\">\n");  
    fprintf(f, "                    <tr>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">排名</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档名称</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">作者帐号</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">作者姓名</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">书籍名称</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">阅读次数</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">更新时间</th>\n");  
    fprintf(f, "                    </tr>\n");  
    fprintf(f, "                </thead>\n");  
    fprintf(f, "                <tbody class=\"divide-y divide-gray-200\">\n");  

    char query[MAX_QUERY_LEN];  
    
    // Create query based on time range  
    if (strcmp(time_range, "week") == 0) {  
        snprintf(query, MAX_QUERY_LEN,  
            "SELECT d.document_id, d.document_name, d.identify as doc_identify, "  
            "d.book_id, d.member_id, d.modify_time, d.view_count, "  
            "m.account, m.real_name, "  
            "b.book_name, b.identify as book_identify "  
            "FROM md_documents d "  
            "LEFT JOIN md_members m ON d.member_id = m.member_id "  
            "LEFT JOIN md_books b ON d.book_id = b.book_id "  
            "WHERE d.modify_time >= datetime('now', '-7 days') "  
            "ORDER BY d.view_count DESC, d.modify_time DESC "  
            "LIMIT 100");  
    } else if (strcmp(time_range, "month") == 0) {  
        snprintf(query, MAX_QUERY_LEN,  
            "SELECT d.document_id, d.document_name, d.identify as doc_identify, "  
            "d.book_id, d.member_id, d.modify_time, d.view_count, "  
            "m.account, m.real_name, "  
            "b.book_name, b.identify as book_identify "  
            "FROM md_documents d "  
            "LEFT JOIN md_members m ON d.member_id = m.member_id "  
            "LEFT JOIN md_books b ON d.book_id = b.book_id "  
            "WHERE d.modify_time >= datetime('now', '-30 days') "  
            "ORDER BY d.view_count DESC, d.modify_time DESC "  
            "LIMIT 100");  
    } else {  
        snprintf(query, MAX_QUERY_LEN,  
            "SELECT d.document_id, d.document_name, d.identify as doc_identify, "  
            "d.book_id, d.member_id, d.modify_time, d.view_count, "  
            "m.account, m.real_name, "  
            "b.book_name, b.identify as book_identify "  
            "FROM md_documents d "  
            "LEFT JOIN md_members m ON d.member_id = m.member_id "  
            "LEFT JOIN md_books b ON d.book_id = b.book_id "  
            "ORDER BY d.view_count DESC, d.modify_time DESC "  
            "LIMIT 100");  
    }  

    sqlite3_stmt *stmt;  
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);  
    
    if (rc != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        return;  
    }  

    int rank = 1;  
    while (sqlite3_step(stmt) == SQLITE_ROW) {  
        const char *doc_name = (const char*)sqlite3_column_text(stmt, 1);  
        const char *doc_identify = (const char*)sqlite3_column_text(stmt, 2);  
        const char *account = (const char*)sqlite3_column_text(stmt, 7);  
        const char *real_name = (const char*)sqlite3_column_text(stmt, 8);  
        const char *book_name = (const char*)sqlite3_column_text(stmt, 9);  
        const char *book_identify = (const char*)sqlite3_column_text(stmt, 10);  
        int view_count = sqlite3_column_int(stmt, 6);  
        const char *modify_time = (const char*)sqlite3_column_text(stmt, 5);  

        // Generate document link  
        char doc_link[MAX_PATH_LEN];  
        snprintf(doc_link, MAX_PATH_LEN, "%s/%s/%s",   
                config.link_prefix,   
                book_identify ? book_identify : "",   
                doc_identify ? doc_identify : "");  

        // Ensure names aren't NULL for display  
        if (!doc_name) doc_name = "";  
        if (!account) account = "";  
        if (!real_name) real_name = "";  
        if (!book_name) book_name = "";  
        if (!modify_time) modify_time = "";  

        // Display row with rank highlighting  
        fprintf(f, "                <tr class=\"hover:bg-gray-50 %s\">\n",   
                rank <= 3 ? "bg-yellow-50" : "");  
        fprintf(f, "                    <td class=\"py-3 px-4 %s font-bold\">%d</td>\n",   
                rank <= 3 ? "text-amber-600" : "text-gray-500", rank++);  
        fprintf(f, "                    <td class=\"py-3 px-4 font-medium text-blue-600\">"  
                   "<a href=\"%s\" target=\"_blank\" class=\"hover:underline\">%s</a></td>\n",   
                   doc_link, doc_name);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%s</td>\n", account);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-800\">%s</td>\n",   
                    real_name[0] ? real_name : account);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%s</td>\n", book_name);  
        fprintf(f, "                    <td class=\"py-3 px-4 font-medium %s\">%d</td>\n",   
                rank <= 4 ? "text-amber-600" : "text-gray-500", view_count);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-500\">%s</td>\n", modify_time);  
        fprintf(f, "                </tr>\n");  
    }  

    sqlite3_finalize(stmt);  
    
    fprintf(f, "                </tbody>\n");  
    fprintf(f, "            </table>\n");  
    fprintf(f, "        </div>\n");  
}  

// Function to write author rankings section  
void write_author_rankings(FILE *f, sqlite3 *db) {  
    fprintf(f, "<div x-show=\"tab === 'authors'\" x-cloak>\n");  
    fprintf(f, "    <div x-data=\"{ timeRange: 'all', expandedAuthor: null }\">\n");  
    
    // Time range selector  
    fprintf(f, "        <div class=\"mb-6 flex justify-center\">\n");  
    fprintf(f, "            <div class=\"bg-white rounded-lg shadow-md p-1 inline-flex space-x-1\">\n");  
    fprintf(f, "                <button @click=\"timeRange = 'all'; expandedAuthor = null\" :class=\"timeRange === 'all' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    所有时间\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "                <button @click=\"timeRange = 'month'; expandedAuthor = null\" :class=\"timeRange === 'month' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    过去一个月\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "                <button @click=\"timeRange = 'week'; expandedAuthor = null\" :class=\"timeRange === 'week' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    过去一周\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "            </div>\n");  
    fprintf(f, "        </div>\n");  

    // All time author rankings  
    fprintf(f, "        <div x-show=\"timeRange === 'all'\">\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">个人文档排名 (所有时间)</h2>\n");  
    write_author_rankings_table(f, db, "all");  
    fprintf(f, "        </div>\n");  

    // Past month author rankings  
    fprintf(f, "        <div x-show=\"timeRange === 'month'\" x-cloak>\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">个人文档排名 (过去一个月)</h2>\n");  
    write_author_rankings_table(f, db, "month");  
    fprintf(f, "        </div>\n");  

    // Past week author rankings  
    fprintf(f, "        <div x-show=\"timeRange === 'week'\" x-cloak>\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">个人文档排名 (过去一周)</h2>\n");  
    write_author_rankings_table(f, db, "week");  
    fprintf(f, "        </div>\n");  

    fprintf(f, "    </div>\n");  
    fprintf(f, "</div>\n");  
}  

// Helper function to write author rankings table with time filters  
void write_author_rankings_table(FILE *f, sqlite3 *db, const char *time_range) {  
    fprintf(f, "        <div class=\"overflow-x-auto\">\n");  
    fprintf(f, "            <table class=\"min-w-full bg-white shadow-md rounded-lg overflow-hidden\">\n");  
    fprintf(f, "                <thead class=\"bg-gray-100\">\n");  
    fprintf(f, "                    <tr>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">排名</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">帐号</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">姓名</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档数量</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-center text-xs font-medium text-gray-500 uppercase tracking-wider\">操作</th>\n");  
    fprintf(f, "                    </tr>\n");  
    fprintf(f, "                </thead>\n");  
    fprintf(f, "                <tbody class=\"divide-y divide-gray-200\">\n");  

    char query[MAX_QUERY_LEN];  
    
    // Create query based on time range for author document counts  
    if (strcmp(time_range, "week") == 0) {  
        snprintf(query, MAX_QUERY_LEN,  
            "SELECT m.member_id, m.account, m.real_name, COUNT(d.document_id) as doc_count "  
            "FROM md_members m "  
            "LEFT JOIN md_documents d ON m.member_id = d.member_id "  
            "WHERE d.modify_time >= datetime('now', '-7 days') "  
            "GROUP BY m.member_id "  
            "HAVING doc_count > 0 "  
            "ORDER BY doc_count DESC "  
            "LIMIT 100");  
    } else if (strcmp(time_range, "month") == 0) {  
        snprintf(query, MAX_QUERY_LEN,  
            "SELECT m.member_id, m.account, m.real_name, COUNT(d.document_id) as doc_count "  
            "FROM md_members m "  
            "LEFT JOIN md_documents d ON m.member_id = d.member_id "  
            "WHERE d.modify_time >= datetime('now', '-30 days') "  
            "GROUP BY m.member_id "  
            "HAVING doc_count > 0 "  
            "ORDER BY doc_count DESC "  
            "LIMIT 100");  
    } else {  
        snprintf(query, MAX_QUERY_LEN,  
            "SELECT m.member_id, m.account, m.real_name, COUNT(d.document_id) as doc_count "  
            "FROM md_members m "  
            "LEFT JOIN md_documents d ON m.member_id = d.member_id "  
            "GROUP BY m.member_id "  
            "HAVING doc_count > 0 "  
            "ORDER BY doc_count DESC "  
            "LIMIT 100");  
    }  

    sqlite3_stmt *stmt;  
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);  
    
    if (rc != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        return;  
    }  

    int rank = 1;  
    while (sqlite3_step(stmt) == SQLITE_ROW) {  
        int member_id = sqlite3_column_int(stmt, 0);  
        const char *account = (const char*)sqlite3_column_text(stmt, 1);  
        const char *real_name = (const char*)sqlite3_column_text(stmt, 2);  
        int doc_count = sqlite3_column_int(stmt, 3);  

        // Ensure names aren't NULL for display  
        if (!account) account = "";  
        if (!real_name) real_name = "";  

        // Display author row  
        fprintf(f, "                <tr class=\"hover:bg-gray-50 %s\">\n",   
                rank <= 3 ? "bg-yellow-50" : "");  
        fprintf(f, "                    <td class=\"py-3 px-4 %s font-bold\">%d</td>\n",   
                rank <= 3 ? "text-amber-600" : "text-gray-500", rank++);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-800\">%s</td>\n", account);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-800\">%s</td>\n",   
                    real_name[0] ? real_name : account);  
        fprintf(f, "                    <td class=\"py-3 px-4 font-medium %s\">%d</td>\n",   
                rank <= 4 ? "text-amber-600" : "text-gray-500", doc_count);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-center\">\n");  
        fprintf(f, "                        <button @click=\"expandedAuthor = expandedAuthor === %d ? null : %d\" "  
                    "class=\"px-3 py-1 rounded bg-blue-100 text-blue-700 hover:bg-blue-200 focus:outline-none "  
                    "focus:ring-2 focus:ring-blue-300 transition-colors\">\n",   
                    member_id, member_id);  
        fprintf(f, "                            <span x-text=\"expandedAuthor === %d ? '收起' : '查看文档'\"></span>\n",   
                    member_id);  
        fprintf(f, "                        </button>\n");  
        fprintf(f, "                    </td>\n");  
        fprintf(f, "                </tr>\n");  

        // Expandable row for author's documents  
        fprintf(f, "                <tr x-show=\"expandedAuthor === %d\" x-cloak>\n", member_id);  
        fprintf(f, "                    <td colspan=\"5\" class=\"p-0 bg-gray-50\">\n");  
        fprintf(f, "                        <div class=\"p-4\">\n");  
        fprintf(f, "                            <h4 class=\"font-medium text-gray-800 mb-2\">文档列表</h4>\n");  
        fprintf(f, "                            <div class=\"overflow-y-auto max-h-96\">\n");  
        fprintf(f, "                                <table class=\"min-w-full divide-y divide-gray-200\">\n");  
        fprintf(f, "                                    <thead class=\"bg-gray-100\">\n");  
        fprintf(f, "                                        <tr>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档名称</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">书籍名称</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">阅读次数</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">更新时间</th>\n");  
        fprintf(f, "                                        </tr>\n");  
        fprintf(f, "                                    </thead>\n");  
        fprintf(f, "                                    <tbody class=\"divide-y divide-gray-200\">\n");  

        // Query the author's documents  
        char doc_query[MAX_QUERY_LEN];  
        if (strcmp(time_range, "week") == 0) {  
            snprintf(doc_query, MAX_QUERY_LEN,  
                "SELECT d.document_name, d.identify as doc_identify, d.view_count, d.modify_time, "  
                "b.book_name, b.identify as book_identify "  
                "FROM md_documents d "  
                "LEFT JOIN md_books b ON d.book_id = b.book_id "  
                "WHERE d.member_id = %d "  
                "AND d.modify_time >= datetime('now', '-7 days') "  
                "ORDER BY d.modify_time DESC", member_id);  
        } else if (strcmp(time_range, "month") == 0) {  
            snprintf(doc_query, MAX_QUERY_LEN,  
                "SELECT d.document_name, d.identify as doc_identify, d.view_count, d.modify_time, "  
                "b.book_name, b.identify as book_identify "  
                "FROM md_documents d "  
                "LEFT JOIN md_books b ON d.book_id = b.book_id "  
                "WHERE d.member_id = %d "  
                "AND d.modify_time >= datetime('now', '-30 days') "  
                "ORDER BY d.modify_time DESC", member_id);  
        } else {  
            snprintf(doc_query, MAX_QUERY_LEN,  
                "SELECT d.document_name, d.identify as doc_identify, d.view_count, d.modify_time, "  
                "b.book_name, b.identify as book_identify "  
                "FROM md_documents d "  
                "LEFT JOIN md_books b ON d.book_id = b.book_id "  
                "WHERE d.member_id = %d "  
                "ORDER BY d.modify_time DESC", member_id);  
        }  

        sqlite3_stmt *doc_stmt;  
        rc = sqlite3_prepare_v2(db, doc_query, -1, &doc_stmt, NULL);  
        
        if (rc == SQLITE_OK) {  
            while (sqlite3_step(doc_stmt) == SQLITE_ROW) {  
                const char *doc_name = (const char*)sqlite3_column_text(doc_stmt, 0);  
                const char *doc_identify = (const char*)sqlite3_column_text(doc_stmt, 1);  
                int view_count = sqlite3_column_int(doc_stmt, 2);  
                const char *modify_time = (const char*)sqlite3_column_text(doc_stmt, 3);  
                const char *book_name = (const char*)sqlite3_column_text(doc_stmt, 4);  
                const char *book_identify = (const char*)sqlite3_column_text(doc_stmt, 5);  

                // Generate document link  
                char doc_link[MAX_PATH_LEN];  
                snprintf(doc_link, MAX_PATH_LEN, "%s/%s/%s",   
                        config.link_prefix,   
                        book_identify ? book_identify : "",   
                        doc_identify ? doc_identify : "");  

                // Ensure names aren't NULL for display  
                if (!doc_name) doc_name = "";  
                if (!book_name) book_name = "";  
                if (!modify_time) modify_time = "";  

                // Display document row  
                fprintf(f, "                                        <tr class=\"hover:bg-gray-100\">\n");  
                fprintf(f, "                                            <td class=\"py-2 px-3 font-medium text-blue-600\">"  
                        "<a href=\"%s\" target=\"_blank\" class=\"hover:underline\">%s</a></td>\n",   
                        doc_link, doc_name);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%s</td>\n", book_name);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%d</td>\n", view_count);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%s</td>\n", modify_time);  
                fprintf(f, "                                        </tr>\n");  
            }  
            sqlite3_finalize(doc_stmt);  
        }  

        fprintf(f, "                                    </tbody>\n");  
        fprintf(f, "                                </table>\n");  
        fprintf(f, "                            </div>\n");  
        fprintf(f, "                        </div>\n");  
        fprintf(f, "                    </td>\n");  
        fprintf(f, "                </tr>\n");  
    }  

    sqlite3_finalize(stmt);  
    
    fprintf(f, "                </tbody>\n");  
    fprintf(f, "            </table>\n");  
    fprintf(f, "        </div>\n");  
}  

// Function to write book rankings section  
void write_book_rankings(FILE *f, sqlite3 *db) {  
    fprintf(f, "<div x-show=\"tab === 'books'\" x-cloak>\n");  
    fprintf(f, "    <div x-data=\"{ bookGroup: 'selected', expandedBook: null }\">\n");  
    
    // Group selector  
    fprintf(f, "        <div class=\"mb-6 flex justify-center\">\n");  
    fprintf(f, "            <div class=\"bg-white rounded-lg shadow-md p-1 inline-flex space-x-1\">\n");  
    fprintf(f, "                <button @click=\"bookGroup = 'selected'; expandedBook = null\" :class=\"bookGroup === 'selected' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    指定组文档排名\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "                <button @click=\"bookGroup = 'all'; expandedBook = null\" :class=\"bookGroup === 'all' ? 'bg-blue-500 text-white' : 'bg-gray-100 text-gray-800'\" class=\"px-4 py-2 rounded-md transition-colors\">\n");  
    fprintf(f, "                    所有组文档排名\n");  
    fprintf(f, "                </button>\n");  
    fprintf(f, "            </div>\n");  
    fprintf(f, "        </div>\n");  

    // Selected books rankings  
    fprintf(f, "        <div x-show=\"bookGroup === 'selected'\">\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">指定组文档排名</h2>\n");  
    write_selected_book_rankings_table(f, db);  
    fprintf(f, "        </div>\n");  

    // All books rankings  
    fprintf(f, "        <div x-show=\"bookGroup === 'all'\" x-cloak>\n");  
    fprintf(f, "            <h2 class=\"text-2xl font-bold mb-4 text-gray-800\">所有组文档排名</h2>\n");  
    write_all_book_rankings_table(f, db);  
    fprintf(f, "        </div>\n");  

    fprintf(f, "    </div>\n");  
    fprintf(f, "</div>\n");  
}  

// Helper function to write selected book rankings table  
void write_selected_book_rankings_table(FILE *f, sqlite3 *db) {  
    fprintf(f, "        <div class=\"overflow-x-auto\">\n");  
    fprintf(f, "            <table class=\"min-w-full bg-white shadow-md rounded-lg overflow-hidden\">\n");  
    fprintf(f, "                <thead class=\"bg-gray-100\">\n");  
    fprintf(f, "                    <tr>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">排名</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">书籍名称</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档数量</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-center text-xs font-medium text-gray-500 uppercase tracking-wider\">操作</th>\n");  
    fprintf(f, "                    </tr>\n");  
    fprintf(f, "                </thead>\n");  
    fprintf(f, "                <tbody class=\"divide-y divide-gray-200\">\n");  

    char book_list[MAX_QUERY_LEN] = "''"; // Start with empty string for SQL IN clause  
    
    // Build the book list for the IN clause  
    for (int i = 0; i < config.sort_books_count; i++) {  
        char temp[MAX_BOOK_NAME_LEN * 2]; // Double size for quotes and escaping  
        snprintf(temp, sizeof(temp), ", '%s'", config.sort_books[i]);  
        strcat(book_list, temp);  
    }  

    char query[MAX_QUERY_LEN];  
    snprintf(query, MAX_QUERY_LEN,  
        "SELECT b.book_id, b.book_name, b.identify, COUNT(d.document_id) as doc_count "  
        "FROM md_books b "  
        "LEFT JOIN md_documents d ON b.book_id = d.book_id "  
        "WHERE b.book_name IN (%s) "  
        "GROUP BY b.book_id "  
        "ORDER BY doc_count DESC", book_list);  

    sqlite3_stmt *stmt;  
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);  
    
    if (rc != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        return;  
    }  

    int rank = 1;  
    while (sqlite3_step(stmt) == SQLITE_ROW) {  
        int book_id = sqlite3_column_int(stmt, 0);  
        const char *book_name = (const char*)sqlite3_column_text(stmt, 1);  
        const char *identify = (const char*)sqlite3_column_text(stmt, 2);  
        int doc_count = sqlite3_column_int(stmt, 3);  

        // Ensure names aren't NULL for display  
        if (!book_name) book_name = "";  
        if (!identify) identify = "";  

        // Display book row  
        fprintf(f, "                <tr class=\"hover:bg-gray-50 %s\">\n",   
                rank <= 3 ? "bg-yellow-50" : "");  
        fprintf(f, "                    <td class=\"py-3 px-4 %s font-bold\">%d</td>\n",   
                rank <= 3 ? "text-amber-600" : "text-gray-500", rank++);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-800\">%s</td>\n", book_name);  
        fprintf(f, "                    <td class=\"py-3 px-4 font-medium %s\">%d</td>\n",   
                rank <= 4 ? "text-amber-600" : "text-gray-500", doc_count);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-center\">\n");  
        fprintf(f, "                        <button @click=\"expandedBook = expandedBook === %d ? null : %d\" "  
                    "class=\"px-3 py-1 rounded bg-blue-100 text-blue-700 hover:bg-blue-200 focus:outline-none "  
                    "focus:ring-2 focus:ring-blue-300 transition-colors\">\n",   
                    book_id, book_id);  
        fprintf(f, "                            <span x-text=\"expandedBook === %d ? '收起' : '查看文档'\"></span>\n",   
                    book_id);  
        fprintf(f, "                        </button>\n");  
        fprintf(f, "                    </td>\n");  
        fprintf(f, "                </tr>\n");  

        // Expandable row for book's documents  
        fprintf(f, "                <tr x-show=\"expandedBook === %d\" x-cloak>\n", book_id);  
        fprintf(f, "                    <td colspan=\"4\" class=\"p-0 bg-gray-50\">\n");  
        fprintf(f, "                        <div class=\"p-4\">\n");  
        fprintf(f, "                            <h4 class=\"font-medium text-gray-800 mb-2\">文档列表</h4>\n");  
        fprintf(f, "                            <div class=\"overflow-y-auto max-h-96\">\n");  
        fprintf(f, "                                <table class=\"min-w-full divide-y divide-gray-200\">\n");  
        fprintf(f, "                                    <thead class=\"bg-gray-100\">\n");  
        fprintf(f, "                                        <tr>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档名称</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">作者</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">阅读次数</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">更新时间</th>\n");  
        fprintf(f, "                                        </tr>\n");  
        fprintf(f, "                                    </thead>\n");  
        fprintf(f, "                                    <tbody class=\"divide-y divide-gray-200\">\n");  

        // Query the book's documents  
        char doc_query[MAX_QUERY_LEN];  
        snprintf(doc_query, MAX_QUERY_LEN,  
            "SELECT d.document_name, d.identify as doc_identify, d.view_count, d.modify_time, "  
            "m.account, m.real_name "  
            "FROM md_documents d "  
            "LEFT JOIN md_members m ON d.member_id = m.member_id "  
            "WHERE d.book_id = %d "  
            "ORDER BY d.view_count DESC, d.modify_time DESC", book_id);  

        sqlite3_stmt *doc_stmt;  
        rc = sqlite3_prepare_v2(db, doc_query, -1, &doc_stmt, NULL);  
        
        if (rc == SQLITE_OK) {  
            while (sqlite3_step(doc_stmt) == SQLITE_ROW) {  
                const char *doc_name = (const char*)sqlite3_column_text(doc_stmt, 0);  
                const char *doc_identify = (const char*)sqlite3_column_text(doc_stmt, 1);  
                int view_count = sqlite3_column_int(doc_stmt, 2);  
                const char *modify_time = (const char*)sqlite3_column_text(doc_stmt, 3);  
                const char *account = (const char*)sqlite3_column_text(doc_stmt, 4);  
                const char *real_name = (const char*)sqlite3_column_text(doc_stmt, 5);  

                // Generate document link  
                char doc_link[MAX_PATH_LEN];  
                snprintf(doc_link, MAX_PATH_LEN, "%s/%s/%s",   
                        config.link_prefix,   
                        identify,   
                        doc_identify ? doc_identify : "");  

                // Ensure names aren't NULL for display  
                if (!doc_name) doc_name = "";  
                if (!account) account = "";  
                if (!real_name) real_name = "";  
                if (!modify_time) modify_time = "";  

                // Display document row  
                fprintf(f, "                                        <tr class=\"hover:bg-gray-100\">\n");  
                fprintf(f, "                                            <td class=\"py-2 px-3 font-medium text-blue-600\">"  
                        "<a href=\"%s\" target=\"_blank\" class=\"hover:underline\">%s</a></td>\n",   
                        doc_link, doc_name);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%s</td>\n",   
                        real_name && real_name[0] ? real_name : account);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%d</td>\n", view_count);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%s</td>\n", modify_time);  
                fprintf(f, "                                        </tr>\n");  
            }  
            sqlite3_finalize(doc_stmt);  
        }  

        fprintf(f, "                                    </tbody>\n");  
        fprintf(f, "                                </table>\n");  
        fprintf(f, "                            </div>\n");  
        fprintf(f, "                        </div>\n");  
        fprintf(f, "                    </td>\n");  
        fprintf(f, "                </tr>\n");  
    }  

    sqlite3_finalize(stmt);  
    
    fprintf(f, "                </tbody>\n");  
    fprintf(f, "            </table>\n");  
    fprintf(f, "        </div>\n");  
}  

// Helper function to write all book rankings table  
void write_all_book_rankings_table(FILE *f, sqlite3 *db) {  
    fprintf(f, "        <div class=\"overflow-x-auto\">\n");  
    fprintf(f, "            <table class=\"min-w-full bg-white shadow-md rounded-lg overflow-hidden\">\n");  
    fprintf(f, "                <thead class=\"bg-gray-100\">\n");  
    fprintf(f, "                    <tr>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">排名</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">书籍名称</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档数量</th>\n");  
    fprintf(f, "                        <th class=\"py-3 px-4 text-center text-xs font-medium text-gray-500 uppercase tracking-wider\">操作</th>\n");  
    fprintf(f, "                    </tr>\n");  
    fprintf(f, "                </thead>\n");  
    fprintf(f, "                <tbody class=\"divide-y divide-gray-200\">\n");  

    char query[MAX_QUERY_LEN];  
    snprintf(query, MAX_QUERY_LEN,  
        "SELECT b.book_id, b.book_name, b.identify, COUNT(d.document_id) as doc_count "  
        "FROM md_books b "  
        "LEFT JOIN md_documents d ON b.book_id = d.book_id "  
        "GROUP BY b.book_id "  
        "HAVING doc_count > 0 "  
        "ORDER BY doc_count DESC "  
        "LIMIT 100");  

    sqlite3_stmt *stmt;  
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);  
    
    if (rc != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        return;  
    }  

    int rank = 1;  
    while (sqlite3_step(stmt) == SQLITE_ROW) {  
        int book_id = sqlite3_column_int(stmt, 0);  
        const char *book_name = (const char*)sqlite3_column_text(stmt, 1);  
        const char *identify = (const char*)sqlite3_column_text(stmt, 2);  
        int doc_count = sqlite3_column_int(stmt, 3);  

        // Ensure names aren't NULL for display  
        if (!book_name) book_name = "";  
        if (!identify) identify = "";  

        // Display book row  
        fprintf(f, "                <tr class=\"hover:bg-gray-50 %s\">\n",   
                rank <= 3 ? "bg-yellow-50" : "");  
        fprintf(f, "                    <td class=\"py-3 px-4 %s font-bold\">%d</td>\n",   
                rank <= 3 ? "text-amber-600" : "text-gray-500", rank++);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-gray-800\">%s</td>\n", book_name);  
        fprintf(f, "                    <td class=\"py-3 px-4 font-medium %s\">%d</td>\n",   
                rank <= 4 ? "text-amber-600" : "text-gray-500", doc_count);  
        fprintf(f, "                    <td class=\"py-3 px-4 text-center\">\n");  
        fprintf(f, "                        <button @click=\"expandedBook = expandedBook === %d ? null : %d\" "  
                    "class=\"px-3 py-1 rounded bg-blue-100 text-blue-700 hover:bg-blue-200 focus:outline-none "  
                    "focus:ring-2 focus:ring-blue-300 transition-colors\">\n",   
                    book_id, book_id);  
        fprintf(f, "                            <span x-text=\"expandedBook === %d ? '收起' : '查看文档'\"></span>\n",   
                    book_id);  
        fprintf(f, "                        </button>\n");  
        fprintf(f, "                    </td>\n");  
        fprintf(f, "                </tr>\n");  

        // Expandable row for book's documents  
        fprintf(f, "                <tr x-show=\"expandedBook === %d\" x-cloak>\n", book_id);  
        fprintf(f, "                    <td colspan=\"4\" class=\"p-0 bg-gray-50\">\n");  
        fprintf(f, "                        <div class=\"p-4\">\n");  
        fprintf(f, "                            <h4 class=\"font-medium text-gray-800 mb-2\">文档列表</h4>\n");  
        fprintf(f, "                            <div class=\"overflow-y-auto max-h-96\">\n");  
        fprintf(f, "                                <table class=\"min-w-full divide-y divide-gray-200\">\n");  
        fprintf(f, "                                    <thead class=\"bg-gray-100\">\n");  
        fprintf(f, "                                        <tr>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">文档名称</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">作者</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">阅读次数</th>\n");  
        fprintf(f, "                                            <th class=\"py-2 px-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider\">更新时间</th>\n");  
        fprintf(f, "                                        </tr>\n");  
        fprintf(f, "                                    </thead>\n");  
        fprintf(f, "                                    <tbody class=\"divide-y divide-gray-200\">\n");  

        // Query the book's documents  
        char doc_query[MAX_QUERY_LEN];  
        snprintf(doc_query, MAX_QUERY_LEN,  
            "SELECT d.document_name, d.identify as doc_identify, d.view_count, d.modify_time, "  
            "m.account, m.real_name "  
            "FROM md_documents d "  
            "LEFT JOIN md_members m ON d.member_id = m.member_id "  
            "WHERE d.book_id = %d "  
            "ORDER BY d.view_count DESC, d.modify_time DESC", book_id);  

        sqlite3_stmt *doc_stmt;  
        rc = sqlite3_prepare_v2(db, doc_query, -1, &doc_stmt, NULL);  
        
        if (rc == SQLITE_OK) {  
            while (sqlite3_step(doc_stmt) == SQLITE_ROW) {  
                const char *doc_name = (const char*)sqlite3_column_text(doc_stmt, 0);  
                const char *doc_identify = (const char*)sqlite3_column_text(doc_stmt, 1);  
                int view_count = sqlite3_column_int(doc_stmt, 2);  
                const char *modify_time = (const char*)sqlite3_column_text(doc_stmt, 3);  
                const char *account = (const char*)sqlite3_column_text(doc_stmt, 4);  
                const char *real_name = (const char*)sqlite3_column_text(doc_stmt, 5);  

                // Generate document link  
                char doc_link[MAX_PATH_LEN];  
                snprintf(doc_link, MAX_PATH_LEN, "%s/%s/%s",   
                        config.link_prefix,   
                        identify,   
                        doc_identify ? doc_identify : "");  

                // Ensure names aren't NULL for display  
                if (!doc_name) doc_name = "";  
                if (!account) account = "";  
                if (!real_name) real_name = "";  
                if (!modify_time) modify_time = "";  

                // Display document row  
                fprintf(f, "                                        <tr class=\"hover:bg-gray-100\">\n");  
                fprintf(f, "                                            <td class=\"py-2 px-3 font-medium text-blue-600\">"  
                        "<a href=\"%s\" target=\"_blank\" class=\"hover:underline\">%s</a></td>\n",   
                        doc_link, doc_name);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%s</td>\n",   
                        real_name && real_name[0] ? real_name : account);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%d</td>\n", view_count);  
                fprintf(f, "                                            <td class=\"py-2 px-3 text-gray-500\">%s</td>\n", modify_time);  
                fprintf(f, "                                        </tr>\n");  
            }  
            sqlite3_finalize(doc_stmt);  
        }  

        fprintf(f, "                                    </tbody>\n");  
        fprintf(f, "                                </table>\n");  
        fprintf(f, "                            </div>\n");  
        fprintf(f, "                        </div>\n");  
        fprintf(f, "                    </td>\n");  
        fprintf(f, "                </tr>\n");  
    }  

    sqlite3_finalize(stmt);  
    
    fprintf(f, "                </tbody>\n");  
    fprintf(f, "            </table>\n");  
    fprintf(f, "        </div>\n");  
}