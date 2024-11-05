// author: katie knizner (featuring given starter code attached)
// date: 11/4/2024
// purpose: extra credit for lab 4 to get the chip the advertise

#include <string>  // For string handling
#include "mbed.h"
#include <cstdint>
#include <events/mbed_events.h>
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "USBSerial.h"

// USB serial for debugging
USBSerial usbTerm;

// BLE instance, GAP, and event queue
BLE &bleinit = BLE::Instance();
ble::Gap &gap = bleinit.gap();
static events::EventQueue event_queue(16 * EVENTS_EVENT_SIZE);

// Variable to hold the connection handle
ble::connection_handle_t connectionHandle = 0;

// Helper function to convert BLE address to string
std::string address_to_string(const ble::address_t &addr) {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return std::string(buf);
}

// GAP event handler to handle BLE events
struct GapEventHandler : public ble::Gap::EventHandler {
    void onAdvertisingReport(const ble::AdvertisingReportEvent &event) override {
        std::string peerAddress = address_to_string(event.getPeerAddress());
        usbTerm.printf("Advertisement found: %s, RSSI: %d dBm\n", 
                       peerAddress.c_str(), event.getRssi());

        // Extract and check the advertising name
        const uint8_t *payload = event.getPayload().data();
        size_t payload_size = event.getPayload().size();
        std::string advName = extract_advertising_name(payload, payload_size);

        // Check for the device name
        if (advName == "kt's phone") {
            usbTerm.printf("Found kt's phone! Connecting...\n");

            // Connect to the device
            gap.connect(event.getPeerAddressType(), event.getPeerAddress(),
                        ble::ConnectionParameters());
        }
    }

    void onConnectionComplete(const ble::ConnectionCompleteEvent &event) override {
        if (event.getStatus() == BLE_ERROR_NONE) {
            usbTerm.printf("Connected to device.\n");
            connectionHandle = event.getConnectionHandle();
        } else {
            usbTerm.printf("Connection failed. Status: %d\n", event.getStatus());
        }
    }

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override {
        usbTerm.printf("Disconnected. Reason: %d\n", event.getReason().value());
        // Restart scanning after disconnection
        gap.startScan();
    }

    // Helper function to extract the advertising name from the payload
    std::string extract_advertising_name(const uint8_t *payload, size_t size) {
        size_t index = 0;
        while (index < size) {
            uint8_t length = payload[index];
            if (length == 0 || index + length >= size) {
                break;  // End of payload or invalid length
            }

            uint8_t type = payload[index + 1];
            if (type == 0x09) {  // Complete Local Name type
                return std::string(reinterpret_cast<const char *>(&payload[index + 2]), length - 1);
            }
            index += length + 1;
        }
        return "";
    }
};

GapEventHandler THE_gap_EvtHandler;

// BLE initialization callback
void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
    if (params->error != BLE_ERROR_NONE) {
        usbTerm.printf("BLE Initialization failed. Error: %d\n", params->error);
        return;
    }
    usbTerm.printf("BLE Initialized successfully.\n");

    // Set the GAP event handler
    gap.setEventHandler(&THE_gap_EvtHandler);

    // Start scanning for BLE advertisements
    usbTerm.printf("Start BLE scan\n");
    gap.startScan();  // Start scanning with default parameters
}

// Schedule BLE events in the event queue
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(mbed::Callback<void()>(&context->ble, &BLE::processEvents));
}

// Main function
int main() {
    // Initialize BLE and set event handling
    bleinit.onEventsToProcess(schedule_ble_events);
    bleinit.init(on_init_complete);

    // Dispatch BLE events forever
    event_queue.dispatch_forever();
}
