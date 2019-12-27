// -------------------------------------------------------------


enum exec_time {timeMeasureStart=0, timeMeasureStop}; 

// -------------------------------------------------------------

void on_age(int age);
bool DoesFileExist (const std::string& name);
bool  myStrToTime(const std::string& timeStr, std::time_t& t );
void printVersions(); 
void *mymemrchr(const void *s, int c, size_t n);

unsigned long long int measureExecTime(exec_time t);
