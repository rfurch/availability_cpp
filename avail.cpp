
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "boost/date_time/local_time/local_time.hpp"

#include <boost/iostreams/device/mapped_file.hpp> // for mmap
#include <algorithm>  // for std::find
#include <iostream>   // for std::cout
#include <cstring>
#include <chrono>
#include <string>

#include "util.hpp"

using namespace boost::program_options;
using namespace boost::posix_time;

std::uint16_t  _verbosity = 0U;

// -------------------------------------------------------------
// -------------------------------------------------------------
// representation of a minute of BW measurment, with time, download and upload rate of that minute

class minuteSample{

  public:
    minuteSample()  { t=0 ; minute=""; upload=download=0; measured=false; }
    time_t getT() { return t; }
    void setT(time_t t) { this->t = t; }
    void setMinute(std::string minute) { this->minute = minute; }
    std::string getMinute() { return this->minute; }
    long long int getUpload() { return this->upload; }
    long long int getDownload() { return this->download; }
    bool isMessured() { return this->measured; }

    void setValues(time_t t, std::string minute, long long int upload, long long int download) {
      this->t = t; this->measured = true; this->minute = minute; if (upload > this->upload) this->upload = upload; if (download > this->download) this->download = download;
      }

    void print() {
      std::cout << "Minute: " << this->minute << " time_t: " << this->t << " IBW: " << this->upload << " OBW: " << this->download << std::endl; 
    }  

  private:
    time_t         t;
    std::string    minute;
    long long int  upload, download; 
    bool           measured;
  };

// -------------------------------------------------------------
// representation of BW measurment, vector of minutes

class bwSample{

  public:
    bwSample( std::time_t ini, std::time_t fin) {
      this->totalMinutes = (fin  - ini) / 60;
      this->minutesVector.resize( this->totalMinutes );

      // fill minutes
      time_t    t = ini; 
      std::tm   *ptm = NULL;
      char      buffer[50];

      // go through vector setting minute to proper value 
      for (auto & i: minutesVector){
        ptm = std::localtime(&t);
        std::strftime(buffer, 32, "%Y%m%d%H%M", ptm);

        if (t>=ini && t<=fin) {
          i.setMinute(buffer); 
        }     
        t += 60;
      }

      if (_verbosity > 2)   
        std::cout << " Elements: " << this->minutesVector.size() << " dsize: "  << this->totalMinutes << std::endl;  
    }

    // set one element values, by index 
    int setSampleValue(long long int index, time_t t, std::string minute, long long int upload, long long int download) {
      if (index >= 0 && index < minutesVector.size())  {
        if (minute != minutesVector[index].getMinute())
          std::cout << " ERROR:  Expected minute: " << minute << " Found: " <<   minutesVector[index].getMinute() << std::endl ;     

        minutesVector[index].setValues(t, minute,  upload, download);
      }
    }

    // basic minute calculation over minutesVector 
    // threshold: kbps  (as in files)
    bool calculateAvailability(float threshold, long int &measuredMinutes, long int &availableMinutes, float &measurePercentage, float &availabilityPercentage, float &maxPossibleAvailabilityPercentage)  {
      measuredMinutes = availableMinutes = 0;
      for (auto & i: minutesVector)  {
        if ( i.isMessured()) {     // there is a measurment fo rthis minute
          measuredMinutes++;
          if ( i.getDownload() >= threshold || i.getUpload() >= threshold ) // measured and with traffic
            availableMinutes++;
        }
      }  

      measurePercentage = (100.0 * (float)measuredMinutes) / this->totalMinutes;
      availabilityPercentage = (100 * (float)availableMinutes) / this->totalMinutes;
      // following formula is: minutes with verified availability  + non measured minutes
      // (we consider all non measured minutes as 'available' [with traffic])
      // just as a reference!
      maxPossibleAvailabilityPercentage = (100 * (float)(availableMinutes + (this->totalMinutes - measuredMinutes))) / this->totalMinutes;
      
      return true;
    }

    void printAll() {
      std::cout << " Elements: " << minutesVector.size() << std::endl;  
      for (auto & i: minutesVector)
        i.print();
    }

    long int getTotalMinutes() { return this->totalMinutes; }

  private:
    std::vector<minuteSample>   minutesVector; 
    long long int               totalMinutes;

  };

// -------------------------------------------------------------

class bwFile {

  public:
    bwFile()  {  }
    bool processFile(std::string& fileName, const std::string& ini, const std::string& fin, float &threshold);

  private:
    std::string fileName;
    boost::iostreams::mapped_file mappedFile;
    char *mappedFilePtr, *finalPointer;
    std::string ini, fin;
    long totalMinutes=0;  
    std::time_t iniTimeT = 0;
    std::time_t finTimeT = 0;
    std::time_t fileIniTimeT = 0; 
    std::time_t fileFinTimeT = 0; 
    bool startTimeNotInFile = false;
    bool finalTimeNotInFile = false;
    std::string fileIniDateTime;
    std::string fileFinDateTime;
    char *iniPoint, *finPoint;
    long long totalLen;


    bool setLimits();
    bool findDate(const std::string& dateTime, char *p[]);
    bool parseBWLine(char *line, long long int *timeT, char *timeTAsString, long long int *dateTime, char *dateTimeAsString, long long int *ibw, long long int *obw );

};

// -------------------------------------------------------------

bool bwFile::setLimits() {

  int yearIni=0, monthIni=1, dayIni=1, hourIni=0, minIni=0;
  int yearFin=0, monthFin=1, dayFin=1, hourFin=0, minFin=0;

  if (_verbosity > 0) {
    std::cout << "Recevied:  INI -> " << this->ini << " ( " << this->iniTimeT << " ) " << " FIN -> " << this->fin << " ( " << this->finTimeT << " ) " << std::endl;

    std::sscanf(this->ini.c_str(), "%04d%02d%02d%02d%02d", &yearIni, &monthIni, &dayIni, &hourIni, &minIni);
    std::sscanf(this->fin.c_str(), "%04d%02d%02d%02d%02d", &yearFin, &monthFin, &dayFin, &hourFin, &minFin);

    std::cout.fill('0');
    std::cout << "Searching from: " << std::setw(2) << hourIni << ":" << std::setw(2) << minIni << " " << std::setw(2) <<  dayIni << "/" << std::setw(2) << monthIni <<  "/" << yearIni ;
    std::cout << " To: " << std::setw(2) << hourFin << ":" << std::setw(2) << minFin << " " << std::setw(2) << dayFin << "/" << std::setw(2) << monthFin <<  "/" << yearFin <<  std::endl;
  }
  return true;
}

// -------------------------------------------------------------
// parse line with following format:
//
// 1546394403.300,20190101230003.300,11460269201223,179682563068181,60396.48,1118541.98,67618.60,1259495.14,73568.62,1325507.24,74601.12,1307573.67,73569.10,1433713.96,0,0,0.2742,0.3483
//    time_t            dateTime                                      IBW       OBW
// 
// This is a replacment for sscanf, which probed to be VERY slow

bool bwFile::parseBWLine(char *line, long long int *timeT, char *timeTAsString, long long int *dateTime, char *dateTimeAsString, long long int *ibw, long long int *obw )   {
char aux[1000];

if ( !line )
  return false;

if (timeT || timeTAsString) {   // get time_t, until first dot
  int i=0;
  for (i=0 ; *line != '.' && line < this->finalPointer ; i++, line++) 
    aux[i] = *line;
  aux[i] = 0;
  if (timeT)
    *timeT = std::atoll(aux);
  if (timeTAsString)  
    std::strcpy(timeTAsString, aux);
  }

// goto comma
for ( ; *line != ','  && line < this->finalPointer;  line++);
line++;

if (dateTime || dateTimeAsString) {  // get dateTime_t, until dot
   int i=0;
  for (i=0 ; *line != '.'  && line < this->finalPointer; i++, line++) 
    aux[i] = *line;
  aux[i] = 0;
  if (dateTime)
    *dateTime = std::atoll(aux);
  if (dateTimeAsString)  
    std::strcpy(dateTimeAsString, aux);
  }

if (ibw || obw) {  // goto comma
  for ( ; *line != ','  && line < this->finalPointer;  line++);
  line++;

  // goto comma, skip first counter
  for ( ; *line != ','  && line < this->finalPointer;  line++);
  line++;

  // goto comma, skip second counter
  for ( ; *line != ','  && line < this->finalPointer;  line++);
  line++;

  if (ibw) {  // get ibw, until dot
    int i=0;
    for (i=0 ; *line != '.' && line < this->finalPointer ; i++, line++) 
      aux[i] = *line;
    aux[i] = 0;
    *ibw = std::atoll(aux);
    }

  // goto comma
  for ( ; *line != ','  && line < this->finalPointer;  line++);
  line++;

  if (obw) {  // get obw, until dot
    int i=0;
    for (i=0 ; *line != '.'  && line < this->finalPointer; i++, line++) 
      aux[i] = *line;
    aux[i] = 0;
    *obw = std::atoll(aux);
    }
  }

return true;
}

// -------------------------------------------------------------

// simple binary search in flat file, mapped in memory

bool bwFile::findDate(const std::string& dateTime, char *p[]) {

  bool        found = false;
  char        ftimestamp[100],fileDate[100], ftimestamp_prev[100],fileDate_prev[100];
  long  int   chunkSize=this->mappedFile.size(), position=0;
  int         ret=0;
  auto        localPointer = this->mappedFilePtr + position;
  // goto 50% 
  chunkSize /= 2;   
  for  (int counter=0 ; !found && std::abs( chunkSize ) > 50 && counter < 100 ; counter++)  {
    
    position += chunkSize;    // pointer to memory location we expect data is located
    localPointer = this->mappedFilePtr + position;

    // is pointer in range 
    if (localPointer >= this->mappedFilePtr && localPointer <= (this->finalPointer)) {
      // find begining of line (after NEWLINE) and scan two comma separated fields
      auto startOfLine = static_cast< char*>(memchr(localPointer, '\n', this->finalPointer-localPointer));
      //std::sscanf(startOfLine, "%[^,],%[^,]",  ftimestamp, fileDate);
      this->parseBWLine(startOfLine, NULL, ftimestamp, NULL, fileDate, NULL, NULL);

      // basic binary jump back or forth (+-1/2, +-1/4, +-1/8, etc)
      if ( (ret = std::strncmp( dateTime.c_str(), fileDate, dateTime.length())) < 0 )
        chunkSize = -std::abs(chunkSize) / 2; 
      else if ( ret > 0 ) 
        chunkSize = std::abs(chunkSize) / 2;    // in case of lower
      else {
        found = true;
        *p = startOfLine;
        break;
      }
    }
  }

  if (!found)   { // check near match...  we iterate over several consecutive lines  
    localPointer =  (localPointer > (this->mappedFilePtr + 300)) ? (localPointer - 300) : this->mappedFilePtr;

    for  (int counter=0 ; !found && (localPointer < this->finalPointer) && counter < 10 ; counter++)  {

      auto startOfLine = static_cast< char*>(memchr(localPointer, '\n', finalPointer-localPointer));
      this->parseBWLine(startOfLine, NULL, ftimestamp, NULL, fileDate, NULL, NULL);
      
      if (_verbosity > 2)   
        std::cout << "LINE -> file: " << fileDate  << " Req: "  << dateTime << " Chunksize: " << chunkSize << " Position: " << position << std:: endl;

      // check if exact match happened
      if ( (std::strncmp( dateTime.c_str(), fileDate, dateTime.length())) == 0 )  {
        found = true;
        *p = startOfLine;
        break;      
      } // check if previous line is lower and next is bigger
      else if (counter > 0 && ( std::strncmp( fileDate_prev, dateTime.c_str(), dateTime.length()) < 0 ) &&
        ( std::strncmp( fileDate, dateTime.c_str(), dateTime.length()) > 0 ) ) {
        found = true;
        *p = startOfLine;
        break;          
        }

      localPointer = startOfLine + 1;
      std::strcpy(fileDate_prev, fileDate);
      std::strcpy(ftimestamp_prev, ftimestamp);
    }
  }
      
    if (_verbosity > 2)   
      std::cout << "LINE -> file: " << fileDate  << " Req: "  << dateTime << " Chunksize: " << chunkSize << " Position: " << position << std:: endl;
    return found;
}

// -------------------------------------------------------------

bool bwFile::processFile(std::string& fileName, const std::string& ini, const std::string& fin, float &threshold) {

  std::vector<uintmax_t> vec; 
  uintmax_t m_numLines = 0;

  this->fileName = fileName;

  // convert string (201911012300)  to time_t
  if ( !myStrToTime(ini, this->iniTimeT ) ) return false;   
  if ( !myStrToTime(fin, this->finTimeT ) ) return false;

  if (ini > fin) {   // are times inverted?
    this->ini = fin;
    this->fin = ini;
    std::swap(this->iniTimeT, this->finTimeT);
    }
   else {
    this->ini = ini;
    this->fin = fin;    
   } 
  
  if ( ! this->setLimits() ) {
    std::cout << "ERROR Setting limits! ABORT" << std::endl;
      return false;
    }

  if ( DoesFileExist(fileName) != true )  {
      std::cout << "\n ERROR: Unable to read the file:  " << fileName << '\n';
      return false;
  }

  try {
      this->mappedFile.open(fileName.c_str(), boost::iostreams::mapped_file::readonly);
      }
  catch( const std::exception& e ) {
    std::cout << "\n ERRPR: Unable to open the file: " << fileName.c_str() << '\n' ;
    std::cerr << e.what() << '\n' ;
    exit(1);
    }

  this->mappedFilePtr = const_cast<char *> (this->mappedFile.const_data());
  this->totalLen = this->mappedFile.size();
  this->finalPointer = const_cast<char *>  (this->mappedFilePtr + this->mappedFile.size());

  auto t1 = std::chrono::high_resolution_clock::now();

  // goto the first line in memory mapped file
  long long int fuploadBW=0, fdownloadBW=0;
  long long int fdate=0, ftimestamp=0;
  long int      counter=0;
  long int      measuredMinutes=0, availableMinutes = 0;
  float         measurePercentage = 0.0, availabilityPercentage = 0.0, maxPossibleAvailabilityPercentage = 0.0;
  
  if (_verbosity > 2)   
    std::cout << "File: "   << fileName << "   Total Length: " << this->mappedFile.size() << std::endl;

  // the following line is a bit tricky, excluding fields and decimals to avoid float and double formats...
  std::sscanf(this->mappedFilePtr, "%lli.%*d,%lli.%*d,%*d,%*d,%lli.%*d,%lli.%*d", &ftimestamp, &fdate, &fuploadBW, &fdownloadBW);
  if (_verbosity > 2)   
    std::cout << "First line datetime: " << fdate << " timestamp: " << ftimestamp << "  up: " << fuploadBW << " down: " << fdownloadBW << std:: endl;
  this->fileIniTimeT = ftimestamp;
  this->fileIniDateTime = fdate;

  // goto the last line in memory mapped file
  fuploadBW=fdownloadBW=fdate=ftimestamp=counter=0;
  try {
    auto ptr = this->mappedFilePtr;
    if (ptr = static_cast<char*>(memrchr(ptr, '\n',  this->mappedFile.size()-2))) {
      std::sscanf(ptr+1, "%lli.%*d,%lli.%*d,%*d,%*d,%lli.%*d,%lli.%*d", &ftimestamp, &fdate, &fuploadBW, &fdownloadBW);
      if (_verbosity > 2)   
        std::cout << "Last line datetime: " << fdate << " timestamp: " << ftimestamp << "  up: " << fuploadBW << " down: " << fdownloadBW << std:: endl;
      this->fileFinTimeT = ftimestamp;
      this->fileFinDateTime = fdate;
      }
    else 
        std::cout << "memrchr NULL !!! "  << std:: endl;
    }
  catch( const std::exception& e ) {
    std::cout << "\n ERROR: Unable to get las line of the file: " << '\n' ;
    std::cerr << e.what() << '\n' ;
    }

  // check if start time id higher than file last line -> nothing to do!
  if (this->iniTimeT > this->fileFinTimeT )  {
    std::cout << "\n ERROR: Start time ("  << this->iniTimeT <<  ") exceeds file end time (" << this->fileFinTimeT << "). Aborting.... " << '\n' ;
    exit (1);
  }

  // check if end time is lower than file last line -> nothing to do!
  if (this->finTimeT < this->fileIniTimeT )  {
    std::cout << "\n ERROR: End time (" << this->finTimeT << ") prior to file start time (" << this->fileIniTimeT <<"). Aborting.... " << '\n' ;
    exit (1);
  }

  // check if start time id lower than file first line -> Warning!
  if (this->iniTimeT < this->fileIniTimeT )  {
    std::cout << "\n WARNING: Start time ("  << this->iniTimeT <<  ") prior to file start time (" << this->fileIniTimeT << "). .... " << '\n' ;
    this->startTimeNotInFile = true;
  }

  // check if end time is higher than file last line -> nothing to do!
  if (this->finTimeT > this->fileFinTimeT )  {
    std::cout << "\n WARNING: End time (" << this->finTimeT << ") exceeds file end time (" << this->fileFinTimeT <<"). .... " << '\n' ;
  }

    measureExecTime(timeMeasureStart);



    // everything OK, find first point
    if (findDate(ini, &(this->iniPoint))) {
      
      if (findDate(fin, &(this->finPoint))) {   // and final point.  Now insert into vector
        char fileDate[100];

        bwSample bw( this->iniTimeT, this->finTimeT );

        if (_verbosity > 2)   
          std::cout <<  " Total Minutes: " <<   ((this->finTimeT - this->iniTimeT)/60)  << std::endl;

        auto localPointer = this->iniPoint;

        while (localPointer >= this->iniPoint && localPointer <= (this->finPoint)) {
        // find begining of line (after NEWLINE) and scan two comma separated fields
        auto startOfLine = static_cast< char*>(memchr(localPointer, '\n', this->finalPointer-localPointer));
        this->parseBWLine(startOfLine, &ftimestamp, NULL, &fdate, fileDate, &fuploadBW, &fdownloadBW);

        // add data to vector
        if (ftimestamp >= this->iniTimeT && ftimestamp <= this->finTimeT) {
          fileDate[12]=0;
          bw.setSampleValue( (ftimestamp - this->iniTimeT)/60 , ftimestamp, std::string(fileDate), fuploadBW, fdownloadBW );
          }

        localPointer = startOfLine+1; 
        }
      // Print vector
      if (_verbosity > 2)   
        bw.printAll();
      
      bw.calculateAvailability(threshold, measuredMinutes, availableMinutes, measurePercentage, availabilityPercentage, maxPossibleAvailabilityPercentage);
    
      if (_verbosity > 1) {
        std::cout << std::setprecision(4) << std::fixed;
        std::cout << "Threshold: " << threshold << " measuredMinutes: "<< measuredMinutes << " availableMinutes: " << availableMinutes <<  " totalMinutes: " << bw.getTotalMinutes() << std::endl ; 
        std::cout << "measurePercentage: " << measurePercentage << " availabilityPercentage: "<< availabilityPercentage << " maxPossibleAvailabilityPercentage: " << maxPossibleAvailabilityPercentage <<  std::endl ; 
        }
      }
    }  

    measureExecTime(timeMeasureStop);


  return true;
}

// -------------------------------------------------------------

// basic argument parser for the program

int process_program_options(const int argc, const char *const argv[],std::vector<std::string>& fileName, std::string& ini, std::string& fin, float &threshold)
{ 
  namespace po = boost::program_options; 

  auto now = std::chrono::system_clock::now();
  auto in_time_t_fin = std::chrono::system_clock::to_time_t(now);
  auto in_time_t_ini = in_time_t_fin - 3600 * 24 * 30 * 3;

  std::stringstream finTime;
  finTime << std::put_time(std::localtime(&in_time_t_fin), "%Y%m%d");
  if (_verbosity > 2)   
    std::cout <<  finTime.str() << std::endl;

  std::stringstream iniTime;
  iniTime << std::put_time(std::localtime(&in_time_t_ini), "%Y%m%d");
  if (_verbosity > 2)   
    std::cout <<  iniTime.str()<< std::endl;

  printVersions();
  
  try
    {
      options_description desc{"Options"};
      desc.add_options()
        ("help,h", "Help screen")
        ("threshold,t", po::value<float>(&threshold)->default_value(10), "Traffic threshold in kbps (default: 10 kbps)")
        ("age", po::value<int>()->notifier(on_age), "Age")
        ("ini,I", po::value<std::string>(&ini)->default_value(iniTime.str()) , "Start date time. e.g.:  20181201 is equvalent to December the 1st, 00:00 ")
        ("fin,F", po::value<std::string> (&fin)->default_value(finTime.str()), "End date time. e.g.:  201812012300 is equvalent to December the 1st, 11:00 PM")
        ("verbose,v", po::value<std::uint16_t> (&_verbosity)->default_value(0), "Print more verbose messages at each additional verbosity level.")
        ("filename,f", po::value< std::vector<std::string> > (&fileName), "filename, can specify many!");
        
      variables_map vm;
      store(parse_command_line(argc, argv, desc), vm);
      notify(vm);

      if (vm.count("help"))  {
        std::cout << desc << '\n';
        exit(1);
        }
      else if (vm.count("age"))
        std::cout << "Age: " << vm["age"].as<int>() << '\n';
      else if (vm.count("threshold"))
        std::cout << "threshold: " << vm["threshold"].as<float>() << '\n';
      
      
      if (! vm.count("filename"))  {
        std::cout << desc << '\n';
        std::cout << "\n filename IS REQUIRED! " <<  '\n';
        exit(1); 
      }
    }
  catch (const error &ex) 
    {
      std::cerr << ex.what() << '\n';
      exit(1);
    }
}

// -------------------------------------------------------------

int main(int argc, const char *argv[])
{
    std::string               ini, fin;
    std::vector<std::string>  fileName; 
    bwFile                    bf;
    float                     threshold=0.0;

    process_program_options(argc, argv, fileName, ini, fin, threshold);

    if ( ! bf.processFile(fileName[0], ini ,fin, threshold) )
      return 1;


}

// -------------------------------------------------------------
// -------------------------------------------------------------

/*   not required, but intersting!

{
  // fill a vector of start lines positions in array. (after carriage return)
  auto localMapPtr = this->mappedFilePtr;
  auto orig = this->mappedFilePtr;
	vec.push_back(localMapPtr-localMapPtr);
    while (localMapPtr && localMapPtr < this->finalPointer) {
        if ((localMapPtr = static_cast< char*>(memchr(localMapPtr, '\n', finalPointer-localMapPtr))))
            {
      			m_numLines++, localMapPtr++;
			      vec.push_back(localMapPtr - orig);
			      }
	    }

  auto t2 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
  std::cout << "\n Execution time in microseconds: " <<  duration << std::endl;

    std::cout << "m_numLines = " << m_numLines << "\n";
    //std::cout << "f: " << mappedFilePtr <<  "l:  " << l << "\n";    
    std::cout << "vec[1]: " << vec[1] << "\n";
    std::cout << "vec[2]: " << vec[2] << "\n";
    std::cout << "vec[20]: " << vec[20] << "\n";    
    std::cout << "vec[max-1]: " << vec[vec.size() - 1] << "("  << vec.size() - 1 << ")" << "\n";

//      std::sscanf(startOfLine, "%[^,],%[^,]",  ftimestamp, fileDate);


    }
    */  // end of not requred block...
// -------------------------------------------------------------
