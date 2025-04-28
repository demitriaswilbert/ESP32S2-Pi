#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <set>

class CustomBLEDevice;

typedef enum
{
    TARGET_BLE = 0,
    TARGET_USB
} log_target_e;

typedef struct
{
    size_t len;
    char data[];
} data_len_t;

#ifndef log_cdc
size_t log_cdc_raw(const int target, const char *format, ...);
#define log_cdc(target, format, ...) log_cdc_raw(target, format "\n", ##__VA_ARGS__)
#endif

// Define UUIDs for the BLE UART service and characteristics
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class CustomBLEDevice : public BLEServerCallbacks, public BLECharacteristicCallbacks, public BLESecurityCallbacks, public Print
{
    SemaphoreHandle_t mTxLock = nullptr;
    QueueHandle_t rxQueue = nullptr;

    BLEServer *pServer = nullptr;
    BLEService *pService = nullptr;
    
    BLECharacteristic *pTxCharacteristic = nullptr;
    BLECharacteristic *pRxCharacteristic = nullptr;
    
    std::set<uint16_t> m_connected_ids;

    void onConnect(BLEServer *pServer) override
    {
        m_connected_ids.insert(pServer->getConnId());
        log_cdc(TARGET_USB, "Device connected");
        // BLEDevice::startAdvertising(); // Restart advertising after disconnection
    }

    void onDisconnect(BLEServer *pServer) override
    {
        m_connected_ids.erase(pServer->getConnId());
        log_cdc(TARGET_USB, "Device disconnected - restarting advertising...");
        BLEDevice::startAdvertising(); // Restart advertising after disconnection
    }

    void onWrite(BLECharacteristic *pCharacteristic) override
    {
        // String rxValue = pCharacteristic->getValue();
        const size_t data_length = pCharacteristic->getLength();
        const uint8_t* p_raw_data = pCharacteristic->getData();

        if (data_length > 0)
        {
            data_len_t *data_len = (data_len_t *)malloc(sizeof(data_len_t) + data_length);
            if (data_len == NULL)
                return;
            data_len->len = data_length;
            memcpy(data_len->data, p_raw_data, data_len->len);

            if (xQueueSend(rxQueue, &data_len, portMAX_DELAY) != pdTRUE)
                free(data_len);
        }
    }

    // Dummy implementation for onPassKeyRequest
    uint32_t onPassKeyRequest() override
    {
        log_cdc(TARGET_USB, "Passkey request received (not implemented)");
        return 123456; // Return 0 as a placeholder
    }

    // Dummy implementation for onPassKeyNotify
    void onPassKeyNotify(uint32_t pass_key) override
    {
        log_cdc(TARGET_USB, "Passkey notify received: %06d (not implemented)", pass_key);
    }

    // Dummy implementation for onSecurityRequest
    bool onSecurityRequest() override
    {
        log_cdc(TARGET_USB, "Security request received (not implemented)");
        return true;
    }

    // Existing implementation for onConfirmPIN
    bool onConfirmPIN(uint32_t pass_key) override
    {
        log_cdc(TARGET_USB, "Confirm PIN: %lu", pass_key);
        return true; // Always accept pairing
    }

    // Existing implementation for onAuthenticationComplete
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override
    {
        log_cdc(TARGET_USB, 
            "Authentication %s. Connected: %lu", 
            cmpl.success? "successful" : "failed", 
            connected());
    }

public:
    CustomBLEDevice()
        : mTxLock(nullptr), rxQueue(nullptr), pServer(nullptr), pService(nullptr), pTxCharacteristic(nullptr), pRxCharacteristic(nullptr), m_connected_ids()
    {
    }

    inline bool canRead() const {return rxQueue != NULL;}
    
    BaseType_t read(data_len_t **data, TickType_t xTicksToWait = portMAX_DELAY)
    {
        if (rxQueue == NULL)
            return pdFALSE;
        return xQueueReceive(rxQueue, data, xTicksToWait);
    }

    void begin(String deviceName);

    inline int connected() const { return m_connected_ids.size(); }

    size_t write(uint8_t c)
    {
        return write(&c, 1);
    }

    size_t write(const uint8_t *buffer, size_t size)
    {
        if (mTxLock == NULL)
            return 0;

        size_t bytesSent = 0;
        if (!connected())
            return bytesSent;
        if (xSemaphoreTake(mTxLock, 1000) == pdTRUE)
        {
            while (bytesSent < size) {
                uint8_t* pData = (uint8_t*) buffer + bytesSent;
                size_t bytesToSend = size - bytesSent > 500? 500 : size - bytesSent;
                pTxCharacteristic->setValue(pData, bytesToSend);
                pTxCharacteristic->notify();
                bytesSent += bytesToSend;
            }
            xSemaphoreGive(mTxLock);
        }
        return bytesSent;
    }

    inline size_t write(const char *buffer, size_t size)
    {
        return write((uint8_t *)buffer, size);
    }
    inline size_t write(const char *s)
    {
        return write((uint8_t *)s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t)n);
    }

    void end()
    {
        if (mTxLock == NULL)
            return;

        if (pServer != nullptr)
            pServer->setCallbacks(nullptr);
        if (pRxCharacteristic != nullptr)
            pRxCharacteristic->setCallbacks(nullptr);

        BLEDevice::deinit(true);
        pServer = nullptr;
        pService = nullptr;
        pTxCharacteristic = nullptr;
        pRxCharacteristic = nullptr;
        m_connected_ids.clear();

        if (rxQueue != NULL)
        {
            data_len_t *pData = nullptr;
            while (xQueueReceive(rxQueue, &pData, 0) == pdTRUE)
                free(pData);
            vQueueDelete(rxQueue);
            rxQueue = NULL;
        }

        if (mTxLock != NULL)
        {
            if (xSemaphoreTake(mTxLock, portMAX_DELAY) == pdTRUE)
            {
                vTaskSuspendAll();
                xSemaphoreGive(mTxLock);
                vSemaphoreDelete(mTxLock);
                mTxLock = NULL;
                xTaskResumeAll();
            }
        }
    }

    ~CustomBLEDevice() { end(); }
};