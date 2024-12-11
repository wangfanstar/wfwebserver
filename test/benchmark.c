#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <sys/time.h>  
#include <pthread.h>  
#include <time.h>  

#define BUFFER_SIZE 1024  
#define SERVER_PORT 8081  
#define SERVER_IP "127.0.0.1"  
#define NUM_THREADS 10  
#define REQUESTS_PER_THREAD 100  
#define MAX_RESPONSE_TIMES 1000  

struct ThreadStats {  
    double total_time;  
    int total_requests;  
    int successful_requests;  
    long total_bytes;  
    double response_times[REQUESTS_PER_THREAD];  
    int response_time_count;  
};  

double get_time_ms() {  
    struct timeval tv;  
    gettimeofday(&tv, NULL);  
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;  
}  

void* worker_thread(void* arg) {  
    struct ThreadStats* stats = (struct ThreadStats*)arg;  
    char buffer[BUFFER_SIZE];  
    const char *request = "GET /index.html HTTP/1.1\r\n"  
                         "Host: localhost\r\n"  
                         "Connection: close\r\n\r\n";  

    stats->response_time_count = 0;  

    for (int i = 0; i < REQUESTS_PER_THREAD; i++) {  
        int sock = socket(AF_INET, SOCK_STREAM, 0);  
        if (sock < 0) continue;  

        struct sockaddr_in server_addr;  
        memset(&server_addr, 0, sizeof(server_addr));  
        server_addr.sin_family = AF_INET;  
        server_addr.sin_port = htons(SERVER_PORT);  
        inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);  

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {  
            close(sock);  
            continue;  
        }  

        double start_time = get_time_ms();  
        
        send(sock, request, strlen(request), 0);  
        
        int total_bytes = 0;  
        int bytes_received;  
        while ((bytes_received = recv(sock, buffer, BUFFER_SIZE-1, 0)) > 0) {  
            total_bytes += bytes_received;  
        }  

        double elapsed = get_time_ms() - start_time;  
        
        stats->total_time += elapsed;  
        stats->total_requests++;  
        stats->successful_requests++;  
        stats->total_bytes += total_bytes;  
        
        if (stats->response_time_count < REQUESTS_PER_THREAD) {  
            stats->response_times[stats->response_time_count++] = elapsed;  
        }  

        close(sock);  
    }  
    return NULL;  
}  

void write_html_report(const char* filename, struct ThreadStats* thread_stats,   
                      int num_threads, double total_time) {  
    FILE* fp = fopen(filename, "w");  
    if (!fp) {  
        perror("Failed to open output file");  
        return;  
    }  

    // 汇总统计  
    struct ThreadStats total = {0};  
    for (int i = 0; i < num_threads; i++) {  
        total.total_time += thread_stats[i].total_time;  
        total.total_requests += thread_stats[i].total_requests;  
        total.successful_requests += thread_stats[i].successful_requests;  
        total.total_bytes += thread_stats[i].total_bytes;  
    }  

    // 收集所有响应时间  
    double all_response_times[NUM_THREADS * REQUESTS_PER_THREAD];  
    int total_responses = 0;  
    for (int i = 0; i < num_threads; i++) {  
        for (int j = 0; j < thread_stats[i].response_time_count; j++) {  
            all_response_times[total_responses++] = thread_stats[i].response_times[j];  
        }  
    }  

    // 获取当前时间  
    time_t now = time(NULL);  
    struct tm *t = localtime(&now);  
    char time_str[64];  
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);  

    fprintf(fp, "<!DOCTYPE html>\n"  
                "<html lang=\"en\">\n"  
                "<head>\n"  
                "<meta charset=\"UTF-8\">\n"  
                "<title>Server Performance Test Results</title>\n"  
                "<style>\n"  
                "    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f2f5; }\n"  
                "    .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n"  
                "    h1 { color: #1a365d; text-align: center; margin-bottom: 30px; }\n"  
                "    h2 { color: #2c5282; margin-top: 30px; }\n"  
                "    .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin: 20px 0; }\n"  
                "    .stat-box { background: #f8fafc; padding: 15px; border-radius: 6px; }\n"  
                "    .stat-label { font-weight: bold; color: #4a5568; }\n"  
                "    .stat-value { color: #2d3748; margin-top: 5px; }\n"  
                "    .chart { margin-top: 30px; height: 400px; }\n"  
                "    .timestamp { text-align: right; color: #718096; font-size: 0.9em; margin-top: 20px; }\n"  
                "</style>\n"  
                "</head>\n"  
                "<body>\n"  
                "<div class=\"container\">\n"  
                "    <h1>Server Performance Test Results</h1>\n"  
                "    <div class=\"timestamp\">Test Time: %s</div>\n"  
                "    <h2>Test Configuration</h2>\n"  
                "    <div class=\"grid\">\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Number of Threads</div>\n"  
                "            <div class=\"stat-value\">%d</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Requests per Thread</div>\n"  
                "            <div class=\"stat-value\">%d</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Server IP</div>\n"  
                "            <div class=\"stat-value\">%s</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Server Port</div>\n"  
                "            <div class=\"stat-value\">%d</div>\n"  
                "        </div>\n"  
                "    </div>\n"  
                "    <h2>Test Results</h2>\n"  
                "    <div class=\"grid\">\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Total Requests</div>\n"  
                "            <div class=\"stat-value\">%d</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Successful Requests</div>\n"  
                "            <div class=\"stat-value\">%d</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Total Time</div>\n"  
                "            <div class=\"stat-value\">%.2f seconds</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Average Response Time</div>\n"  
                "            <div class=\"stat-value\">%.2f ms</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Requests Per Second</div>\n"  
                "            <div class=\"stat-value\">%.2f</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Total Data Transferred</div>\n"  
                "            <div class=\"stat-value\">%.2f MB</div>\n"  
                "        </div>\n"  
                "        <div class=\"stat-box\">\n"  
                "            <div class=\"stat-label\">Transfer Rate</div>\n"  
                "            <div class=\"stat-value\">%.2f MB/s</div>\n"  
                "        </div>\n"  
                "    </div>\n"  
                "    <h2>Response Time Distribution</h2>\n"  
                "    <div class=\"chart\" id=\"chart\"></div>\n"  
                "</div>\n"  
                "<script>\n"  
                "function createHistogram(data, containerId) {\n"  
                "    const container = document.getElementById(containerId);\n"  
                "    const width = container.clientWidth;\n"  
                "    const height = container.clientHeight;\n"  
                "    const padding = 50;\n"  
                "    \n"  
                "    // Calculate bins\n"  
                "    const binCount = 30;\n"  
                "    const min = Math.min(...data);\n"  
                "    const max = Math.max(...data);\n"  
                "    const binWidth = (max - min) / binCount;\n"  
                "    const bins = Array(binCount).fill(0);\n"  
                "    \n"  
                "    data.forEach(value => {\n"  
                "        const binIndex = Math.min(Math.floor((value - min) / binWidth), binCount - 1);\n"  
                "        bins[binIndex]++;\n"  
                "    });\n"  
                "    \n"  
                "    const maxBinValue = Math.max(...bins);\n"  
                "    \n"  
                "    // Create SVG\n"  
                "    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');\n"  
                "    svg.setAttribute('width', width);\n"  
                "    svg.setAttribute('height', height);\n"  
                "    \n"  
                "    // Create bars\n"  
                "    const barWidth = (width - 2 * padding) / binCount;\n"  
                "    bins.forEach((count, i) => {\n"  
                "        const barHeight = count / maxBinValue * (height - 2 * padding);\n"  
                "        const x = padding + i * barWidth;\n"  
                "        const y = height - padding - barHeight;\n"  
                "        \n"  
                "        const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');\n"  
                "        rect.setAttribute('x', x);\n"  
                "        rect.setAttribute('y', y);\n"  
                "        rect.setAttribute('width', barWidth - 1);\n"  
                "        rect.setAttribute('height', barHeight);\n"  
                "        rect.setAttribute('fill', '#3b82f6');\n"  
                "        svg.appendChild(rect);\n"  
                "    });\n"  
                "    \n"  
                "    // Add axes\n"  
                "    const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');\n"  
                "    xAxis.setAttribute('x1', padding);\n"  
                "    xAxis.setAttribute('y1', height - padding);\n"  
                "    xAxis.setAttribute('x2', width - padding);\n"  
                "    xAxis.setAttribute('y2', height - padding);\n"  
                "    xAxis.setAttribute('stroke', 'black');\n"  
                "    svg.appendChild(xAxis);\n"  
                "    \n"  
                "    const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');\n"  
                "    yAxis.setAttribute('x1', padding);\n"  
                "    yAxis.setAttribute('y1', padding);\n"  
                "    yAxis.setAttribute('x2', padding);\n"  
                "    yAxis.setAttribute('y2', height - padding);\n"  
                "    yAxis.setAttribute('stroke', 'black');\n"  
                "    svg.appendChild(yAxis);\n"  
                "    \n"  
                "    // Add labels\n"  
                "    const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"  
                "    xLabel.textContent = 'Response Time (ms)';\n"  
                "    xLabel.setAttribute('x', width / 2);\n"  
                "    xLabel.setAttribute('y', height - 10);\n"  
                "    xLabel.setAttribute('text-anchor', 'middle');\n"  
                "    svg.appendChild(xLabel);\n"  
                "    \n"  
                "    const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"  
                "    yLabel.textContent = 'Frequency';\n"  
                "    yLabel.setAttribute('x', -height/2);\n"  
                "    yLabel.setAttribute('y', 20);\n"  
                "    yLabel.setAttribute('transform', 'rotate(-90)');\n"  
                "    yLabel.setAttribute('text-anchor', 'middle');\n"  
                "    svg.appendChild(yLabel);\n"  
                "    \n"  
                "    container.appendChild(svg);\n"  
                "}\n"  
                "\n"  
                "const responseTimes = [",  
            time_str,  
            NUM_THREADS, REQUESTS_PER_THREAD, SERVER_IP, SERVER_PORT,  
            total.total_requests, total.successful_requests,  
            total_time / 1000.0,  
            total.total_time / total.successful_requests,  
            (total.successful_requests / (total_time / 1000.0)),  
            total.total_bytes / (1024.0 * 1024.0),  
            (total.total_bytes / (1024.0 * 1024.0)) / (total_time / 1000.0));  

    // 写入响应时间数据  
    for (int i = 0; i < total_responses; i++) {  
        fprintf(fp, "%.2f%s", all_response_times[i], i < total_responses - 1 ? "," : "");  
    }  

    fprintf(fp, "];\n"  
                "createHistogram(responseTimes, 'chart');\n"  
                "</script>\n"  
                "</body>\n"  
                "</html>\n");
	fclose(fp);  
}  

int main() {  
    pthread_t threads[NUM_THREADS];  
    struct ThreadStats thread_stats[NUM_THREADS] = {0};  
    
    // 打印开始测试信息  
    printf("Starting performance test...\n");  
    printf("Configuration:\n");  
    printf("- Threads: %d\n", NUM_THREADS);  
    printf("- Requests per thread: %d\n", REQUESTS_PER_THREAD);  
    printf("- Target server: %s:%d\n", SERVER_IP, SERVER_PORT);  
    
    double start_time = get_time_ms();  

    // 创建线程  
    for (int i = 0; i < NUM_THREADS; i++) {  
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_stats[i]) != 0) {  
            perror("Failed to create thread");  
            exit(1);  
        }  
        printf("Thread %d started\n", i + 1);  
    }  

    // 等待所有线程完成  
    for (int i = 0; i < NUM_THREADS; i++) {  
        pthread_join(threads[i], NULL);  
        printf("Thread %d completed\n", i + 1);  
    }  

    double total_time = get_time_ms() - start_time;  

    // 生成带时间戳的文件名  
    time_t now = time(NULL);  
    struct tm *t = localtime(&now);  
    char filename[100];  
    strftime(filename, sizeof(filename), "result-%Y%m%d-%H%M%S.html", t);  

    // 生成HTML报告  
    write_html_report(filename, thread_stats, NUM_THREADS, total_time);  
    
    printf("\nTest completed successfully!\n");  
    printf("Results have been written to: %s\n", filename);  
    printf("You can open this file directly in any web browser to view the results.\n");  

    return 0;  
}