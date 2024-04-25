/*
--------------------------------
|          Web Crawler         |
|       Operating Systems      |
|                              |
|@Author: Marlon Dominguez     |
|@Author:                      |
|@Author:                      |
--------------------------------

@Functionality:

    • Multithreading: Use multiple threads to fetch web pages concurrently. 

    • URL Queue: Implement a thread-safe queue to manage URLs that are pending to be 
                 fetched. 

    • HTML Parsing: Extract links from the fetched web pages to find new URLs to crawl. 

    • Depth Control: Allow the crawler to limit the depth of the crawl to prevent infinite 
                     recursion. 
    
    • Synchronization: Implement synchronization mechanisms to manage access to 
                       shared resources among threads. 
    
    • Error Handling: Handle possible errors gracefully, including network errors, 
                      parsing errors, and dead links. 

    • Logging: Log the crawler’s activity, including fetched URLs and encountered errors.


@Requirements:

    • Makefile

    • GCC Compatible

    • README.txt


@Approach:
    1) URL & Parsing Data 
       ------------------
        C does not provide a native way to open websites or invoke HTTP requests.
        Why do we need HTTP requests? It is because we must aquire the HTML data from the website. 
        Therefore, we will be implementing the libcurl library (DOES NOT PROVIDE LOCKING).
            - On linux systems use [sudo apt install curl] to install libcurl. 
            - In the program, include the following header -> #include <curl/curl.h> 
        
        If still confused, please glance over the documentation -> https://everything.curl.dev/examples/get


    2) 

*/

#include <curl/curl.h> 
#include <cjson/cJSON.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>
#include <pthread.h>

typedef struct mem{
    char *memory; //String 
    size_t size;
} mem;





typedef struct URL{
    char *url;
    struct URL *next_URL;
} URL;



//Define a structure for a thread-safe queue to be written to.
typedef struct data_list{
    URL *head, *tail;
    pthread_mutex_t lock;
} data_list;



//Define a structure for queue elements.
typedef struct URLQueueNode{
    char *html_url;
    struct URLQueueNode *next_URL;
} URLQueueNode;



// Define a structure for a thread-safe queue.
typedef struct URLQueue{
    URLQueueNode *head, *tail;
    pthread_mutex_t lock;
} URLQueue;




void append_to_log_file(const char *message){
    // Open the log file in append mode
    FILE *log_file = fopen("crawler_log.txt", "a");

    // Check if the file opened successfully
    if (log_file == NULL) {
        fprintf(stderr, "Error opening log file.\n");
        return;
    }

    // Append the message to the log file
    fprintf(log_file, "%s\n", message);

    // Close the log file
    fclose(log_file);
}


//URLQUEUE struct 
struct URL * create_URL(char *url){
  
    struct URL *newNode = (struct URL *) malloc(sizeof(struct URL));

    if (newNode == NULL) {

        printf("Memory allocation failed.\n");
        return NULL;
    }
    
    newNode -> url = (char *) malloc(strlen(url) + 1); 

    if(newNode->url == NULL) {

        printf("Memory allocation failed.\n");
        free(newNode); // Free previously allocated memory
        return NULL;
    }

 
    strcpy(newNode -> url, url);

    newNode -> next_URL = NULL;

    return newNode;
}

// Add a URL to the queue.
void enqueue_URL(URLQueue **url_q, const char *url){

    struct URLQueueNode *newURL = (struct URLQueueNode *) malloc( sizeof(URLQueueNode) );
    
    newURL -> html_url = strdup(url);
    newURL -> next_URL = NULL;

    pthread_mutex_lock(&((*url_q)->lock));
    
    if((*url_q) -> tail) {

        (*url_q)->tail->next_URL = newURL;
    } 
    
    else {
        (*url_q) -> head = newURL;
    }

    (*url_q) -> tail = newURL;
    pthread_mutex_unlock(&((*url_q)->lock));
}


bool url_exists(struct URL *URLS, const char *url){

    struct URL *ptr = URLS;

    while(ptr != NULL){
        if(strcmp(ptr->url, url) == 0){
            //printf("Duplicate avoided.");
            return true;
        }

        ptr = ptr -> next_URL;

    }

    return false;
}

// Add a URL to the output queue.
void append_data(struct data_list **output_q, const char *url){

    
    //check if URL already exists
    if(url_exists((*output_q) -> head, url)){
        return;
    }

    struct URL *newURL = (struct URL *) malloc( sizeof(URL) );
    
    newURL -> url = strdup(url);
    newURL -> next_URL = NULL;

    pthread_mutex_lock(&((*output_q)->lock));

    if((*output_q) -> tail) {

        (*output_q)->tail->next_URL = newURL;
    } 
    
    else {

        (*output_q) -> head = newURL;
    }

    (*output_q) -> tail = newURL;

    pthread_mutex_unlock(&((*output_q)->lock));

    return;
}







//Remove a URL from the URLQueue.
char* dequeue_URL(URLQueue *URLS) {

    //pthread_mutex_lock(&queue->lock);

    if(URLS -> head == NULL) {
        pthread_mutex_unlock(&URLS -> lock);
        return NULL;
    }


    //URLQueue is not empty.
    URLQueueNode *temp = URLS -> head;

    char *url = temp -> html_url;

    URLS -> head = URLS -> head -> next_URL;

    if (URLS ->head == NULL) {

        URLS -> tail = NULL;
    }

    free(temp);

    pthread_mutex_unlock(&URLS->lock);

    return url;
}



/*
// Placeholder for the function to fetch and process a URL.



*/





/*
Please read https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html#:~:text=This%20callback%20function%20gets%20called,nmemb%3B%20size%20is%20always%201.
for a better understanding of the write_callback function and it's arguments. 

In a brief description, the write_callback() function is responsible for the transfer of data. In this case,
between the webpage and the caller who is curl. The data being handled in this case is HTML, which comes 
in the form of strings. We have to implement this method ourselves. 

@param *ptr: ptr to curl handler. 
@param size_t size: size of data object being transferred. (e.g int = 4 bytes, char = 1 byte)
@param size_t nmemb: data object total count. In this case, think of chars or strings. 
@param void *userdata: Data container to be referenced. 
*/

/*
Checks if url is a sitemap xml. 
*/
bool check_ifXML(char *url){

    char *needle = ".xml";

    if(strstr(url, needle) != NULL){
        return true;
    }

    //Not a sitemap url, must be a regular url. 
    return false;
}


int write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {

    size_t real_size = size * nmemb;
    struct mem *memory_ = (struct mem *)userdata;

    memory_->memory = realloc(memory_->memory, memory_->size + real_size + 1);
    if(memory_->memory == NULL){
        append_to_log_file("Failed to allocate memory.");
        return 0;
    }

    memcpy(&(memory_->memory[memory_->size]), ptr, real_size);
    memory_->size += real_size;
    //memory_->memory[memory_->size] = 0;

    memory_->memory[memory_->size] = '\0';

    return real_size;
}



void Parse_JSON(const char *json_data){

    //Create CSJON obj
    //cJSON *json_parsed = cJSON_Parse(json_data);

    cJSON *json_parsed = cJSON_Parse(json_data);

    char *parsed_string = cJSON_Print(json_parsed);
    
    //printf("%s", parsed_string);

    
    return;
}



/*
The open_url() function takes a string url as input and creates an http
request to the respective page using libcurl. 

@param char *url: pointer to string, the unique resource identifier of the target page. 
*/
char* open_url(char *url){
    
    /*
    Create libcurl object, or handler, necessary for http interaction. 
    The call to curl_easy_init() initializes the handler. 
    IMPORTANT: Make sure to close the handler when finished. 
    */
    CURL *curl_handler = curl_easy_init();

    
    //We have to dynamically allocate the userdata struct aswell, as we are passing it to CJSON.
    struct mem *userdata = malloc(sizeof(struct mem));


    //Check if the handler was initialized correctly, dereferencing the handler pointer gives us the error status. 
    if(curl_handler){

        /*
        In libcurl a variety of options exist to modify the behaviour of the curl handler. 
        These options are initialized using curl_set_opt(curl handler, option, option_parameter).
        
        What do we want the curl handler to do?
            -> We want it to establish connection with a web page 
            
            -> Retrieve HTML data 

        We must open the url before transferring the data. 
        */

        /*Since we are going to be reallocating memory in the 
          writeback function we can allocate a single byte to start with.
        */
        userdata->memory = malloc(1); 
        userdata->size = 0;

        struct curl_slist *headers = NULL;
        //headers = curl_slist_append(headers, "Accept: application/json");
        //headers = curl_slist_append(headers, "Content-Type: application/json");
        //headers = curl_slist_append(headers, "charset: utf-8");

        curl_easy_setopt(curl_handler, CURLOPT_URL, url);
        curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handler, CURLOPT_USERAGENT, "libcurl-agent/1.0"); //Additional information for server requests
        curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, userdata); // Pass userdata to write callback
        curl_easy_setopt(curl_handler, CURLOPT_HEADER, 0);



        //Execute the behaviour (data transfer) attributed to curl_handler.
        CURLcode flag = curl_easy_perform(curl_handler);

        if (flag != CURLE_OK) {
            //fprintf(stderr, "Retrieval of : %s\n", curl_easy_strerror(flag));
            return NULL;
        }
        

        //Close the handler. The handler is what performed the data transfer for us. 
        curl_easy_cleanup(curl_handler);

        //Data is now preserved in data struct
        //printf("%s", userdata->memory); 

        return userdata->memory; 
    }

    return NULL;
    
}


 
struct URLQueueNode* create_URLQueueNode(char *url){
  
    struct URLQueueNode *newNode = (struct URLQueueNode *) malloc(sizeof(struct URLQueueNode));

    if (newNode == NULL) {

        printf("Memory allocation failed.\n");
        return NULL;
    }
    
    newNode -> html_url = (char *) malloc(strlen(url) + 1); 

    if(newNode->html_url == NULL) {

        printf("Memory allocation failed.\n");
        free(newNode); // Free previously allocated memory
        return NULL;
    }

 
    strcpy(newNode -> html_url, url);

    newNode -> next_URL = NULL;

    return newNode;
}



// Initialize a URL queue.
void initQueue(URLQueue *URLS){

    URLS -> head = NULL;
    URLS -> tail = NULL;

    pthread_mutex_init(&URLS->lock, NULL);
}

void initData(struct data_list *output_q){
    output_q -> head = NULL;
    output_q -> tail = NULL;

    pthread_mutex_init(&output_q->lock, NULL);
}


/*
Some websites do not support www.website.com/sitemap.xml. 
Therefore, our other attempt to retrieve the sitemap will consist
of analyzing and parsing the robot.txt file.
The robots.txt file should give us the sitemaps, if any. 
We are just searching for the sitemap url(s), it is possible for 
there to be more than one sitemap url. That is the difference between 
this method and getSiteMaps_XML. The idea is to send the sitemap urls 
found here to getSiteMaps_XML so that it can search for more sitemap urls. 
If robot.txt is not available and the sitemap cannot be retrieved, let the
user know. OPTIONAL: Implement logic to allow user to enter sitemap url manually. 


void get_RobotSiteMaps(char *url, struct sitemap **sitemaps){ //Accept pointer to pointer since we are modifying the url 

    char append[12] = "/robots.txt";    //Text to append to URL.
    char modified_url[100];

    strcpy(modified_url, url);
    strcat(modified_url, append);

    struct mem *memory = open_url(modified_url);

    char *line = strtok((char *) memory->memory, "\n");
    
    while (line != NULL) {

        if (strstr(line, "Sitemap:") == line) {

            char *sitemap_url = line + strlen("Sitemap:");
            
            while(*sitemap_url == ' ' || *sitemap_url == '\t'){

                sitemap_url++;
            }
            
            //printf("Hello!");
            //insertSitemap(&sitemaps, sitemap_url);

            printf("URL: %s\n", sitemap_url);
        }

        line = strtok(NULL, "\n");
    }


    //printf("%s", memory->memory);

    /*
    


    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[1000];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "Sitemap:") == line) {
            char *sitemap_url = line + strlen("Sitemap:");
            // Trim leading and trailing whitespace
            while (*sitemap_url == ' ' || *sitemap_url == '\t' || *sitemap_url == '\n') {
                sitemap_url++;
            }
            // Print or process the sitemap URL
            printf("Sitemap URL: %s", sitemap_url);
        }
    }

    fclose(file);
  
}

*/


void getTextInsideLoc(xmlNode *node, struct URLQueue *url_q, char *url){

    
    for (xmlNode *cur = node; cur; cur = cur->next){

        if(cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, (const xmlChar *) "loc") == 0){

            xmlNode *child = cur->children;
            if(child && child->type == XML_TEXT_NODE){
                
                enqueue_URL(&url_q, child->content);
                
                //printf("Text inside <loc>: %s\n", child->content);
            }
        }

        getTextInsideLoc(cur->children, url_q, url);
    }
}



char* getTextInsideElements(TidyDoc doc, TidyNode node, char *elementName) {

    TidyNode child;

    char *target = "window.__PRELOADED_STATE__ =";

    for (child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        
        ctmbstr name = tidyNodeGetName(child);
        
        if(name){
            // Check if the node matches the specified element name
            if (strcmp(name, elementName) == 0) {
                TidyBuffer buf;
                tidyBufInit(&buf);
                tidyNodeGetText(doc, child, &buf);
                //printf("Text inside <%s>: %s\n", elementName, buf.bp);

                char* foundit = strstr(buf.bp, target);

                if(foundit != NULL){
                    //printf("Found it! \n%s", foundit); 
                    
                    char *json = strstr(foundit, "{");
                    //printf("%s", json);
                    //Call parseJSON
                    //Parse_JSON(json);

                    tidyBufFree(&buf);
                    return json;
                }

                ///printf("Didnt find it. ");

                tidyBufFree(&buf);
            }
        }

        //Recursively search for the specified elements in child nodes
        getTextInsideElements(doc, child, elementName);
        
    }
}


void printAnchorHrefs(TidyDoc tdoc) {
    TidyNode anchor = tidyGetBody(tdoc);  // Start with the body tag
    anchor = tidyGetChild(anchor);        // Get the first child of the body

    while (anchor) {
        TidyAttr attr;
        if (tidyNodeGetId(anchor) == TidyTag_A) {  // Check if the node is an <a> tag
            attr = tidyAttrFirst(anchor);
            while (attr) {
                if (tidyAttrGetId(attr) == TidyAttr_HREF) {  // Check if the attribute is 'href'
                    //printf("Found href: %s\n", tidyAttrValue(attr));
                    break;
                }
                attr = tidyAttrNext(attr);
            }
        }
        anchor = tidyGetNext(anchor);  // Get next sibling node
    }
}



void crawlElements(xmlNode *node, char *target, struct data_list *output, char *url, int *depth_count){


    xmlNode *cur = NULL;
    for (cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            //printf("Element: <%s>\n", cur->name);

            // If the element has children, recursively traverse them
            if (cur->children) {
                crawlElements(cur->children, target, output, url, depth_count);
            }

            //printf("End Element: <%s>\n", cur->name); // Print end tag
        } 
        
        else if (cur->type == XML_TEXT_NODE) {
            
            //printf("Loc has <%s>: %s\n", node->parent->name, cur->content);
            if(strstr(cur->content, target) != NULL){
                printf("found it! At ");
                printf("URL: %s\n", url);
                (*depth_count) += 1;
                append_data(&output, url);

                return;
            }
        }
    }

    
}



void parseHTMLElements(struct URLQueue *url_q, struct data_list *output, const char *html, char *target, char *url, int *depth_count){

     
    htmlDocPtr doc = NULL;
    xmlNode *root = NULL;

    // Parse the HTML content from the string
    doc = htmlReadMemory(html, strlen(html), NULL, NULL, HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);

    if (doc == NULL) {
        append_to_log_file("Failed to parse document");
        return;
    }

    // Get the root element of the HTML document
    root = xmlDocGetRootElement((xmlDoc *)doc);
    if (root == NULL) {
        append_to_log_file("Empty document");
        xmlFreeDoc((xmlDoc *)doc);
        return;
    }

    //Start traversing the HTML tree
    crawlElements(root, target, output, url, depth_count);

    // Cleanup
    xmlFreeDoc((xmlDoc *)doc);
    xmlCleanupParser();

    return;
}




//printing the href attributes 
bool parseHTML(struct URLQueue *url_q, const char *html_content){

    //Reads the HTML content, htmlReadDoc - converts the HTML string to xmlDoc pointer 
    htmlDocPtr doc = htmlReadDoc((xmlChar*)html_content, NULL, NULL, HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR); //giving warning and error reports 
    if (!doc){
        append_to_log_file("Failed to parse the document");
    }

    //for the parsed XML document 
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    if(!context){
        append_to_log_file("Failed to create XPath context");
        xmlFreeDoc(doc);
    }

    //Evaluate XPath expression to find all <a> elements with an href attribute
    xmlXPathObjectPtr result = xmlXPathEvalExpression((xmlChar*)"//a[@href]", context);
    if (!result){
        append_to_log_file("Failed to evaluate XPath expression");
        xmlXPathFreeContext(context);
        xmlFreeDoc(doc);
    }
    //checking if the node set is empty 
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        printf("");
        return false;
    }


    else{

        //representing a set of nodes in a document 
        xmlNodeSetPtr nodes = result->nodesetval; //referencing to access the node set from the result of the XPath evaluation.
        for(int i = 0; i < nodes -> nodeNr; i++){ //accessing each node

            xmlNodePtr node = nodes->nodeTab[i];
            xmlChar *href = xmlGetProp(node, (xmlChar *)"href"); //Retreving the href attribute 

            if(href){

                //printf("herf: %s\n", href);
                //printf("Enqueing URL: %s", href);
                enqueue_URL(&url_q, href);
                xmlFree(href);
            }

        }
    }
       
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);

    return true; 
}



bool parseXML(char *url, char *XML, struct URLQueue *url_q){

    xmlDoc *doc = xmlReadMemory(XML, strlen(XML), NULL, NULL, 0);
    if (!doc){
        append_to_log_file("Failed to parse XML content");
        return false;
    }

    xmlNode *root = xmlDocGetRootElement(doc);

    if (!root){
        append_to_log_file("Empty XML document");
        xmlFreeDoc(doc);
        return false;
    }

    //Get url from <loc> elements
    getTextInsideLoc(root, url_q, url);


    xmlFreeDoc(doc);
    xmlCleanupParser();

    return true;
}




/*
The execute_page() function is the function we should call to process a web page, or url, within our crawler. 
Since C does not provide native support for retrieving web pages the process consists of multiple
steps. This function will begin the execution of those steps. 

@param char *url 
@return bool: returns false if error, true otherwise. 
*/
void * execute_crawl(void *arg){

    struct {
        struct URLQueue *url_q;
        struct data_list *output;
        char *url;
        char *target;
        int  depth_limit; 
        int depth_count;
    } *args = arg;
    
    if (args->output == NULL || args->url_q == NULL) {
        append_to_log_file("Invalid arguments");
        return NULL;
    }
    
    
    // Initialize local variables
    char *target = args->target;

    //printf("Target: %s", target);

    while(1){

        if(args->depth_count >= args->depth_limit){

            printf("Depth limit %ls reached!\n\n", &args->depth_count);
            break; 
        }

        // Dequeue URL from the queue
        char *url = dequeue_URL(args->url_q);
        //printf("%s", url);
        if (url == NULL) {
            // No more URLs in the queue
            break;
        }

        // Process the URL
        char *data = open_url(url); 

        if(data == NULL){
            printf("HTTP Request failed.");
            return 1;
        }

        if (data != NULL){

            if (check_ifXML(url)) {
                // Parse XML content
                if (parseXML(url, data, args->url_q)) {
                    //printf("XML Parsed.\n");
                }
            } else {
                // Parse HTML content
                if (parseHTML(args->url_q, data)) {
                    //printf("HTML URL's Parsed.\n");
                }

                // Parse specific elements in the HTML
                parseHTMLElements(args->url_q, args->output, data, target, url, &(args->depth_count));
            }
            // Free memory allocated for data
            free(data);
        } else {
            // Handle error fetching URL
            append_to_log_file("Failed to fetch URL");
        }
        
        // Free memory allocated for the URL
        free(url);
    }

    return NULL;
}
    



void print_queue(struct URLQueue *url_q){

    struct URLQueueNode *ptr = url_q -> head;

    while(ptr != NULL){
        printf("URL: %s\n", ptr->html_url);
        ptr = ptr -> next_URL;
    }

    return;
}


void printOutput(struct data_list *output){

    struct URL *ptr = output -> head;
    printf("Printing output...\n");

    while(ptr != NULL){
        printf("URL: %s\n", ptr->url);
        ptr = ptr -> next_URL;
    }

    return;
}


int main(int argc, char *argv[]){

    if(argc < 2) {
        printf("Usage: %s <starting-url>\n", argv[0]);
        return 1;
    }

    
    //"https://www.ubisoft.com/en-ca/game/assassins-creed/mirage/photomode"

    char *first_url = argv[2];

    struct URLQueue *url_q = (struct URLQueue *) malloc(sizeof(struct URLQueue));
    if (url_q == NULL) {
        append_to_log_file("Memory allocation failed\n");
        return 1;
    }
    initQueue(url_q);
    enqueue_URL(&url_q, first_url);

    struct data_list *output = (struct data_list *)malloc(sizeof(struct data_list)); // Initialize output structure
    if (output == NULL) {
        append_to_log_file("Memory allocation failed");
        free(url_q);
        return 1;
    }
    output->head = output->tail = NULL; // Set head and tail to NULL initially

    char *target = "About";

    int depth_limit = atoi(argv[1]);

    //printf("%ls %s", &depth_limit, first_url);

    printf("We will scrape URL's that contain the following target.\n");
    printf("Target: %s\n", target);

    //execute_crawl(url_q, output, target);

    
    // Placeholder for creating and joining threads.
    // You will need to create multiple threads and distribute the work of URL fetching among them.
    const int NUM_THREADS = 10; // Example thread count, adjust as needed.

    pthread_t threads[NUM_THREADS]; 


    struct {
        struct URLQueue *url_q;
        struct data_list *output;
        char *url;
        char *target;
        int  depth_limit; 
        int depth_count;
    } args = {url_q, output, first_url, target, 4, 0};


    // Create the thread and pass the arguments
    
    //Create worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, execute_crawl, &args) != 0){
            //fprintf(stderr, "Failed to create thread\n");
            free(url_q);
            free(output);
        }
    }
    

    //Join threads after completion.
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    if (output && output->head) { //Check if output and output->head are not NULL
        printOutput(output);

    } else {

        printf("Output is empty\n");
    }
    
    // Cleanup and program termination.
    // You may need to add additional cleanup logic here.
    



    return 0; 
} 