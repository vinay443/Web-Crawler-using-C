#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <time.h>

// Define a structure for queue elements.
typedef struct URLQueueNode {
    char *url;
    int depth; // Depth of the URL
    struct URLQueueNode *next;
} URLQueueNode;

typedef struct
{
  char *string;
  size_t size;
} Response;

// Define a structure for a thread-safe queue.
typedef struct {
    URLQueueNode *head, *tail;
    pthread_mutex_t lock;
} URLQueue;

// Initialize a URL queue.
void initQueue(URLQueue *queue) {
    queue->head = queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
}

// Add a URL to the queue with depth information.
void enqueue(URLQueue *queue, const char *url, int depth) {
    URLQueueNode *newNode = malloc(sizeof(URLQueueNode));
    newNode->url = strdup(url);
    newNode->depth = depth;
    newNode->next = NULL;
    pthread_mutex_lock(&queue->lock);
    if (queue->tail) {
        queue->tail->next = newNode;
    } else {
        queue->head = newNode;
    }
    queue->tail = newNode;
    pthread_mutex_unlock(&queue->lock);
}

// Remove a URL from the queue.
char *dequeue(URLQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }
    URLQueueNode *temp = queue->head;
    char *url = temp->url;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    free(temp);
    pthread_mutex_unlock(&queue->lock);
    return url;
}

// Write callback function for libcurl to store fetched data
/*size_t write_callback(void *ptr, size_t size, size_t nmemb, char **data) {
    size_t realsize = size * nmemb;
    *data = realloc(*data, realsize + 1);
    if (*data == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 0;
    }
    memcpy(*data, ptr, realsize);
    (*data)[realsize] = '\0';
    return realsize;
}*/
size_t write_chunk(void *data, size_t size, size_t nmemb, void *userdata)
{
   size_t real_size = size * nmemb;


  Response *response = (Response *) userdata;

  char *ptr = realloc(response->string, response->size + real_size + 1);

  if (ptr == NULL)
  {
    // return 0;
    return 0;
  }

  response->string = ptr;

  memcpy(&(response->string[response->size]), data, real_size);

  response->size += real_size;

  response->string[response->size] = 0; // '\0';

  return real_size;
}

// Function to fetch HTML content from a URL.
/*char *fetch_html(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Failed to initialize libcurl\n");
        return NULL;
    }
    char *html_content = NULL;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_content);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: Failed to fetch URL: %s\n", url);
        curl_easy_cleanup(curl);
        return NULL;
    }
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        fprintf(stderr, "Error: HTTP request failed with response code %ld\n", response_code);
        curl_easy_cleanup(curl);
        return NULL;
    }
    curl_easy_cleanup(curl);
    printf("html from curl %s\n",html_content);
    return html_content;
}*/
char* extract_html(char *url)
{
    CURL *curl;
  CURLcode result;

  curl = curl_easy_init();
  if (curl == NULL)
  {
    fprintf(stderr, "HTTP request failed\n");
    curl_easy_cleanup(curl);
    return NULL;
  }

  Response response;
  response.string = malloc(1);
  response.size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &response);

  result = curl_easy_perform(curl);

   if (result != CURLE_OK)
  {
    fprintf(stderr, "Error: %s\n", curl_easy_strerror(result));
      curl_easy_cleanup(curl);
    return NULL;
  }

  //printf("%s\n", response.string);

  curl_easy_cleanup(curl);

  char* h = malloc(sizeof(char)*(response.size+1));
  strcpy(h,response.string);
  free(response.string);
  return h;

}

// Function to parse HTML content and extract links using strstr.
void parse_html(const char *html_content, URLQueue *queue) {
    const char *anchor_start = "<a href=\"";
    const char *anchor_end = "\">";
    const char *cursor = html_content;
    while ((cursor = strstr(cursor, anchor_start)) != NULL) {
        cursor += strlen(anchor_start);
        const char *href_end = strstr(cursor, anchor_end);
        if (href_end != NULL) {
            char *href = malloc(href_end - cursor + 1);
            if (href != NULL) {
                strncpy(href, cursor, href_end - cursor);
                href[href_end - cursor] = '\0';
                enqueue(queue, href, 0); // Add links with depth 0
                cursor = href_end;
            } else {
                fprintf(stderr, "Error: Memory allocation failed\n");
                break;
            }
        }
    }
}


// Function to log messages to a file.
void log_message(const char *message) {
    FILE *logfile = fopen("crawler.log", "a");
    if (logfile != NULL) {
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        fprintf(logfile, "[%s] %s\n", asctime(timeinfo), message);
        fclose(logfile);
    } else {
        fprintf(stderr, "Error: Failed to open log file\n");
    }
}

// Cleanup and program termination.
void cleanup(URLQueue *queue) {
    // Cleanup resources associated with the URL queue.
    URLQueueNode *current = queue->head;
    URLQueueNode *next;
    while (current != NULL) {
        next = current->next;
        free(current->url);
        free(current);
        current = next;
    }
    pthread_mutex_destroy(&queue->lock);
}

// Placeholder for the function to fetch and process a URL.
void *fetch_url(void *arg) {
    // Cast arg to your queue or custom data structure.
    URLQueue *queue = (URLQueue *)arg;
    char *url;
    while ((url = dequeue(queue)) != NULL) {
	// Log the URL
	log_message(url);
        // Fetch HTML content from the URL.
        char *html_content = extract_html(url);
        if (html_content != NULL) {
            // Parse HTML content to extract links.
            parse_html(html_content, queue);
            // Free allocated memory for HTML content.

        }
        free(html_content);
        // Free allocated memory for the URL.
        free(url);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Main function to drive the web crawler.
    if (argc < 2) {
        printf("Usage: %s <starting-url>\n", argv[0]);
        return 1;
    }
    URLQueue queue;
    initQueue(&queue);
    enqueue(&queue, argv[1], 0); // Starting URL with depth 0
    const int NUM_THREADS = 4; // Example thread count, adjust as needed.
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, fetch_url, (void *)&queue);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    cleanup(&queue);
    return 0;
}
