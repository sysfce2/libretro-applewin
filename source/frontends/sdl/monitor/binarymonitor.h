#include <cstdint>
#include <cstddef>
#include <vector>
#include <map>
#include <memory>

#include "Core.h"

namespace common2
{
  class CommonFrame;
}

namespace binarymonitor
{

  class BinaryBuffer;

  class BinaryClient
  {
  public:
    BinaryClient(const int socket, common2::CommonFrame * frame);
    ~BinaryClient();

    bool process();

  private:

  #pragma pack(push)
  #pragma pack(1)
    struct Command
    {
      uint8_t stx;
      uint8_t version;
      uint32_t length;
      uint32_t request;
      uint8_t type;
    };

    struct Response
    {
      uint8_t stx;
      uint8_t version;
      uint32_t length;
      uint8_t type;
      uint8_t error;
      uint32_t request;
    };
  #pragma pack(pop)

    struct Register_t
    {
      const char * name;
      uint8_t size;  // in bytes
      void * ptr;
    };

    const std::map<uint8_t, Register_t> myAvailableRegisters;
    const std::map<uint8_t, std::string> myBankNames;

    int mySocket;
    common2::CommonFrame * myFrame;
    bool myStopped;

    Command myCommand;
    size_t myCommandRead;
    std::vector<uint8_t> myPayloadIn;
    size_t myPayloadRead;

    size_t readData(char * dest, size_t len);

    void reset();
    bool readCommand();
    bool readPayload();
    void processCommand();

    void throwIfError(const ssize_t result);

    void sendBreakpointIfHit();
    void sendReply(const BinaryBuffer & buffer, const uint8_t type, const uint32_t request, const uint8_t error);
    void sendResourceStringReply(const uint32_t request, const char * value);
    void sendResourceIntReply(const uint32_t request, const uint32_t value);
    void sendBreakpoint(const uint32_t request, const size_t i);
    void sendResume(const uint32_t request);

    void sendError(const uint8_t type, const uint8_t error);

    void cmdViceInfo();
    void cmdResourceGet();
    void cmdRegistersAvailable();
    void cmdDisplayGet();
    void cmdBanksAvailable();
    void cmdCheckpointSet();
    void cmdCheckpointGet();
    void cmdCheckpointDelete();
    void cmdCheckpointList();
    void cmdCheckpointToggle();
    void cmdRegistersGet();
    void cmdRegistersSet();
    void cmdMemoryGet();
    void cmdPaletteGet();
    void cmdExit();
    void cmdReset();
    void cmdAutostart();
    void cmdQuit();
    void cmdPing();

    void sendRegisters(const uint32_t request);
    void sendStopped();

    void enterMonitorState(const AppMode_e mode);
    void sendMonitorState(const AppMode_e mode);
  };

  class BinaryMonitor
  {
  public:
    BinaryMonitor(common2::CommonFrame * frame);
    ~BinaryMonitor();

    void process();

  private:
    common2::CommonFrame * myFrame;
    int mySocket;
    std::vector<std::shared_ptr<BinaryClient>> myClients;
  };

}
