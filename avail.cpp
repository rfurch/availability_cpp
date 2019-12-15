
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







// -------------------------------------------------------------

class bwFile {

  public:
    bwFile()  {  }
    bool processFile(std::string& fileName, const std::string& ini, const std::string& fin);


  private:
    std::string fileName;
    boost::iostreams::mapped_file mappedFile;
    char *mappedFilePtr;
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
    

    bool setLimits();
    bool processFile(const std::string& dateTime, char &p);


};

// -------------------------------------------------------------

bool bwFile::setLimits() {

  int yearIni=0, monthIni=1, dayIni=1, hourIni=0, minIni=0;
  int yearFin=0, monthFin=1, dayFin=1, hourFin=0, minFin=0;

  std::cout << "Recevied:  INI -> " << this->ini << " FIN -> " << this->fin << std::endl;

  std::sscanf(this->ini.c_str(), "%04d%02d%02d%02d%02d", &yearIni, &monthIni, &dayIni, &hourIni, &minIni);
  std::sscanf(this->fin.c_str(), "%04d%02d%02d%02d%02d", &yearFin, &monthFin, &dayFin, &hourFin, &minFin);

  std::cout << "Searching from: " << hourIni << ":" << minIni << " " <<   dayIni << "/" << monthIni <<  "/" << yearIni ;
  std::cout << " To: " << hourFin << ":" << minFin << " " <<   dayFin << "/" << monthFin <<  "/" << yearFin <<  std::endl;

  if ( !myStrToTime(this->ini, this->iniTimeT ) )
    return false;   
 
  if ( !myStrToTime(this->fin, this->finTimeT ) )
    return false;


  return true;
}

// -------------------------------------------------------------

bool processFile(const std::string& dateTime, char &p) {


    return true;

}

// -------------------------------------------------------------


bool bwFile::processFile(std::string& fileName, const std::string& ini, const std::string& fin) {

  std::vector<uintmax_t> vec; 
  uintmax_t m_numLines = 0;
  //auto mf =  mmap(fileName.c_str(), );


  try {
      this->mappedFile.open(fileName.c_str(), boost::iostreams::mapped_file::readonly);
      }
  catch( const std::exception& e ) {
    std::cout << "\n ERRPR: Unable to open the file: " << fileName.c_str() << '\n' ;
    std::cerr << e.what() << '\n' ;
    exit(1);
    }


  this->mappedFilePtr = const_cast<char *> (this->mappedFile.const_data());
  auto l = this->mappedFilePtr + this->mappedFile.size();

  this->fileName = fileName;
  this->ini = ini;
  this->fin = fin;

  if ( DoesFileExist(fileName) != true )  {
      std::cout << "\n ERROR: Unable to read the file:  " << fileName << '\n';
      return false;
  }


  if ( ! this->setLimits() ) {
    std::cout << "ERROR Setting limits! ABORT" << std::endl;
      return false;
    }

  auto t1 = std::chrono::high_resolution_clock::now();

  // goto the first line in memory mapped file
  long long int fuploadBW=0, fdownloadBW=0;
  long long int fdate=0, ftimestamp=0;
  long int      counter=0;

  // the following line is a bit tricky, excluding fields and decimals to avoid float and double formats...
  std::sscanf(this->mappedFilePtr, "%lli.%*d,%lli.%*d,%*d,%*d,%lli.%*d,%lli.%*d", &ftimestamp, &fdate, &fuploadBW, &fdownloadBW);
  std::cout << "First line datetime: " << fdate << " timestamp: " << ftimestamp << "  up: " << fuploadBW << " down: " << fdownloadBW << std:: endl;
  this->fileIniTimeT = ftimestamp;
  this->fileIniDateTime = fdate;

  // goto the last line in memory mapped file
  fuploadBW=fdownloadBW=fdate=ftimestamp=counter=0;
  try {
    auto ptr = this->mappedFilePtr;
    if (ptr = static_cast<char*>(memrchr(ptr, '\n',  this->mappedFile.size()-2))) {
      std::sscanf(ptr+1, "%lli.%*d,%lli.%*d,%*d,%*d,%lli.%*d,%lli.%*d", &ftimestamp, &fdate, &fuploadBW, &fdownloadBW);
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

  // fill a vector of start lines positions in array. (after carriage return)
  auto localMapPtr = this->mappedFilePtr;
  auto orig = this->mappedFilePtr;
	vec.push_back(localMapPtr-localMapPtr);
    while (localMapPtr && localMapPtr!=l) {
        if ((localMapPtr = static_cast< char*>(memchr(localMapPtr, '\n', l-localMapPtr))))
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






  return true;
}


// -------------------------------------------------------------


// basic argument parser for the program

int process_program_options(const int argc, const char *const argv[],std::vector<std::string>& fileName, std::string& ini, std::string& fin)
{
  
  
  namespace po = boost::program_options; 

  auto now = std::chrono::system_clock::now();
  auto in_time_t_fin = std::chrono::system_clock::to_time_t(now);
  auto in_time_t_ini = in_time_t_fin - 3600 * 24 * 30 * 3;

  std::stringstream finTime;
  finTime << std::put_time(std::localtime(&in_time_t_fin), "%Y%m%d");
  std::cout <<  finTime.str() << std::endl;

  std::stringstream iniTime;
  iniTime << std::put_time(std::localtime(&in_time_t_ini), "%Y%m%d");
  std::cout <<  iniTime.str()<< std::endl;

  printVersions();
  
  try
    {
      options_description desc{"Options"};
      desc.add_options()
        ("help,h", "Help screen")
        ("pi", po::value<float>()->default_value(3.14f), "Pi")
        ("age", po::value<int>()->notifier(on_age), "Age")
        ("ini,I", po::value<std::string>(&ini)->default_value(iniTime.str()) , "Start date time. e.g.:  20181201 is equvalent to December the 1st, 00:00 ")
        ("fin,F", po::value<std::string> (&fin)->default_value(finTime.str()), "End date time. e.g.:  201812012300 is equvalent to December the 1st, 11:00 PM")
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
      else if (vm.count("pi"))
        std::cout << "Pi: " << vm["pi"].as<float>() << '\n';
      
      
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
    std::string ini, fin;
    std::vector<std::string> fileName; 
    bwFile bf;

    process_program_options(argc, argv, fileName, ini, fin);

    if ( ! bf.processFile(fileName[0], ini ,fin) )
      return 1;


}

