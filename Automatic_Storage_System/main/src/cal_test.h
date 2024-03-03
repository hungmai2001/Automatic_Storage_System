#include <time.h>
clock_t start, end;
		start = clock();
        end = clock();
		double time_taken = ((double)(end - start))/CLOCKS_PER_SEC; // in seconds
		ESP_LOGI(TAG, "MAIN: took %f mseconds to execute",time_taken*1000);