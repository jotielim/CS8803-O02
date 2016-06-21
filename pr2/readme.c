/* ============================================================================
HOW TO RUN EXPERIMENTS
===============================================================================
Use the following functions to run experiments:

void RUN_FIXED( unsigned int server_worker_threads,
                int request_pattern,
                unsigned long long int request_size );

void RUN_MIXED(	unsigned int server_worker_threads,
                int request_pattern,
                unsigned long long int request_size_min,
                unsigned long long int request_size_max );

server_worker_threads - number of server worker threads
request_pattern - FIXED_FILE, FIXED_SIZE, or MIXED_FILES (#define)
request_size - the size of file(s) in bytes
request_size_min - the minimum size of files in bytes
request_size_max - the maximum size of files in bytes

===============================================================================
EXAMPLE EXPERIMENTS
===============================================================================
// 1 server thread, requests same 1kB file many times
RUN_FIXED(1, FIXED_FILE, 1024);	

// 20 server threads, requests many 1MB files many times
RUN_FIXED(20, FIXED_SIZE, 1048576); 

// 375 server threads, requests many files ranging from 1kB to 10kb many times
RUN_MIXED(375, MIXED_FILES, 1024, 10240);

===============================================================================
TIPS
===============================================================================
If you have not read the Project Design Recommendations, then please review 
the Project 2 Description again before proceeding.

The functions listed above do not contain a way to run a "sets of experiments". 
To emulate this behavior, group (and comment) function calls into "sets".

Only use the above functions calls -- do not include any additional function calls.
    
============================================================================ */
