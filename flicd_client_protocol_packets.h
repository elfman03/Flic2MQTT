/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 * NOTE: 
 *
 * File Source: based on https://github.com/50ButtonsEach/fliclib-linux-hci/tree/master/simpleclient)
 * File License CC0 (see https://github.com/50ButtonsEach/fliclib-linux-hci/blob/master/README.md)
 *
 */

/*
 * Specification of the protocol messages used in the Flic protocol.
 * 
 * Note: 16-bit little endian length header is prepended to each packet. The length of the length field itself is not included in the length.
 * 
 * Note: These structures are only valid on little endian platforms.
 * 
 */

#ifndef CLIENT_PROTOCOL_PACKETS_H
#define CLIENT_PROTOCOL_PACKETS_H

#include <stdint.h>

#ifdef __cplusplus
namespace FlicClientProtocol {
#endif

#ifdef __GNUC__
// need MSVC equivalent of packed attribute for this block of definitions (both for packed structures and packed enums)
// base solution on https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed
// https://stackoverflow.com/questions/837319/packing-enums-using-the-msvc-compiler
#define PACKED __attribute__((packed))
#define PACKED_ENUM_PRE 
#else
#define PACKED
#define PACKED_ENUM_PRE : unsigned char
#endif

#define CMD_GET_INFO_OPCODE                        0
#define CMD_CREATE_SCANNER_OPCODE                  1
#define CMD_REMOVE_SCANNER_OPCODE                  2
#define CMD_CREATE_CONNECTION_CHANNEL_OPCODE       3
#define CMD_REMOVE_CONNECTION_CHANNEL_OPCODE       4
#define CMD_FORCE_DISCONNECT_OPCODE                5
#define CMD_CHANGE_MODE_PARAMETERS_OPCODE          6
#define CMD_PING_OPCODE                            7
#define CMD_GET_BUTTON_INFO_OPCODE                 8
#define CMD_CREATE_SCAN_WIZARD_OPCODE              9
#define CMD_CANCEL_SCAN_WIZARD_OPCODE             10
#define CMD_DELETE_BUTTON_OPCODE                  11
#define CMD_CREATE_BATTERY_STATUS_LISTENER_OPCODE 12
#define CMD_REMOVE_BATTERY_STATUS_LISTENER_OPCODE 13

static const char* FLICD_CMDS[]={"CMD_GET_INFO_OPCODE",
                                 "CMD_CREATE_SCANNER_OPCODE",
                                 "CMD_REMOVE_SCANNER_OPCODE",
                                 "CMD_CREATE_CONNECTION_CHANNEL_OPCODE",
                                 "CMD_REMOVE_CONNECTION_CHANNEL_OPCODE",
                                 "CMD_FORCE_DISCONNECT_OPCODE",
                                 "CMD_CHANGE_MODE_PARAMETERS_OPCODE",
                                 "CMD_PING_OPCODE",
                                 "CMD_GET_BUTTON_INFO_OPCODE",
                                 "CMD_CREATE_SCAN_WIZARD_OPCODE",
                                 "CMD_CANCEL_SCAN_WIZARD_OPCODE",
                                 "CMD_DELETE_BUTTON_OPCODE",
                                 "CMD_CREATE_BATTERY_STATUS_LISTENER_OPCODE",
                                 "CMD_REMOVE_BATTERY_STATUS_LISTENER_OPCODE"};

#define EVT_ADVERTISEMENT_PACKET_OPCODE                  0
#define EVT_CREATE_CONNECTION_CHANNEL_RESPONSE_OPCODE    1
#define EVT_CONNECTION_STATUS_CHANGED_OPCODE             2
#define EVT_CONNECTION_CHANNEL_REMOVED_OPCODE            3
#define EVT_BUTTON_UP_OR_DOWN_OPCODE                     4
#define EVT_BUTTON_CLICK_OR_HOLD_OPCODE                  5
#define EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE         6
#define EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OR_HOLD_OPCODE 7
#define EVT_NEW_VERIFIED_BUTTON_OPCODE                   8
#define EVT_GET_INFO_RESPONSE_OPCODE                     9
#define EVT_NO_SPACE_FOR_NEW_CONNECTION_OPCODE          10
#define EVT_GOT_SPACE_FOR_NEW_CONNECTION_OPCODE         11
#define EVT_BLUETOOTH_CONTROLLER_STATE_CHANGE_OPCODE    12
#define EVT_PING_RESPONSE_OPCODE                        13
#define EVT_GET_BUTTON_INFO_RESPONSE_OPCODE             14
#define EVT_SCAN_WIZARD_FOUND_PRIVATE_BUTTON_OPCODE     15
#define EVT_SCAN_WIZARD_FOUND_PUBLIC_BUTTON_OPCODE      16
#define EVT_SCAN_WIZARD_BUTTON_CONNECTED_OPCODE         17
#define EVT_SCAN_WIZARD_COMPLETED_OPCODE                18
#define EVT_BUTTON_DELETED_OPCODE                       19
#define EVT_BATTERY_STATUS_OPCODE                       20

static const char* FLICD_EVTS[]={"EVT_ADVERTISEMENT_PACKET_OPCODE",
                                 "EVT_CREATE_CONNECTION_CHANNEL_RESPONSE_OPCODE",
                                 "EVT_CONNECTION_STATUS_CHANGED_OPCODE",
                                 "EVT_CONNECTION_CHANNEL_REMOVED_OPCODE",
                                 "EVT_BUTTON_UP_OR_DOWN_OPCODE",
                                 "EVT_BUTTON_CLICK_OR_HOLD_OPCODE",
                                 "EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE",
                                 "EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OR_HOLD_OPCODE",
                                 "EVT_NEW_VERIFIED_BUTTON_OPCODE",
                                 "EVT_GET_INFO_RESPONSE_OPCODE",
                                 "EVT_NO_SPACE_FOR_NEW_CONNECTION_OPCODE",
                                 "EVT_GOT_SPACE_FOR_NEW_CONNECTION_OPCODE",
                                 "EVT_BLUETOOTH_CONTROLLER_STATE_CHANGE_OPCODE",
                                 "EVT_PING_RESPONSE_OPCODE",
                                 "EVT_GET_BUTTON_INFO_RESPONSE_OPCODE",
                                 "EVT_SCAN_WIZARD_FOUND_PRIVATE_BUTTON_OPCODE",
                                 "EVT_SCAN_WIZARD_FOUND_PUBLIC_BUTTON_OPCODE",
                                 "EVT_SCAN_WIZARD_BUTTON_CONNECTED_OPCODE",
                                 "EVT_SCAN_WIZARD_COMPLETED_OPCODE",
                                 "EVT_BUTTON_DELETED_OPCODE",
                                 "EVT_BATTERY_STATUS_OPCODE"};


/// Enums

enum CreateConnectionChannelError PACKED_ENUM_PRE {
	NoError,
	MaxPendingConnectionsReached
} PACKED;

enum ConnectionStatus PACKED_ENUM_PRE {
	Disconnected,
	Connected,
	Ready
} PACKED;

enum DisconnectReason PACKED_ENUM_PRE {
	Unspecified,
	ConnectionEstablishmentFailed,
	TimedOut,
	BondingKeysMismatch
} PACKED;

enum RemovedReason PACKED_ENUM_PRE {
	RemovedByThisClient,
	ForceDisconnectedByThisClient,
	ForceDisconnectedByOtherClient,
	
	ButtonIsPrivate,
	VerifyTimeout,
	InternetBackendError,
	InvalidData,
	
	CouldntLoadDevice,
	
	DeletedByThisClient,
	DeletedByOtherClient,
	ButtonBelongsToOtherPartner,
	DeletedFromButton
} PACKED;

enum ClickType PACKED_ENUM_PRE {
	ButtonDown,
	ButtonUp,
	ButtonClick,
	ButtonSingleClick,
	ButtonDoubleClick,
	ButtonHold
} PACKED;

enum BdAddrType PACKED_ENUM_PRE {
	PublicBdAddrType,
	RandomBdAddrType
} PACKED;

enum LatencyMode PACKED_ENUM_PRE {
	NormalLatency,
	LowLatency,
	HighLatency
} PACKED;

enum ScanWizardResult PACKED_ENUM_PRE {
	WizardSuccess,
	WizardCancelledByUser,
	WizardFailedTimeout,
	WizardButtonIsPrivate,
	WizardBluetoothUnavailable,
	WizardInternetBackendError,
	WizardInvalidData,
	WizardButtonBelongsToOtherPartner,
	WizardButtonAlreadyConnectedToOtherDevice
} PACKED;

enum BluetoothControllerState PACKED_ENUM_PRE {
	Detached,
	Resetting,
	Attached
} PACKED;

/// Commands

#ifdef _MSC_VER
#pragma pack(push,1)
#endif

typedef struct {
	uint8_t opcode;
} PACKED CmdGetInfo;

typedef struct {
	uint8_t opcode;
	uint32_t scan_id;
} PACKED CmdCreateScanner;

typedef struct {
	uint8_t opcode;
	uint32_t scan_id;
} PACKED CmdRemoveScanner;

typedef struct {
	uint8_t opcode;
	uint32_t conn_id;
	uint8_t bd_addr[6];
	enum LatencyMode latency_mode;
	int16_t auto_disconnect_time;
} PACKED CmdCreateConnectionChannel;

typedef struct {
	uint8_t opcode;
	uint32_t conn_id;
} PACKED CmdRemoveConnectionChannel;

typedef struct {
	uint8_t opcode;
	uint8_t bd_addr[6];
} PACKED CmdForceDisconnect;

typedef struct {
	uint8_t opcode;
	uint32_t conn_id;
	enum LatencyMode latency_mode;
	int16_t auto_disconnect_time;
} PACKED CmdChangeModeParameters;

typedef struct {
	uint8_t opcode;
	uint32_t ping_id;
} PACKED CmdPing;

typedef struct {
	uint8_t opcode;
	uint8_t bd_addr[6];
} PACKED CmdGetButtonInfo;

typedef struct {
	uint8_t opcode;
	uint32_t scan_wizard_id;
} PACKED CmdCreateScanWizard;

typedef struct {
	uint8_t opcode;
	uint32_t scan_wizard_id;
} PACKED CmdCancelScanWizard;

typedef struct {
	uint8_t opcode;
	uint8_t bd_addr[6];
} PACKED CmdDeleteButton;

typedef struct {
	uint8_t opcode;
	uint32_t listener_id;
	uint8_t bd_addr[6];
} PACKED CmdCreateBatteryStatusListener;

typedef struct {
	uint8_t opcode;
	uint32_t listener_id;
} PACKED CmdRemoveBatteryStatusListener;

/// Events

typedef struct {
	uint8_t opcode;
	uint32_t scan_id;
	uint8_t bd_addr[6];
	uint8_t name_length;
	char name[16];
	int8_t rssi;
	int8_t is_private;
	int8_t already_verified;
	int8_t already_connected_to_this_device;
	int8_t already_connected_to_other_device;
} PACKED EvtAdvertisementPacket;

typedef struct {
	uint8_t opcode;
	uint32_t conn_id;
} PACKED ConnectionEventBase;

typedef struct {
	ConnectionEventBase base;
	enum CreateConnectionChannelError error;
	enum ConnectionStatus connection_status;
} PACKED EvtCreateConnectionChannelResponse;

typedef struct {
	ConnectionEventBase base;
	enum ConnectionStatus connection_status;
	enum DisconnectReason disconnect_reason;
} PACKED EvtConnectionStatusChanged;

typedef struct {
	ConnectionEventBase base;
	enum RemovedReason removed_reason;
} PACKED EvtConnectionChannelRemoved;

typedef struct {
	ConnectionEventBase base;
	enum ClickType click_type;
	uint8_t was_queued;
	uint32_t time_diff;
} PACKED EvtButtonEvent;

typedef struct {
	uint8_t opcode;
	uint8_t bd_addr[6];
} PACKED EvtNewVerifiedButton;

typedef struct {
	uint8_t opcode;
	//uint8_t BluetoothControllerState bluetooth_controller_state;
	enum BluetoothControllerState bluetooth_controller_state;
	uint8_t my_bd_addr[6];
	//uint8_t BdAddrType my_bd_addr_type;
	enum BdAddrType my_bd_addr_type;
	uint8_t max_pending_connections;
	int16_t max_concurrently_connected_buttons;
	uint8_t current_pending_connections;
	uint8_t currently_no_space_for_new_connection;
	uint16_t nb_verified_buttons;
	uint8_t bd_addr_of_verified_buttons[0][6];
} PACKED EvtGetInfoResponse;

typedef struct {
	uint8_t opcode;
	uint8_t max_concurrently_connected_buttons;
} PACKED EvtNoSpaceForNewConnection;

typedef struct {
	uint8_t opcode;
	uint8_t max_concurrently_connected_buttons;
} PACKED EvtGotSpaceForNewConnection;

typedef struct {
	uint8_t opcode;
	enum BluetoothControllerState state;
} PACKED EvtBluetoothControllerStateChange;

typedef struct {
	uint8_t opcode;
	uint32_t ping_id;
} PACKED EvtPingResponse;

typedef struct {
	uint8_t opcode;
	uint8_t bd_addr[6];
	uint8_t uuid[16];
	uint8_t color_length;
	char color[16];
	uint8_t serial_number_length;
	char serial_number[16];
	uint8_t flic_version;
	uint32_t firmware_version;
} PACKED EvtGetButtonInfoResponse;

typedef struct {
	uint8_t opcode;
	uint32_t scan_wizard_id;
} PACKED EvtScanWizardBase;

typedef struct {
	EvtScanWizardBase base;
} PACKED EvtScanWizardFoundPrivateButton;

typedef struct {
	EvtScanWizardBase base;
	uint8_t bd_addr[6];
	uint8_t name_length;
	char name[16];
} PACKED EvtScanWizardFoundPublicButton;

typedef struct {
	EvtScanWizardBase base;
} PACKED EvtScanWizardButtonConnected;

typedef struct {
	EvtScanWizardBase base;
	enum ScanWizardResult result;
} PACKED EvtScanWizardCompleted;

typedef struct {
	uint8_t opcode;
	uint8_t bd_addr[6];
	uint8_t deleted_by_this_client;
} PACKED EvtButtonDeleted;

typedef struct {
	uint8_t opcode;
	uint32_t listener_id;
	int8_t battery_percentage;
	int64_t timestamp;
} PACKED EvtBatteryStatus;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif
