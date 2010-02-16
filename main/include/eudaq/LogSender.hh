#ifndef EUDAQ_INCLUDED_LogSender
#define EUDAQ_INCLUDED_LogSender

#include "eudaq/TransportClient.hh"
#include "eudaq/Serializer.hh"
#include "eudaq/Status.hh"
#include <string>

namespace eudaq {

  class LogMessage;

  class LogSender {
  public:
    LogSender();
    ~LogSender();
    void Connect(const std::string & type, const std::string & name, const std::string & server);
    void SendLogMessage(const LogMessage &, bool show = true);
    void SetLevel(int level) { m_level = level; }
    void SetLevel(const std::string & level) { SetLevel(Status::String2Level(level)); }
  private:
    std::string m_name;
    TransportClient * m_logclient;
    int m_level;
    bool m_shownotconnected;
  };

}

#endif // EUDAQ_INCLUDED_LogSender