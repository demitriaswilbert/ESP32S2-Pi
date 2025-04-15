#include "ble_config.h"

void CustomBLEDevice::begin() {
    
    if (mTxLock == NULL)
    {
        mTxLock = xSemaphoreCreateMutex();
        if (mTxLock == NULL)
        {
            log_e("xSemaphoreCreateMutex failed");
            return;
        }
    }
    if (rxQueue == NULL)
    {
        rxQueue = xQueueCreate(0x100, sizeof(data_len_t *));
        if (rxQueue == NULL)
        {
            log_e("xQueueCreate failed");
            return;
        }
    }

    // Initialize BLE device
    BLEDevice::init("Pi Sensor");
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);

    // Create BLE security configuration
    BLESecurity* pSecurity = new BLESecurity();
    // // Set a static PIN (optional, for testing purposes)
    // pSecurity->setStaticPIN(123456); // Use "123456" as the static passkey
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY); // Enable SC, MITM, and bonding
    pSecurity->setCapability(ESP_IO_CAP_NONE); // Display passkey on ESP32, confirm on peer
    // pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    // Set response encryption key
    // pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    // Set encryption key size
    // pSecurity->setKeySize(16); // 128-bit encryption key


    // Set global security callbacks
    BLEDevice::setSecurityCallbacks(this);

    // Create BLE server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);
    
    // Create BLE service
    pService = pServer->createService(SERVICE_UUID);

    // Create BLE characteristics
    pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(this);
    pRxCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED | ESP_GATT_PERM_READ_ENCRYPTED); // Protect RX characteristic

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pTxCharacteristic->addDescriptor(new BLE2902());
    pTxCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED); // Protect TX characteristic
    // Start the service
    pService->start();
    BLEDevice::setMTU(517); // Set the MTU size to 517 (maximum for BLE)
    BLEDevice::setPower(ESP_PWR_LVL_N0);

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x100); // Set minimum advertising interval
    pAdvertising->setMaxPreferred(0x200); // Set maximum advertising interval
    BLEDevice::startAdvertising();

    log_cdc(TARGET_USB, "BLE initialized and advertising started.");
}