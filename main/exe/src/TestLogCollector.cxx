#include <iostream>
#include <ostream>
#include <cctype>


#include "eudaq/Utils.hh"
#include "eudaq/LogCollector.hh"
#include "eudaq/Logger.hh"
#include "eudaq/OptionParser.hh"
#ifndef WIN32
#include <unistd.h>
#endif
class TestLogCollector : public eudaq::LogCollector {
  public:
    TestLogCollector(const std::string & runcontrol,
		     const std::string & listenaddress,
		     const std::string & directory,
		     int loglevel)
      : eudaq::LogCollector(runcontrol, listenaddress, directory),
      m_loglevel(loglevel), done(false)
  {}
    void OnConnect(const eudaq::ConnectionInfo & id) override{
      std::cout << "Connect:    " << id << std::endl;
    }
    void OnDisconnect(const eudaq::ConnectionInfo & id) override{
      std::cout << "Disconnect: " << id << std::endl;
    }

    virtual void OnReceive(const eudaq::LogMessage & ev) override{
      if (ev.GetLevel() >= m_loglevel) std::cout << ev << std::endl;
    }
    virtual void OnConfigure(const eudaq::Configuration & param) override {
      std::cout << "Configuring (" << param.Name() << ")..." << std::endl;
      LogCollector::OnConfigure(param);
      std::cout << "...Configured (" << param.Name() << ")" << std::endl;
      SetStatus(eudaq::Status::LVL_OK, "Configured (" + param.Name() + ")");
    }
    virtual void OnStartRun(unsigned param) {
      LogCollector::OnStartRun(param);
      SetStatus(eudaq::Status::LVL_OK, "Running");
    }
    virtual void OnStopRun()override {
      std::cout << "Stop Run" << std::endl;
      SetStatus(eudaq::Status::LVL_OK);
    }
    virtual void OnTerminate() override {
      SetStatus(eudaq::Status::LVL_OK, "LC Terminating");
      std::cout << "Terminating" << std::endl;
      done = true;
    }
    virtual void OnReset() override {
      std::cout << "Reset" << std::endl;
      SetStatus(eudaq::Status::LVL_OK);
    }
    virtual void OnStatus() override {
     // std::cout << "Status - " << m_status << std::endl;
    
    }
    virtual void OnUnrecognised(const std::string & cmd, const std::string & param) override {
      std::cout << "Unrecognised: (" << cmd.length() << ") " << cmd;
      if (param.length() > 0) std::cout << " (" << param << ")";
      std::cout << std::endl;
      SetStatus(eudaq::Status::LVL_ERROR, "Just testing");
    }
    int m_loglevel;
    bool done;
};

// static TestLogCollector * g_ptr = 0;
// void ctrlc_handler(int) {
//   if (g_ptr) g_ptr->done = 1;
// }

int main(int /*argc*/, const char ** argv) {
  eudaq::OptionParser op("EUDAQ Log Collector", "1.0", "A comand-line version of the Log Collector");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol", "tcp://localhost:44000", "address",
      "The address of the RunControl application");
  eudaq::Option<std::string> addr (op, "a", "listen-address", "tcp://44002", "address",
      "The address on which to listen for Log connections");
  eudaq::Option<std::string> directory (op, "d", "directory", "logs", "directory",
				   "The path in which the log files should be stored");
  eudaq::Option<std::string> level(op, "l", "log-level", "INFO", "level",
      "The minimum level for displaying log messages");
  eudaq::mSleep(2000);
  try {
    op.Parse(argv);
    EUDAQ_LOG_LEVEL("NONE");
    std::cout << "Log Collector  Connected to \"" << rctrl.Value() << "\"" << std::endl;
    TestLogCollector fw(rctrl.Value(), addr.Value(), directory.Value(), eudaq::Status::String2Level(level.Value()));
    //g_ptr = &fw;
    //std::signal(SIGINT, &ctrlc_handler);
    do {
      eudaq::mSleep(10);
    } while (!fw.done);
    std::cout << "Quitting" << std::endl;
  } catch (...) {
    return op.HandleMainException();
  }
  return 0;
}
