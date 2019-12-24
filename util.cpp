
#include <iostream>   // for std::cout
#include <chrono>

#include "util.hpp"

// -------------------------------------------------------------
// Stupid function / procedure

void on_age(int age)
{ 
  std::cout << "On age: " << age << '\n';
}

// -------------------------------------------------------------

// check if file exists AND it can be opened for reading
 
bool DoesFileExist (const std::string& name) {

  bool isOK = false;

  try {
    if (FILE *file = fopen(name.c_str(), "r")) {
      fclose(file);
      isOK = true;
      }
    }
  catch( const std::exception& e ) {
    std::cout << "\n ERRPR: Unable to open the file: " << name << '\n' ;
    std::cerr << e.what() << '\n' ;
    isOK = false;
    }
  return isOK;
}


// -------------------------------------------------------------

bool  myStrToTime(const std::string& timeStr, std::time_t& t )  {
  int year=0, month=1, day=1, hour=0, min=0;
  struct tm stm;

  std::sscanf(timeStr.c_str(), "%04d%02d%02d%02d%02d", &year, &month, &day, &hour, &min);

  if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || min < 0 || min > 59) {
    std::cout << "ERROR: bad date-time format: " << timeStr << std::endl;
    return false;
  }

  stm.tm_year = year - 1900;
  stm.tm_mon = month - 1;
  stm.tm_mday = day;
  stm.tm_hour = hour;
  stm.tm_min = min;
  stm.tm_sec = 0;
  stm.tm_isdst = -1;

 t = mktime(&stm);
 return true;
}

// -------------------------------------------------------------

// measure time between two consecutive calls:
// meassureExecTime(ini);
// code....
// ret = meassureExecTime(fin);

unsigned long long int meassureExecTime(exec_time t) {

  static auto t1 = std::chrono::high_resolution_clock::now();

  if (t == timeMeassureStart) {
    t1 = std::chrono::high_resolution_clock::now();
    return 0;
  }
  else { 
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
    std::cout << "\n Execution time in microseconds: " <<  duration << std::endl;
    return duration;
  }
}

// -------------------------------------------------------------

// print C++ version according to compiler...  just to check features

void printVersions() {

    std::cout << "C++ Version:  " ;

    if (__cplusplus == 201703L) std::cout << "C++17\n";
    else if (__cplusplus == 201402L) std::cout << "C++14\n";
    else if (__cplusplus == 201103L) std::cout << "C++11\n";
    else if (__cplusplus == 199711L) std::cout << "C++98\n";
    else std::cout << "pre-standard C++\n";

    std::cout << "Compile time: "  << __DATE__ << std::endl;
}

// -------------------------------------------------------------

// find character backwards in string, returns pointer to  next char

void *mymemrchr(const void *s, int c, size_t n)
{
    if (n > 0) {
        const char*  p = (const char*) s;
        const char*  q = p + n;

        while (1) {
            q--; if (q < p || q[0] == c) break;
            q--; if (q < p || q[0] == c) break;
            q--; if (q < p || q[0] == c) break;
            q--; if (q < p || q[0] == c) break;
        }
        if (q >= p)
            return (void*)q;
    }
    return NULL;
}

// -------------------------------------------------------------

