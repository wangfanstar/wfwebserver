#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <time.h>  
#include <sqlite3.h>  
#include <stdbool.h>  

#define CONFIG_MAX_LINE 1024  
#define MAX_SORT_BOOKS 50  
#define MAX_BOOK_NAME_LEN 256  
#define MAX_SQL_LEN 4096  
#define DEFAULT_CONFIG_PATH "./mindocsort.conf"  

typedef struct {  
    int recycle_time;  
    char db_path[CONFIG_MAX_LINE];  
    char html_path[CONFIG_MAX_LINE];  
    char sort_books[MAX_SORT_BOOKS][MAX_BOOK_NAME_LEN];  
    int sort_books_count;  
    char link_prefix[CONFIG_MAX_LINE];  
} Config;  

// Function prototypes  
bool load_config(const char *config_path, Config *config);  
bool generate_html(Config *config);  
char *get_time_condition(const char *timeframe);  
void generate_latest_documents_html(FILE *fp, sqlite3 *db, Config *config);  
void generate_popular_documents_html(FILE *fp, sqlite3 *db, Config *config);  
void generate_user_rankings_html(FILE *fp, sqlite3 *db, Config *config);  
void generate_group_rankings_html(FILE *fp, sqlite3 *db, Config *config);  

// Config file parsing  
bool load_config(const char *config_path, Config *config) {  
    FILE *fp = fopen(config_path, "r");  
    if (!fp) {  
        fprintf(stderr, "Failed to open config file: %s\n", config_path);  
        return false;  
    }  

    char line[CONFIG_MAX_LINE];  
    config->sort_books_count = 0;  

    while (fgets(line, sizeof(line), fp)) {  
        // Skip comments and empty lines  
        if (line[0] == '#' || line[0] == '\n') {  
            continue;  
        }  

        char *key = strtok(line, "=");  
        char *value = strtok(NULL, "\n");  

        if (!key || !value) {  
            continue;  
        }  

        // Trim whitespace  
        while (*key && (*key == ' ' || *key == '\t')) key++;  
        while (*value && (*value == ' ' || *value == '\t')) value++;  

        char *end = key + strlen(key) - 1;  
        while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';  
        
        end = value + strlen(value) - 1;  
        while (end > value && (*end == ' ' || *end == '\t')) *end-- = '\0';  

        if (strcmp(key, "recycle_time") == 0) {  
            config->recycle_time = atoi(value);  
        } else if (strcmp(key, "db_path") == 0) {  
            strncpy(config->db_path, value, CONFIG_MAX_LINE - 1);  
        } else if (strcmp(key, "html_path") == 0) {  
            strncpy(config->html_path, value, CONFIG_MAX_LINE - 1);  
        } else if (strcmp(key, "link_prefix") == 0) {  
            strncpy(config->link_prefix, value, CONFIG_MAX_LINE - 1);  
        } else if (strcmp(key, "sort_books") == 0) {  
            char *book = strtok(value, ",");  
            while (book && config->sort_books_count < MAX_SORT_BOOKS) {  
                // Trim whitespace  
                while (*book && (*book == ' ' || *book == '\t')) book++;  
                char *end = book + strlen(book) - 1;  
                while (end > book && (*end == ' ' || *end == '\t')) *end-- = '\0';  
                
                strncpy(config->sort_books[config->sort_books_count], book, MAX_BOOK_NAME_LEN - 1);  
                config->sort_books[config->sort_books_count][MAX_BOOK_NAME_LEN - 1] = '\0';  
                config->sort_books_count++;  
                book = strtok(NULL, ",");  
            }  
        }  
    }  

    fclose(fp);  
    return true;  
}  

// Helper function to get the time condition for SQL queries  
char *get_time_condition(const char *timeframe) {  
    static char condition[256];  
    
    if (strcmp(timeframe, "week") == 0) {  
        strcpy(condition, "AND md_documents.modify_time >= datetime('now', '-7 days')");  
    } else if (strcmp(timeframe, "month") == 0) {  
        strcpy(condition, "AND md_documents.modify_time >= datetime('now', '-30 days')");  
    } else {  
        condition[0] = '\0';  // All time, no condition  
    }  
    
    return condition;  
}  

// Generate HTML for latest updated documents  
void generate_latest_documents_html(FILE *fp, sqlite3 *db, Config *config) {  
    sqlite3_stmt *stmt;  
    const char *sql =   
        "WITH latest_history AS ("  
        "    SELECT document_id, MAX(modify_time) as latest_time"  
        "    FROM md_document_history"  
        "    GROUP BY document_id"  
        ")"  
        "SELECT md_documents.document_id, md_documents.document_name, "  
        "       md_documents.identify as doc_identify, "  
        "       author.account as author_account, author.real_name as author_real_name, "  
        "       modifier.account as modifier_account, modifier.real_name as modifier_real_name, "  
        "       md_books.book_name, md_books.identify as book_identify, "  
        "       md_documents.modify_time "  
        "FROM latest_history "  
        "JOIN md_documents ON latest_history.document_id = md_documents.document_id "  
        "JOIN md_members as author ON md_documents.member_id = author.member_id "  
        "JOIN md_members as modifier ON md_documents.modify_at = modifier.member_id "  
        "JOIN md_books ON md_documents.book_id = md_books.book_id "  
        "ORDER BY latest_history.latest_time DESC "  
        "LIMIT 100";  

    fprintf(fp, "<div x-show=\"currentView === 'latest'\" class=\"w-full\">\n");  
    fprintf(fp, "  <h2 class=\"text-2xl font-bold mb-4\">最新更新的文档</h2>\n");  
    fprintf(fp, "  <div class=\"overflow-x-auto\">\n");  
    fprintf(fp, "    <table class=\"min-w-full bg-white border border-gray-300\">\n");  
    fprintf(fp, "      <thead class=\"bg-gray-100\">\n");  
    fprintf(fp, "        <tr>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">文档名称</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">作者</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">最后修改者</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">所属知识库</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">最后更新时间</th>\n");  
    fprintf(fp, "        </tr>\n");  
    fprintf(fp, "      </thead>\n");  
    fprintf(fp, "      <tbody>\n");  

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
    } else {  
        while (sqlite3_step(stmt) == SQLITE_ROW) {  
            const char *doc_name = (const char *)sqlite3_column_text(stmt, 1);  
            const char *doc_identify = (const char *)sqlite3_column_text(stmt, 2);  
            const char *author_account = (const char *)sqlite3_column_text(stmt, 3);  
            const char *author_real_name = (const char *)sqlite3_column_text(stmt, 4);  
            const char *modifier_account = (const char *)sqlite3_column_text(stmt, 5);  
            const char *modifier_real_name = (const char *)sqlite3_column_text(stmt, 6);  
            const char *book_name = (const char *)sqlite3_column_text(stmt, 7);  
            const char *book_identify = (const char *)sqlite3_column_text(stmt, 8);  
            const char *modify_time = (const char *)sqlite3_column_text(stmt, 9);  

            // Format document link  
            char doc_link[CONFIG_MAX_LINE * 2];  
            snprintf(doc_link, sizeof(doc_link), "%s/%s/%s",   
                     config->link_prefix, book_identify, doc_identify);  

            // Display author name or account  
            const char *author_display = author_real_name && strlen(author_real_name) > 0 ?   
                                         author_real_name : author_account;  
            
            // Format author display with account and real name  
            char author_info[512];  
            if (author_real_name && strlen(author_real_name) > 0) {  
                snprintf(author_info, sizeof(author_info), "%s（%s）", author_account, author_real_name);  
            } else {  
                snprintf(author_info, sizeof(author_info), "%s", author_account);  
            }  
            
            // Format modifier display with account and real name  
            char modifier_info[512];  
            if (modifier_real_name && strlen(modifier_real_name) > 0) {  
                snprintf(modifier_info, sizeof(modifier_info), "%s（%s）", modifier_account, modifier_real_name);  
            } else {  
                snprintf(modifier_info, sizeof(modifier_info), "%s", modifier_account);  
            }  

            fprintf(fp, "        <tr class=\"hover:bg-gray-50\">\n");  
            fprintf(fp, "          <td class=\"px-4 py-2 border\"><a href=\"%s\" class=\"text-blue-600 hover:underline\" target=\"_blank\">%s</a></td>\n",   
                   doc_link, doc_name);  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", author_info);  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", modifier_info);  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", book_name);  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", modify_time);  
            fprintf(fp, "        </tr>\n");  
        }  
        sqlite3_finalize(stmt);  
    }  

    fprintf(fp, "      </tbody>\n");  
    fprintf(fp, "    </table>\n");  
    fprintf(fp, "  </div>\n");  
    fprintf(fp, "</div>\n");  
}  

// Main function  
int main(int argc, char *argv[]) {  
    Config config;  
    const char *config_path = (argc > 1) ? argv[1] : DEFAULT_CONFIG_PATH;  
    
    if (!load_config(config_path, &config)) {  
        fprintf(stderr, "Failed to load configuration.\n");  
        return 1;  
    }  
    
    printf("Configuration loaded. Starting document ranking generator...\n");  
    printf("Database path: %s\n", config.db_path);  
    printf("HTML output path: %s\n", config.html_path);  
    printf("Update cycle: %d minutes\n", config.recycle_time);  
    
    // Main loop for periodic updates  
    while (1) {  
        printf("Generating HTML...\n");  
        if (generate_html(&config)) {  
            printf("HTML generated successfully at %s\n", config.html_path);  
        } else {  
            fprintf(stderr, "Failed to generate HTML.\n");  
        }  
        
        // Sleep until next update  
        sleep(config.recycle_time * 60);  
    }  
    
    return 0;  
}  

// Generate HTML for popular documents  
void generate_popular_documents_html(FILE *fp, sqlite3 *db, Config *config) {  
    fprintf(fp, "<div x-show=\"currentView === 'popular'\" class=\"w-full\">\n");  
    fprintf(fp, "  <h2 class=\"text-2xl font-bold mb-4\">最受欢迎文档</h2>\n");  
    fprintf(fp, "  <div class=\"mb-4\">\n");  
    fprintf(fp, "    <button @click=\"popularTimeframe = 'all'\" :class=\"popularTimeframe === 'all' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg mr-2\">所有时间</button>\n");  
    fprintf(fp, "    <button @click=\"popularTimeframe = 'month'\" :class=\"popularTimeframe === 'month' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg mr-2\">过去一个月</button>\n");  
    fprintf(fp, "    <button @click=\"popularTimeframe = 'week'\" :class=\"popularTimeframe === 'week' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">过去一周</button>\n");  
    fprintf(fp, "  </div>\n");  

    // Generate tables for each timeframe (all, month, week)  
    const char *timeframes[] = {"all", "month", "week"};  
    for (int i = 0; i < 3; i++) {  
        const char *timeframe = timeframes[i];  
        char sql[MAX_SQL_LEN];  
        snprintf(sql, sizeof(sql),  
            "SELECT md_documents.document_id, md_documents.document_name, "  
            "       md_documents.identify as doc_identify, "  
            "       author.account as author_account, author.real_name as author_real_name, "  
            "       modifier.account as modifier_account, modifier.real_name as modifier_real_name, "  
            "       md_books.book_name, md_books.identify as book_identify, "  
            "       md_documents.view_count, md_documents.modify_time "  
            "FROM md_documents "  
            "JOIN md_members as author ON md_documents.member_id = author.member_id "  
            "JOIN md_members as modifier ON md_documents.modify_at = modifier.member_id "  
            "JOIN md_books ON md_documents.book_id = md_books.book_id "  
            "%s "  // Time condition will be inserted here  
            "ORDER BY md_documents.view_count DESC "  
            "LIMIT 100",   
            get_time_condition(timeframe));  

        fprintf(fp, "  <div x-show=\"popularTimeframe === '%s'\" class=\"overflow-x-auto\">\n", timeframe);  
        fprintf(fp, "    <table class=\"min-w-full bg-white border border-gray-300\">\n");  
        fprintf(fp, "      <thead class=\"bg-gray-100\">\n");  
        fprintf(fp, "        <tr>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">排名</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">文档名称</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">作者</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">最后修改者</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">所属知识库</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">阅读次数</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">最后更新时间</th>\n");  
        fprintf(fp, "        </tr>\n");  
        fprintf(fp, "      </thead>\n");  
        fprintf(fp, "      <tbody>\n");  

        sqlite3_stmt *stmt;  
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {  
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        } else {  
            int rank = 1;  
            while (sqlite3_step(stmt) == SQLITE_ROW) {  
                const char *doc_name = (const char *)sqlite3_column_text(stmt, 1);  
                const char *doc_identify = (const char *)sqlite3_column_text(stmt, 2);  
                const char *author_account = (const char *)sqlite3_column_text(stmt, 3);  
                const char *author_real_name = (const char *)sqlite3_column_text(stmt, 4);  
                const char *modifier_account = (const char *)sqlite3_column_text(stmt, 5);  
                const char *modifier_real_name = (const char *)sqlite3_column_text(stmt, 6);  
                const char *book_name = (const char *)sqlite3_column_text(stmt, 7);  
                const char *book_identify = (const char *)sqlite3_column_text(stmt, 8);  
                int view_count = sqlite3_column_int(stmt, 9);  
                const char *modify_time = (const char *)sqlite3_column_text(stmt, 10);  

                // Format document link  
                char doc_link[CONFIG_MAX_LINE * 2];  
                snprintf(doc_link, sizeof(doc_link), "%s/%s/%s",   
                         config->link_prefix, book_identify, doc_identify);  

                // Display author name or account  
                const char *author_display = author_real_name && strlen(author_real_name) > 0 ?   
                                             author_real_name : author_account;  
                
                // Display modifier name or account  
                const char *modifier_display = modifier_real_name && strlen(modifier_real_name) > 0 ?   
                                               modifier_real_name : modifier_account;  

                fprintf(fp, "        <tr class=\"hover:bg-gray-50\">\n");  
                fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", rank++);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\"><a href=\"%s\" class=\"text-blue-600 hover:underline\" target=\"_blank\">%s</a></td>\n",   
                       doc_link, doc_name);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", author_display);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", modifier_display);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", book_name);  
                fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", view_count);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", modify_time);  
                fprintf(fp, "        </tr>\n");  
            }  
            sqlite3_finalize(stmt);  
        }  

        fprintf(fp, "      </tbody>\n");  
        fprintf(fp, "    </table>\n");  
        fprintf(fp, "  </div>\n");  
    }  
    
    fprintf(fp, "</div>\n");  
}  

// Generate HTML for user document rankings  
void generate_user_rankings_html(FILE *fp, sqlite3 *db, Config *config) {  
    fprintf(fp, "<div x-show=\"currentView === 'user'\" class=\"w-full\">\n");  
    fprintf(fp, "  <h2 class=\"text-2xl font-bold mb-4\">个人文档排名</h2>\n");  
    fprintf(fp, "  <div class=\"mb-4\">\n");  
    fprintf(fp, "    <button @click=\"userTimeframe = 'all'\" :class=\"userTimeframe === 'all' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg mr-2\">所有时间</button>\n");  
    fprintf(fp, "    <button @click=\"userTimeframe = 'month'\" :class=\"userTimeframe === 'month' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg mr-2\">过去一个月</button>\n");  
    fprintf(fp, "    <button @click=\"userTimeframe = 'week'\" :class=\"userTimeframe === 'week' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">过去一周</button>\n");  
    fprintf(fp, "  </div>\n");  

    // Generate tables for each timeframe (all, month, week)  
    const char *timeframes[] = {"all", "month", "week"};  
    for (int i = 0; i < 3; i++) {  
        const char *timeframe = timeframes[i];  
        char sql[MAX_SQL_LEN];  
        
        // Query to get authors and their document counts  
        snprintf(sql, sizeof(sql),  
            "SELECT author.member_id, author.account, author.real_name, "  
            "       COUNT(DISTINCT md_documents.document_id) as doc_count, "  
            "       GROUP_CONCAT(DISTINCT modifier.account) as modifier_accounts, "  
            "       GROUP_CONCAT(DISTINCT modifier.real_name) as modifier_real_names "  
            "FROM md_documents "  
            "JOIN md_members as author ON md_documents.member_id = author.member_id "  
            "JOIN md_members as modifier ON md_documents.modify_at = modifier.member_id "  
            "%s "  // Time condition will be inserted here  
            "GROUP BY author.member_id "  
            "ORDER BY doc_count DESC",   
            get_time_condition(timeframe));  

        fprintf(fp, "  <div x-show=\"userTimeframe === '%s'\" class=\"overflow-x-auto\">\n", timeframe);  
        fprintf(fp, "    <table class=\"min-w-full bg-white border border-gray-300\">\n");  
        fprintf(fp, "      <thead class=\"bg-gray-100\">\n");  
        fprintf(fp, "        <tr>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">排名</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">作者</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">最后修改者</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">文档数量</th>\n");  
        fprintf(fp, "          <th class=\"px-4 py-2 border\">文档列表</th>\n");  
        fprintf(fp, "        </tr>\n");  
        fprintf(fp, "      </thead>\n");  
        fprintf(fp, "      <tbody>\n");  

        sqlite3_stmt *stmt;  
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {  
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
        } else {  
            int rank = 1;  
            while (sqlite3_step(stmt) == SQLITE_ROW) {  
                int author_id = sqlite3_column_int(stmt, 0);  
                const char *author_account = (const char *)sqlite3_column_text(stmt, 1);  
                const char *author_real_name = (const char *)sqlite3_column_text(stmt, 2);  
                int doc_count = sqlite3_column_int(stmt, 3);  
                
                // Display author name or account  
                const char *author_display = author_real_name && strlen(author_real_name) > 0 ?   
                                             author_real_name : author_account;  
                
                // Get unique modifiers  
                const char *modifier_accounts = (const char *)sqlite3_column_text(stmt, 4);  
                const char *modifier_real_names = (const char *)sqlite3_column_text(stmt, 5);  
                
                char modifiers_display[1024] = "";  
                if (modifier_accounts && modifier_real_names) {  
                    // This is simplified - in a real implementation, you might want to  
                    // parse these concatenated lists and match accounts with real names  
                    strncpy(modifiers_display, modifier_real_names[0] ? modifier_real_names : modifier_accounts, 1023);  
                }  

                fprintf(fp, "        <tr class=\"hover:bg-gray-50\">\n");  
                fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", rank++);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", author_display);  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", modifiers_display);  
                fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", doc_count);  
                
                // Button to show document list (using Alpine.js)  
                fprintf(fp, "          <td class=\"px-4 py-2 border\">\n");  
                fprintf(fp, "            <div x-data=\"{open: false}\">\n");  
                fprintf(fp, "              <button @click=\"open = !open\" class=\"bg-blue-500 hover:bg-blue-700 text-white font-bold py-1 px-3 rounded\">\n");  
                fprintf(fp, "                <span x-text=\"open ? '隐藏' : '查看'\"></span>\n");  
                fprintf(fp, "              </button>\n");  
                fprintf(fp, "              <div x-show=\"open\" class=\"mt-2\">\n");  
                
                // Get documents for this author  
                char doc_sql[MAX_SQL_LEN];  
                snprintf(doc_sql, sizeof(doc_sql),  
                    "SELECT md_documents.document_name, md_documents.identify as doc_identify, "  
                    "       md_books.book_name, md_books.identify as book_identify "  
                    "FROM md_documents "  
                    "JOIN md_books ON md_documents.book_id = md_books.book_id "  
                    "WHERE md_documents.member_id = %d %s "  
                    "ORDER BY md_documents.modify_time DESC",  
                    author_id, get_time_condition(timeframe));  
                
                sqlite3_stmt *doc_stmt;  
                if (sqlite3_prepare_v2(db, doc_sql, -1, &doc_stmt, NULL) == SQLITE_OK) {  
                    fprintf(fp, "                <ul class=\"list-disc pl-5\">\n");  
                    while (sqlite3_step(doc_stmt) == SQLITE_ROW) {  
                        const char *doc_name = (const char *)sqlite3_column_text(doc_stmt, 0);  
                        const char *doc_identify = (const char *)sqlite3_column_text(doc_stmt, 1);  
                        const char *book_name = (const char *)sqlite3_column_text(doc_stmt, 2);  
                        const char *book_identify = (const char *)sqlite3_column_text(doc_stmt, 3);  
                        
                        char doc_link[CONFIG_MAX_LINE * 2];  
                        snprintf(doc_link, sizeof(doc_link), "%s/%s/%s",   
                                 config->link_prefix, book_identify, doc_identify);  
                        
                        fprintf(fp, "                  <li><a href=\"%s\" class=\"text-blue-600 hover:underline\" target=\"_blank\">%s</a> [%s]</li>\n",   
                               doc_link, doc_name, book_name);  
                    }  
                    fprintf(fp, "                </ul>\n");  
                    sqlite3_finalize(doc_stmt);  
                }  
                
                fprintf(fp, "              </div>\n");  
                fprintf(fp, "            </div>\n");  
                fprintf(fp, "          </td>\n");  
                fprintf(fp, "        </tr>\n");  
            }  
            sqlite3_finalize(stmt);  
        }  

        fprintf(fp, "      </tbody>\n");  
        fprintf(fp, "    </table>\n");  
        fprintf(fp, "  </div>\n");  
    }  
    
    fprintf(fp, "</div>\n");  
}  

// Generate HTML for group document rankings  
void generate_group_rankings_html(FILE *fp, sqlite3 *db, Config *config) {  
    fprintf(fp, "<div x-show=\"currentView === 'group'\" class=\"w-full\">\n");  
    fprintf(fp, "  <h2 class=\"text-2xl font-bold mb-4\">组内文档排名</h2>\n");  
    fprintf(fp, "  <div class=\"mb-4\">\n");  
    fprintf(fp, "    <button @click=\"groupType = 'specified'\" :class=\"groupType === 'specified' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg mr-2\">指定配置组文档排名</button>\n");  
    fprintf(fp, "    <button @click=\"groupType = 'all'\" :class=\"groupType === 'all' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">所有组文档排名</button>\n");  
    fprintf(fp, "  </div>\n");  

    // Generate specified groups ranking  
    fprintf(fp, "  <div x-show=\"groupType === 'specified'\" class=\"overflow-x-auto\">\n");  
    fprintf(fp, "    <table class=\"min-w-full bg-white border border-gray-300\">\n");  
    fprintf(fp, "      <thead class=\"bg-gray-100\">\n");  
    fprintf(fp, "        <tr>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">排名</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">知识库名称</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">文档数量</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">文档列表</th>\n");  
    fprintf(fp, "        </tr>\n");  
    fprintf(fp, "      </thead>\n");  
    fprintf(fp, "      <tbody>\n");  

    // Build a SQL query for specified books  
    char specified_books_sql[MAX_SQL_LEN] =   
        "SELECT md_books.book_id, md_books.book_name, md_books.identify, "  
        "       COUNT(md_documents.document_id) as doc_count "  
        "FROM md_books "  
        "LEFT JOIN md_documents ON md_books.book_id = md_documents.book_id "  
        "WHERE md_books.book_name IN (";  
    
    for (int i = 0; i < config->sort_books_count; i++) {  
        char escaped_name[MAX_BOOK_NAME_LEN * 2];  
        // Simple escaping for single quotes in SQL  
        char *s = config->sort_books[i];  
        char *d = escaped_name;  
        while (*s) {  
            if (*s == '\'') *d++ = '\'';  // Double single quotes for SQL  
            *d++ = *s++;  
        }  
        *d = '\0';  
        
        if (i > 0) strcat(specified_books_sql, ", ");  
        strcat(specified_books_sql, "'");  
        strcat(specified_books_sql, escaped_name);  
        strcat(specified_books_sql, "'");  
    }  
    
    strcat(specified_books_sql, ") GROUP BY md_books.book_id ORDER BY doc_count DESC");  

    sqlite3_stmt *stmt;  
    if (sqlite3_prepare_v2(db, specified_books_sql, -1, &stmt, NULL) != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
    } else {  
        int rank = 1;  
        while (sqlite3_step(stmt) == SQLITE_ROW) {  
            int book_id = sqlite3_column_int(stmt, 0);  
            const char *book_name = (const char *)sqlite3_column_text(stmt, 1);  
            const char *book_identify = (const char *)sqlite3_column_text(stmt, 2);  
            int doc_count = sqlite3_column_int(stmt, 3);  

            fprintf(fp, "        <tr class=\"hover:bg-gray-50\">\n");  
            fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", rank++);  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", book_name);  
            fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", doc_count);  
            
            // Button to show document list (using Alpine.js)  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">\n");  
            fprintf(fp, "            <div x-data=\"{open: false}\">\n");  
            fprintf(fp, "              <button @click=\"open = !open\" class=\"bg-blue-500 hover:bg-blue-700 text-white font-bold py-1 px-3 rounded\">\n");  
            fprintf(fp, "                <span x-text=\"open ? '隐藏' : '查看'\"></span>\n");  
            fprintf(fp, "              </button>\n");  
            fprintf(fp, "              <div x-show=\"open\" class=\"mt-2\">\n");  
            
            // Get documents for this book  
            char doc_sql[MAX_SQL_LEN];  
            snprintf(doc_sql, sizeof(doc_sql),  
                "SELECT md_documents.document_name, md_documents.identify as doc_identify, "  
                "       author.account as author_account, author.real_name as author_real_name "  
                "FROM md_documents "  
                "JOIN md_members as author ON md_documents.member_id = author.member_id "  
                "WHERE md_documents.book_id = %d "  
                "ORDER BY md_documents.modify_time DESC",  
                book_id);  
            
            sqlite3_stmt *doc_stmt;  
            if (sqlite3_prepare_v2(db, doc_sql, -1, &doc_stmt, NULL) == SQLITE_OK) {  
                fprintf(fp, "                <ul class=\"list-disc pl-5\">\n");  
                while (sqlite3_step(doc_stmt) == SQLITE_ROW) {  
                    const char *doc_name = (const char *)sqlite3_column_text(doc_stmt, 0);  
                    const char *doc_identify = (const char *)sqlite3_column_text(doc_stmt, 1);  
                    const char *author_account = (const char *)sqlite3_column_text(doc_stmt, 2);  
                    const char *author_real_name = (const char *)sqlite3_column_text(doc_stmt, 3);  
                    
                    // Display author name or account  
                    const char *author_display = author_real_name && strlen(author_real_name) > 0 ?   
                                                 author_real_name : author_account;  
                    
                    char doc_link[CONFIG_MAX_LINE * 2];  
                    snprintf(doc_link, sizeof(doc_link), "%s/%s/%s",   
                             config->link_prefix, book_identify, doc_identify);  
                    
                    fprintf(fp, "                  <li><a href=\"%s\" class=\"text-blue-600 hover:underline\" target=\"_blank\">%s</a> [%s]</li>\n",   
                           doc_link, doc_name, author_display);  
                }  
                fprintf(fp, "                </ul>\n");  
                sqlite3_finalize(doc_stmt);  
            }  
            
            fprintf(fp, "              </div>\n");  
            fprintf(fp, "            </div>\n");  
            fprintf(fp, "          </td>\n");  
            fprintf(fp, "        </tr>\n");  
        }  
        sqlite3_finalize(stmt);  
    }  

    fprintf(fp, "      </tbody>\n");  
    fprintf(fp, "    </table>\n");  
    fprintf(fp, "  </div>\n");  

    // Generate all groups ranking  
    fprintf(fp, "  <div x-show=\"groupType === 'all'\" class=\"overflow-x-auto\">\n");  
    fprintf(fp, "    <table class=\"min-w-full bg-white border border-gray-300\">\n");  
    fprintf(fp, "      <thead class=\"bg-gray-100\">\n");  
    fprintf(fp, "        <tr>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">排名</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">知识库名称</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">文档数量</th>\n");  
    fprintf(fp, "          <th class=\"px-4 py-2 border\">文档列表</th>\n");  
    fprintf(fp, "        </tr>\n");  
    fprintf(fp, "      </thead>\n");  
    fprintf(fp, "      <tbody>\n");  

    char all_books_sql[MAX_SQL_LEN] =   
        "SELECT md_books.book_id, md_books.book_name, md_books.identify, "  
        "       COUNT(md_documents.document_id) as doc_count "  
        "FROM md_books "  
        "LEFT JOIN md_documents ON md_books.book_id = md_documents.book_id "  
        "GROUP BY md_books.book_id "  
        "ORDER BY doc_count DESC";  

    if (sqlite3_prepare_v2(db, all_books_sql, -1, &stmt, NULL) != SQLITE_OK) {  
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));  
    } else {  
        int rank = 1;  
        while (sqlite3_step(stmt) == SQLITE_ROW) {  
            int book_id = sqlite3_column_int(stmt, 0);  
            const char *book_name = (const char *)sqlite3_column_text(stmt, 1);  
            const char *book_identify = (const char *)sqlite3_column_text(stmt, 2);  
            int doc_count = sqlite3_column_int(stmt, 3);  

            fprintf(fp, "        <tr class=\"hover:bg-gray-50\">\n");  
            fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", rank++);  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">%s</td>\n", book_name);  
            fprintf(fp, "          <td class=\"px-4 py-2 border text-center\">%d</td>\n", doc_count);  
            
            // Button to show document list (using Alpine.js)  
            fprintf(fp, "          <td class=\"px-4 py-2 border\">\n");  
            fprintf(fp, "            <div x-data=\"{open: false}\">\n");  
            fprintf(fp, "              <button @click=\"open = !open\" class=\"bg-blue-500 hover:bg-blue-700 text-white font-bold py-1 px-3 rounded\">\n");  
            fprintf(fp, "                <span x-text=\"open ? '隐藏' : '查看'\"></span>\n");  
            fprintf(fp, "              </button>\n");  
            fprintf(fp, "              <div x-show=\"open\" class=\"mt-2\">\n");  
            
            // Get documents for this book  
            char doc_sql[MAX_SQL_LEN];  
            snprintf(doc_sql, sizeof(doc_sql),  
                "SELECT md_documents.document_name, md_documents.identify as doc_identify, "  
                "       author.account as author_account, author.real_name as author_real_name "  
                "FROM md_documents "  
                "JOIN md_members as author ON md_documents.member_id = author.member_id "  
                "WHERE md_documents.book_id = %d "  
                "ORDER BY md_documents.modify_time DESC "  
                "LIMIT 50",  // Limit to 50 documents per book for performance  
                book_id);  
            
            sqlite3_stmt *doc_stmt;  
            if (sqlite3_prepare_v2(db, doc_sql, -1, &doc_stmt, NULL) == SQLITE_OK) {  
                fprintf(fp, "                <ul class=\"list-disc pl-5\">\n");  
                while (sqlite3_step(doc_stmt) == SQLITE_ROW) {  
                    const char *doc_name = (const char *)sqlite3_column_text(doc_stmt, 0);  
                    const char *doc_identify = (const char *)sqlite3_column_text(doc_stmt, 1);  
                    const char *author_account = (const char *)sqlite3_column_text(doc_stmt, 2);  
                    const char *author_real_name = (const char *)sqlite3_column_text(doc_stmt, 3);  
                    
                    // Display author name or account  
                    const char *author_display = author_real_name && strlen(author_real_name) > 0 ?   
                                                 author_real_name : author_account;  
                    
                    char doc_link[CONFIG_MAX_LINE * 2];  
                    snprintf(doc_link, sizeof(doc_link), "%s/%s/%s",   
                             config->link_prefix, book_identify, doc_identify);  
                    
                    fprintf(fp, "                  <li><a href=\"%s\" class=\"text-blue-600 hover:underline\" target=\"_blank\">%s</a> [%s]</li>\n",   
                           doc_link, doc_name, author_display);  
                }  
                fprintf(fp, "                </ul>\n");  
                sqlite3_finalize(doc_stmt);  
            }  
            
            fprintf(fp, "              </div>\n");  
            fprintf(fp, "            </div>\n");  
            fprintf(fp, "          </td>\n");  
            fprintf(fp, "        </tr>\n");  
        }  
        sqlite3_finalize(stmt);  
    }  

    fprintf(fp, "      </tbody>\n");  
    fprintf(fp, "    </table>\n");  
    fprintf(fp, "  </div>\n");  
    
    fprintf(fp, "</div>\n");  
}  

// Generate the complete HTML page  
bool generate_html(Config *config) {  
    sqlite3 *db;  
    
    // Open database  
    if (sqlite3_open(config->db_path, &db) != SQLITE_OK) {  
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));  
        sqlite3_close(db);  
        return false;  
    }  
    
    // Open output file  
    FILE *fp = fopen(config->html_path, "w");  
    if (!fp) {  
        fprintf(stderr, "Cannot open output file: %s\n", config->html_path);  
        sqlite3_close(db);  
        return false;  
    }  
    
    // Get current time  
    time_t t = time(NULL);  
    struct tm *tm_info = localtime(&t);  
    char timestamp[64];  
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);  
    
    // Write HTML header  
    fprintf(fp, "<!DOCTYPE html>\n");  
    fprintf(fp, "<html lang=\"zh-CN\">\n");  
    fprintf(fp, "<head>\n");  
    fprintf(fp, "  <meta charset=\"UTF-8\">\n");  
    fprintf(fp, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");  
    fprintf(fp, "  <title>MinDoc 文档统计排名</title>\n");  
    fprintf(fp, "  <link href=\"tailwind.min.css\" rel=\"stylesheet\">\n");  
    fprintf(fp, "  <script src=\"alpine.min.js\" defer></script>\n");  
    fprintf(fp, "</head>\n");  
    fprintf(fp, "<body class=\"bg-gray-100 min-h-screen\">\n");  
    
    // Alpine.js app  
    fprintf(fp, "<div x-data=\"{ \n");  
    fprintf(fp, "  currentView: 'latest', \n");  
    fprintf(fp, "  popularTimeframe: 'all',\n");  
    fprintf(fp, "  userTimeframe: 'all',\n");  
    fprintf(fp, "  groupType: 'specified'\n");  
    fprintf(fp, "}\" class=\"container mx-auto px-4 py-8\">\n");  
    
    // Header  
    fprintf(fp, "  <header class=\"bg-white shadow-md rounded-lg p-6 mb-6\">\n");  
    fprintf(fp, "    <h1 class=\"text-3xl font-bold text-gray-800 mb-2\">MinDoc 文档统计排名</h1>\n");  
    fprintf(fp, "    <p class=\"text-gray-600\">数据更新时间: %s</p>\n", timestamp);  
    fprintf(fp, "  </header>\n");  
    
    // Navigation  
    fprintf(fp, "  <nav class=\"bg-white shadow-md rounded-lg p-4 mb-6 sticky top-0 z-10\">\n");  
    fprintf(fp, "    <div class=\"flex space-x-4\">\n");  
    fprintf(fp, "      <button @click=\"currentView = 'latest'\" :class=\"currentView === 'latest' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">最新更新文档</button>\n");  
    fprintf(fp, "      <button @click=\"currentView = 'popular'\" :class=\"currentView === 'popular' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">最受欢迎文档</button>\n");  
    fprintf(fp, "      <button @click=\"currentView = 'user'\" :class=\"currentView === 'user' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">个人文档排名</button>\n");  
    fprintf(fp, "      <button @click=\"currentView = 'group'\" :class=\"currentView === 'group' ? 'bg-blue-600 text-white' : 'bg-gray-200'\" class=\"px-4 py-2 rounded-lg\">组内文档排名</button>\n");  
    fprintf(fp, "    </div>\n");  
    fprintf(fp, "  </nav>\n");  
    
    // Main content  
    fprintf(fp, "  <main class=\"bg-white shadow-md rounded-lg p-6\">\n");  
    
    // Generate content for each view  
    generate_latest_documents_html(fp, db, config);  
    generate_popular_documents_html(fp, db, config);  
    generate_user_rankings_html(fp, db, config);  
    generate_group_rankings_html(fp, db, config);  
    
    fprintf(fp, "  </main>\n");  
    
    // Footer  
    fprintf(fp, "  <footer class=\"mt-8 text-center text-gray-600 text-sm\">\n");  
    fprintf(fp, "    <p>© %d MinDoc文档统计排名生成器</p>\n", tm_info->tm_year + 1900);  
    fprintf(fp, "  </footer>\n");  
    
    fprintf(fp, "</div>\n");  
    fprintf(fp, "</body>\n");  
    fprintf(fp, "</html>\n");  
    
    fclose(fp);  
    sqlite3_close(db);  
    return true;  
}  

