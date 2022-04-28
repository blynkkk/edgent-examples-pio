#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <queue>

constexpr static char SERVICE_UUID[]            = "95e30001-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RX[]  = "95e30002-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_TX[]  = "95e30003-5737-45a9-a092-a88e2e5dd659";

class ConfigBLE :
    public BLEServerCallbacks,
    public BLECharacteristicCallbacks
{

public:
    ConfigBLE()
        : isConn(false)
    {}

    void begin(const char* name) {
        // Create the BLE Device
        BLEDevice::init(name);

        // Create the BLE Server
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(this);

        // Create the BLE Service
        pService = pServer->createService(SERVICE_UUID);

        // Create a BLE Characteristic
        pCharTX = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_TX,
                            BLECharacteristic::PROPERTY_NOTIFY);

        pCharTX->addDescriptor(new BLE2902());

        pCharRX = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_RX,
                            BLECharacteristic::PROPERTY_WRITE);

        pCharRX->setCallbacks(this);

        // Start the service
        pService->start();

        // Start advertising
        pServer->getAdvertising()->addServiceUUID(pService->getUUID());
        pServer->getAdvertising()->setScanResponse(true);
        pServer->getAdvertising()->start();
    }

    size_t write(const void* buf, size_t len) {
        pCharTX->setValue((uint8_t*)buf, len);
        pCharTX->notify();

        BLYNK_DBG_DUMP("<< ", buf, len);

        return len;
    }

    size_t write(const char* buf) {
        unsigned len = strlen(buf);
        return write(buf, len);
    }

    String read() {
        char* msg = rxQueue.front();
        String result = msg;
        free(msg);
        rxQueue.pop();
        return result;
    }
    
    bool available() {
        return !rxQueue.empty();
    }
    
    bool isConnected() {
        return isConn;
    }

private:

    void onConnect(BLEServer* pServer) {
        DEBUG_PRINT("BLE connected");
        isConn = true;
    }
    void onDisconnect(BLEServer* pServer) {
        DEBUG_PRINT("BLE disconnected");
        BLEDevice::startAdvertising();
        isConn = false;
    }

    void onWrite(BLECharacteristic *pChar) {
      std::string rxValue = pChar->getValue();

      if (rxValue.length() > 0) {
        uint8_t* data = (uint8_t*)rxValue.data();
        size_t len = rxValue.length();

        BLYNK_DBG_DUMP(">> ", data, len);
        
        char* msg = (char*)malloc(len+1);
        memcpy(msg, data, len);
        msg[len] = 0;   // Null-terminate string
        rxQueue.push(msg);
      }
    }

private:
    bool                    isConn;
    std::queue<char*>       rxQueue;
    BLEServer *pServer;
    BLEService *pService;
    BLECharacteristic *pCharTX;
    BLECharacteristic *pCharRX;
};
