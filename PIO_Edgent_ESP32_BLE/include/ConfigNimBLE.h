#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <queue>

constexpr static char SERVICE_UUID[]            = "95e30001-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_RX[]  = "95e30002-5737-45a9-a092-a88e2e5dd659";
constexpr static char CHARACTERISTIC_UUID_TX[]  = "95e30003-5737-45a9-a092-a88e2e5dd659";

class ConfigBLE :
    public NimBLEServerCallbacks,
    public NimBLECharacteristicCallbacks
{

public:
    ConfigBLE()
        : isConn(false)
    {}

    void begin(const char* name) {
        // Create the BLE Device
        NimBLEDevice::init(name);

        NimBLEDevice::setPower(ESP_PWR_LVL_P9);

        // Create the BLE Server
        pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(this);

        // Create the BLE Service
        pService = pServer->createService(SERVICE_UUID);

        // Create a BLE Characteristic
        pCharTX = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_TX,
                            NIMBLE_PROPERTY::NOTIFY);

        pCharRX = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_RX,
                            NIMBLE_PROPERTY::WRITE_NR);

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

    void onConnect(NimBLEServer* pServer) {
        DEBUG_PRINT("BLE connected");
        isConn = true;
    }
    void onDisconnect(NimBLEServer* pServer) {
        DEBUG_PRINT("BLE disconnected");
        NimBLEDevice::startAdvertising();
        isConn = false;
    }
    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) {
        DEBUG_PRINTF("MTU updated: %u for connection ID: %u", MTU, desc->conn_handle);
    }

    void onWrite(NimBLECharacteristic *pChar) {
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
    NimBLEServer            *pServer;
    NimBLEService           *pService;
    NimBLECharacteristic    *pCharTX;
    NimBLECharacteristic    *pCharRX;
};
