
#include <osg/ConcurrencyViewerMacros>


using namespace osg;

bool CVMarkerSeries::sMarkersActive = false;

#ifdef WIN32
#define CV_PROFILING 
#endif // WIN32

#if _MSC_VER < 1700
#undef CV_PROFILING
#endif

#ifdef  CV_PROFILING 
#include <sstream>
#include <thread>


#include <cvmarkersobj.h>
using namespace Concurrency::diagnostic;

#include <stdarg.h>


CVMarkerSeries::CVMarkerSeries() : myMarkerSeries(NULL) {
   if (sMarkersActive) {
      myMarkerSeries = new marker_series();
   }
}

CVMarkerSeries::CVMarkerSeries(const char * name, bool add_thread_id ) : myMarkerSeries(NULL)
{
   if (!sMarkersActive) {
      return;
   }
   if (add_thread_id) {
      std::string task = name;
      std::ostringstream ss;
      ss << std::this_thread::get_id();
      task += " ";
      task += ss.str();// boost::lexical_cast<std::string>(boost::this_thread::get_id());
      myMarkerSeries = new marker_series(task.c_str());
   }
   else {
      myMarkerSeries = new marker_series(name);

   }
   
}

CVMarkerSeries::~CVMarkerSeries()
{
   delete myMarkerSeries;
}


void CVMarkerSeries::write_flag(const char * message, ...)
{
   if (!myMarkerSeries) {
      return;
   }
   char buffer[256];
   va_list args;
   va_start(args, message);
   _vsnprintf(buffer, 256, message, args);
   myMarkerSeries->write_flag(1, &buffer[0]);
   va_end(args);
}

void CVMarkerSeries::write_message(const char * message, ...)
{
   if (!myMarkerSeries) {
      return;
   }
   char buffer[256];
   va_list args;
   va_start(args, message);
   _vsnprintf(buffer, 256, message, args);
   myMarkerSeries->write_message(1, &buffer[0]);
   va_end(args);
}



void CVMarkerSeries::write_alert(const char * message, ...)
{
   if (!myMarkerSeries) {
      return;
   }
   char buffer[256];
   va_list args;
   va_start(args, message);
   _vsnprintf(buffer, 256, message, args);
   myMarkerSeries->write_alert(&buffer[0]);
   va_end(args);
}


CVSpan::CVSpan(CVMarkerSeries & series, int level, const char * name) : mySpan(NULL)
{
   if (CVMarkerSeries::sMarkersActive && series.myMarkerSeries) {
      mySpan = new span(*series.myMarkerSeries, level, name);
   }
}

CVSpan::~CVSpan()
{
   delete mySpan;
   mySpan = NULL;
}

void CVSpan::reset_span()
{
   delete mySpan;
   mySpan = NULL;
}

#else

CVMarkerSeries::CVMarkerSeries() {
   myMarkerSeries = 0L;
}

CVMarkerSeries::CVMarkerSeries(const char * name, bool add_thread_id)
{
   myMarkerSeries = 0L;
}

CVMarkerSeries::~CVMarkerSeries() {
   ;
}

void CVMarkerSeries::write_flag(const char * flag, ...) {}

void CVMarkerSeries::write_message(const char * message, ...) {}

void CVMarkerSeries::write_alert(const char * alert, ...) {}


//////////////////////////////////////////////////////////////////////
CVSpan::CVSpan(CVMarkerSeries &, int, const char *)
{
   mySpan = 0L;
}

CVSpan::~CVSpan()
{
   ;
}

void CVSpan::reset_span()
{
}

#endif