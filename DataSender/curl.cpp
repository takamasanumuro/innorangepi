#ifdef __aarch64__
#include </usr/include/aarch64-linux-gnu/curl/curl.h>
#elif __x86_64__
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MemoryStruct {
  char *memory;
  size_t size;
};

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    printf("error: not enough memory\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

//Function to curl the URL
void curlURL(char *url)
{
	CURL *curl_handle;
	CURLcode result;

	struct MemoryStruct chunk;
	chunk.memory = (char*)malloc(1);
	chunk.size = 0;

	curl_handle = curl_easy_init();
	if (curl_handle)
	{
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		result = curl_easy_perform(curl_handle);

		if (result != CURLE_OK)
		{
			fprintf(stderr, "error: %s\n", curl_easy_strerror(result));
		}
		else
		{
			//printf("Size: %lu\n", (unsigned long)chunk.size);
			//printf("Data: %s\n", chunk.memory);
		}
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
	}
}
