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

// 获取当前时间（毫秒）  
double get_time_ms() {  
    struct timeval tv;  
    gettimeofday(&tv, NULL);  
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;  
}  

// 工作线程函数  
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

// HTML报告生成函数  
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

    // 计算统计数据  
    double min_response_time = all_response_times[0];  
    double max_response_time = all_response_times[0];  
    double total_response_time = 0;  

    for (int i = 0; i < total_responses; i++) {  
        if (all_response_times[i] < min_response_time) min_response_time = all_response_times[i];  
        if (all_response_times[i] > max_response_time) max_response_time = all_response_times[i];  
        total_response_time += all_response_times[i];  
    }  

    double avg_response_time = total_response_time / total_responses;  

    // 获取当前时间  
    time_t now = time(NULL);  
    struct tm *t = localtime(&now);  
    char time_str[64];  
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);  

    // 开始写入HTML文件  
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
                "    .data-table { width: 100%; border-collapse: collapse; margin: 20px 0; }\n"  
                "    .data-table th, .data-table td { padding: 12px; text-align: left; border: 1px solid #e5e7eb; }\n"  
                "    .data-table th { background-color: #f8fafc; font-weight: bold; }\n"  
                "    .data-table tr:nth-child(even) { background-color: #f8fafc; }\n"  
                "    .data-table tr:hover { background-color: #f1f5f9; }\n"  
                "</style>\n"  
                "</head>\n"  
                "<body>\n"  
                "<div class=\"container\">\n");  

    // 写入标题和时间戳  
    fprintf(fp, "    <h1>Server Performance Test Results</h1>\n"  
                "    <div class=\"timestamp\">Test Time: %s</div>\n", time_str);  

    // 写入测试配置  
    fprintf(fp, "    <h2>Test Configuration</h2>\n"  
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
                "    </div>\n",  
            NUM_THREADS, REQUESTS_PER_THREAD, SERVER_IP, SERVER_PORT);
			    // 写入测试结果  
    fprintf(fp, "    <h2>Test Results</h2>\n"  
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
                "    </div>\n",  
            total.total_requests,  
            total.successful_requests,  
            total_time / 1000.0,  
            avg_response_time,  
            (total.successful_requests / (total_time / 1000.0)),  
            total.total_bytes / (1024.0 * 1024.0),  
            (total.total_bytes / (1024.0 * 1024.0)) / (total_time / 1000.0));  

    // 写入详细统计表格  
    fprintf(fp, "    <div class=\"stats-table\">\n"  
                "        <h2>Detailed Statistics</h2>\n"  
                "        <table class=\"data-table\">\n"  
                "            <tr>\n"  
                "                <th>Metric</th>\n"  
                "                <th>Value</th>\n"  
                "            </tr>\n"  
                "            <tr>\n"  
                "                <td>Minimum Response Time</td>\n"  
                "                <td>%.2f ms</td>\n"  
                "            </tr>\n"  
                "            <tr>\n"  
                "                <td>Maximum Response Time</td>\n"  
                "                <td>%.2f ms</td>\n"  
                "            </tr>\n"  
                "            <tr>\n"  
                "                <td>Average Response Time</td>\n"  
                "                <td>%.2f ms</td>\n"  
                "            </tr>\n"  
                "        </table>\n"  
                "    </div>\n",  
            min_response_time,  
            max_response_time,  
            avg_response_time);  

    // 写入响应时间分布图  
    fprintf(fp, "    <h2>Response Time Distribution</h2>\n"  
                "    <div class=\"chart\" id=\"chart\"></div>\n"  
                "</div>\n"  
                "<script>\n"  
                "function createHistogram(data, containerId) {\n"  
                "    const container = document.getElementById(containerId);\n"  
                "    const width = container.clientWidth;\n"  
                "    const height = container.clientHeight;\n"  
                "    const padding = 60;\n"  
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
                "        \n"  
                "        rect.setAttribute('title', `Range: ${(min + i * binWidth).toFixed(2)} - ${(min + (i + 1) * binWidth).toFixed(2)} ms\\nCount: ${count}`);\n"  
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
                "    // Add X-axis labels\n"  
                "    const xLabels = 5;\n"  
                "    for (let i = 0; i <= xLabels; i++) {\n"  
                "        const x = padding + (width - 2 * padding) * (i / xLabels);\n"  
                "        const value = min + (max - min) * (i / xLabels);\n"  
                "        \n"  
                "        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"  
                "        label.textContent = value.toFixed(1);\n"  
                "        label.setAttribute('x', x);\n"  
                "        label.setAttribute('y', height - padding + 20);\n"  
                "        label.setAttribute('text-anchor', 'middle');\n"  
                "        label.setAttribute('font-size', '12px');\n"  
                "        svg.appendChild(label);\n"  
                "        \n"  
                "        const gridLine = document.createElementNS('http://www.w3.org/2000/svg', 'line');\n"  
                "        gridLine.setAttribute('x1', x);\n"  
                "        gridLine.setAttribute('y1', padding);\n"  
                "        gridLine.setAttribute('x2', x);\n"  
                "        gridLine.setAttribute('y2', height - padding);\n"  
                "        gridLine.setAttribute('stroke', '#e5e7eb');\n"  
                "        gridLine.setAttribute('stroke-dasharray', '4');\n"  
                "        svg.appendChild(gridLine);\n"  
                "    }\n"  
                "    \n"  
                "    // Add Y-axis labels\n"  
                "    const yLabels = 5;\n"  
                "    for (let i = 0; i <= yLabels; i++) {\n"  
                "        const y = height - padding - (height - 2 * padding) * (i / yLabels);\n"  
                "        const value = maxBinValue * (i / yLabels);\n"  
                "        \n"  
                "        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"  
                "        label.textContent = Math.round(value);\n"  
                "        label.setAttribute('x', padding - 10);\n"  
                "        label.setAttribute('y', y + 5);\n"  
                "        label.setAttribute('text-anchor', 'end');\n"  
                "        label.setAttribute('font-size', '12px');\n"  
                "        svg.appendChild(label);\n"  
                "        \n"  
                "        const gridLine = document.createElementNS('http://www.w3.org/2000/svg', 'line');\n"  
                "        gridLine.setAttribute('x1', padding);\n"  
                "        gridLine.setAttribute('y1', y);\n"  
                "        gridLine.setAttribute('x2', width - padding);\n"  
                "        gridLine.setAttribute('y2', y);\n"  
                "        gridLine.setAttribute('stroke', '#e5e7eb');\n"  
                "        gridLine.setAttribute('stroke-dasharray', '4');\n"  
                "        svg.appendChild(gridLine);\n"  
                "    }\n"  
                "    \n"  
                "    // Add labels\n"  
                "    const xLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"  
                "    xLabel.textContent = 'Response Time (ms)';\n"  
                "    xLabel.setAttribute('x', width / 2);\n"  
                "    xLabel.setAttribute('y', height - 10);\n"  
                "    xLabel.setAttribute('text-anchor', 'middle');\n"  
                "    xLabel.setAttribute('font-size', '14px');\n"  
                "    svg.appendChild(xLabel);\n"  
                "    \n"  
                "    const yLabel = document.createElementNS('http://www.w3.org/2000/svg', 'text');\n"  
                "    yLabel.textContent = 'Frequency';\n"  
                "    yLabel.setAttribute('x', -height/2);\n"  
                "    yLabel.setAttribute('y', 20);\n"  
                "    yLabel.setAttribute('transform', 'rotate(-90)');\n"  
                "    yLabel.setAttribute('text-anchor', 'middle');\n"  
                "    yLabel.setAttribute('font-size', '14px');\n"  
                "    svg.appendChild(yLabel);\n"  
                "    \n"  
                "    const style = document.createElementNS('http://www.w3.org/2000/svg', 'style');\n"  
                "    style.textContent = 'rect:hover { fill: #2563eb; }';\n"  
                "    svg.appendChild(style);\n"  
                "    \n"  
                "    container.appendChild(svg);\n"  
                "}\n"  
                "\n"  
                "const responseTimes = [");  

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
    
    printf("Starting performance test...\n");  
    printf("Configuration:\n");  
    printf("- Threads: %d\n", NUM_THREADS);  
    printf("- Requests per thread: %d\n", REQUESTS_PER_THREAD);  
    printf("- Target server: %s:%d\n", SERVER_IP, SERVER_PORT);  
    
    double start_time = get_time_ms();  

    for (int i = 0; i < NUM_THREADS; i++) {  
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_stats[i]) != 0) {  
            perror("Failed to create thread");  
            exit(1);  
        }  
        printf("Thread %d started\n", i + 1);  
    }  

    for (int i = 0; i < NUM_THREADS; i++) {  
        pthread_join(threads[i], NULL);  
        printf("Thread %d completed\n", i + 1);  
    }  

    double total_time = get_time_ms() - start_time;  

    time_t now = time(NULL);  
    struct tm *t = localtime(&now);  
    char filename[100];  
    strftime(filename, sizeof(filename), "result-%Y%m%d-%H%M%S.html", t);  

    write_html_report(filename, thread_stats, NUM_THREADS, total_time);  
    
    printf("\nTest completed successfully!\n");  
    printf("Results have been written to: %s\n", filename);  
    printf("You can open this file directly in any web browser to view the results.\n");  

    return 0;  
}
			